/*
 * stats.cpp - Statistics mode: 2-D "no 3D scene" emergent-pattern observatory.
 *
 * Runs the single-scenario dynamics on its own worker thread and converts
 * every completed light frame into numeric readouts and live graphs of the
 * emergent patterns that the 3-D mode draws around its scene: the radial
 * shell profile (|u| envelope, signed trigger, sieve-gate hits, pulse radius)
 * and the per-frame active / pB / sB census, together with the lattice-wide
 * convolution diagnostics (calls, s2B passes, pairs, collapse/adiah/repel).
 * Pure HUD: no 3-D scene, no voxel buffer, no campaign text.
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Renderer2D.h"
#include "globals.h"
#include "model/simulation.h"
#include "sinc_overlay.h"
#include "stats.h"
#include "text_renderer.h"

extern TextRenderer* textRenderer;

namespace stats {

// ============================================================================
// Thread state
// ============================================================================

std::atomic<bool> simulationRunning{false};
std::atomic<bool> pauseSimulation{false};
std::atomic<bool> stopSimulation{false};
std::thread        simulationThread;
std::chrono::steady_clock::time_point tbegin;

std::atomic<bool> resetRequested{false};

std::atomic<unsigned long long> sTicks{0};
std::atomic<unsigned long long> sFrames{0};
std::atomic<double>             sElapsedSec{0.0};
std::atomic<double>             sFrameRate{0.0};
std::atomic<unsigned>           sSelW{0};

std::atomic<long long> sActive{0};
std::atomic<long long> sPB{0};
std::atomic<long long> sSB{0};
std::atomic<long long> sS2bCells{0};
std::atomic<long long> sPulseR{0};
std::atomic<double>    sMeanAbsU{0.0};

std::atomic<long long> sCalls{0};
std::atomic<long long> sS2b{0};
std::atomic<long long> sPairs{0};
std::atomic<long long> sSelf{0};
std::atomic<long long> sCollapse{0};
std::atomic<long long> sAdiah{0};
std::atomic<long long> sRepel{0};

// Per-frame history for the time-series graph.
struct Series {
  std::vector<float> active;
  std::vector<float> pb;
  std::vector<float> sb;
  std::vector<float> meanU;
};
Series gSeries;
std::mutex seriesMutex;
const size_t SERIES_CAPACITY = 900;

// Console (bottom strip).
std::vector<std::string> consoleLines;
std::mutex consoleMutex;
const int MAX_CONSOLE_LINES = 60;

// ----------------------------------------------------------------------------
// Metric-card help popups.
// All of this state is touched only by the main thread (both rendering and
// the GLFW callbacks run there), so plain variables are safe.
// ----------------------------------------------------------------------------
static GLFWwindow* sWindow = nullptr;
static int sHelpCard = -1;     // card whose popup is open (-1 = none)
static int sHoverCard = -1;    // card whose '?' icon is hovered (-1 = none)
struct RectF { float x0, y0, x1, y1; };
struct IconGeo { float cx = 0.0f, cy = 0.0f; };
static RectF   sPopupRect{ 0.0f, 0.0f, 0.0f, 0.0f };
static bool    sPopupRectValid = false;
static IconGeo sCardIcons[6];
static IconGeo sPanelIcons[2];   // [0] radial profile graph, [1] timeline graph
static constexpr float kIconRadius = 10.0f;

static TextRenderer* rendererPtr = nullptr;

void stop();
bool start(GLFWwindow* window, TextRenderer* sharedRenderer);

// ============================================================================
// Console logging
// ============================================================================

void consolePrintf(const char* format, ...) {
  char buffer[512];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  {
    std::lock_guard<std::mutex> lock(consoleMutex);
    consoleLines.push_back(std::string(buffer));
    if (consoleLines.size() > (size_t)MAX_CONSOLE_LINES) {
      consoleLines.erase(consoleLines.begin());
    }
  }
  std::cout << buffer << std::flush;
}
// ============================================================================
// Frame census (worker thread, reads lattice_curr only)
// ============================================================================

static void scanLayer(unsigned w, long long& active, long long& pb, long long& sb,
                      long long& s2bc, double& meanAbsU) {
  active = pb = sb = s2bc = 0;
  meanAbsU = 0.0;

  const unsigned EL = automaton::EL;
  if (automaton::lattice_curr.empty() || w >= automaton::W_USED || EL == 0) return;

  long long sumAbs = 0;
  for (unsigned x = 0; x < EL; ++x)
    for (unsigned y = 0; y < EL; ++y)
      for (unsigned z = 0; z < EL; ++z) {
        const automaton::Cell& c =
            automaton::getCell(automaton::lattice_curr, x, y, z, w);
        if (c.active) { ++active; sumAbs += std::abs(c.u); }
        if (c.pB) ++pb;
        if (c.sB) ++sb;
        if (c.s2B) ++s2bc;
      }

  meanAbsU = active ? (double)sumAbs / (double)active : 0.0;
}
// ============================================================================
// Simulation worker
// ============================================================================

void SimulationLoop() {
  consolePrintf("Statistics observatory thread launched.\n");

  // Initialise the single scenario.
  int step = 0;
  while (automaton::initSimulation(step++)) {}
  automaton::swap_lattices();

  // Baseline for the lattice-wide convolution counters of this run.
  long long bCalls    = automaton::enc_calls;
  long long bS2b      = automaton::enc_s2b;
  long long bPair     = automaton::enc_pair;
  long long bSelf     = automaton::enc_self;
  long long bCollapse = automaton::enc_collapse;
  long long bAdiah    = automaton::enc_adiah;
  long long bRepel    = automaton::enc_repel;

  auto runStart = std::chrono::steady_clock::now();
  tbegin = runStart;
  unsigned long long ticks = 0, frames = 0;
  auto lastStore = runStart;

  sTicks.store(0); sFrames.store(0);
  sElapsedSec.store(0.0); sFrameRate.store(0.0);
  sSelW.store(0);
  {
    std::lock_guard<std::mutex> lock(seriesMutex);
    gSeries = Series();
  }

  simulationRunning = true;
  consolePrintf("Observatory running (L=%u W=%u S=%d).\n",
                automaton::EL, automaton::W_USED, automaton::s2b_target);

  while (!stopSimulation) {
    if (pauseSimulation) {
      std::this_thread::sleep_for(std::chrono::milliseconds(80));
      continue;
    }

    const bool newFrame = automaton::simulation();
    ++ticks;
    sTicks.store(ticks);

    // Lattice-wide convolution snapshots, refreshed every tick.
    sCalls.store(automaton::enc_calls - bCalls);
    sS2b.store(automaton::enc_s2b - bS2b);
    sPairs.store(automaton::enc_pair - bPair);
    sSelf.store(automaton::enc_self - bSelf);
    sCollapse.store(automaton::enc_collapse - bCollapse);
    sAdiah.store(automaton::enc_adiah - bAdiah);
    sRepel.store(automaton::enc_repel - bRepel);

    if (resetRequested.exchange(false)) {
      bCalls    = automaton::enc_calls;
      bS2b      = automaton::enc_s2b;
      bPair     = automaton::enc_pair;
      bSelf     = automaton::enc_self;
      bCollapse = automaton::enc_collapse;
      bAdiah    = automaton::enc_adiah;
      bRepel    = automaton::enc_repel;
      ticks = 0; frames = 0;
      runStart = std::chrono::steady_clock::now();
      tbegin = runStart;
      sTicks.store(0); sFrames.store(0);
      sElapsedSec.store(0.0); sFrameRate.store(0.0);
      {
        std::lock_guard<std::mutex> lock(seriesMutex);
        gSeries = Series();
      }
      consolePrintf("Run reset (counters re-baselined).\n");
    }

    if (newFrame) {
      ++frames;
      sFrames.store(frames);

      unsigned selW = sSelW.load();
      if (selW >= automaton::W_USED) { selW = 0; sSelW.store(0); }

      long long active = 0, pb = 0, sb = 0, s2bc = 0;
      double meanAbsU = 0.0;
      scanLayer(selW, active, pb, sb, s2bc, meanAbsU);
      sActive.store(active);
      sPB.store(pb);
      sSB.store(sb);
      sS2bCells.store(s2bc);
      sMeanAbsU.store(meanAbsU);

      // Radial shell profile for the selected layer: same read-out that the
      // 3-D mode shows next to its scene, sampled here per light frame.
      sinc_overlay::update(selW);
      sPulseR.store((long long)sinc_overlay::currentRadius());

      {
        std::lock_guard<std::mutex> lock(seriesMutex);
        gSeries.active.push_back((float)active);
        gSeries.pb.push_back((float)pb);
        gSeries.sb.push_back((float)sb);
        gSeries.meanU.push_back((float)meanAbsU);
        if (gSeries.active.size() > SERIES_CAPACITY) {
          gSeries.active.erase(gSeries.active.begin());
          gSeries.pb.erase(gSeries.pb.begin());
          gSeries.sb.erase(gSeries.sb.begin());
          gSeries.meanU.erase(gSeries.meanU.begin());
        }
      }

      auto now = std::chrono::steady_clock::now();
      double sec = std::chrono::duration<double>(now - runStart).count();
      sElapsedSec.store(sec);
      sFrameRate.store(sec > 0.001 ? (double)frames / sec : 0.0);
    }

    auto now = std::chrono::steady_clock::now();
    if (now - lastStore > std::chrono::milliseconds(200)) {
      sElapsedSec.store(std::chrono::duration<double>(now - runStart).count());
      lastStore = now;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  simulationRunning = false;
  consolePrintf("Statistics observatory thread ended.\n");
}

// ============================================================================
// Minimal 2-D render helpers (bottom-left origin, matches TextRenderer)
// ============================================================================

namespace {

struct Canvas { int w = 0; int h = 0; glm::mat4 ortho{1.0f}; };
Canvas gCanvas;

void set2D() {
  Renderer2D::use();
  Renderer2D::setMVP(gCanvas.ortho);
}

GLuint sVao = 0, sVbo = 0;
void ensureVAO() {
  if (sVao) return;
  glGenVertexArrays(1, &sVao);
  glGenBuffers(1, &sVbo);
  glBindVertexArray(sVao);
  glBindBuffer(GL_ARRAY_BUFFER, sVbo);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (void*)0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void drawColorVec(const std::vector<float>& v, GLenum mode,
                  const glm::vec3& color, float lw = 0.0f) {
  if (v.empty()) return;
  ensureVAO();
  // IMPORTANT: bind the Renderer2D program BEFORE setting the color uniform.
  // RenderText leaves glUseProgram(0), so calling Renderer2D::setColor first
  // would silently drop the uniform and leave stale colours on screen.
  set2D();
  Renderer2D::setColor(color);
  glBindVertexArray(sVao);
  glBindBuffer(GL_ARRAY_BUFFER, sVbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size() * sizeof(float)),
               v.data(), GL_STREAM_DRAW);
  if (lw > 0.0f) glLineWidth(lw);
  glDrawArrays(mode, 0, (GLsizei)(v.size() / 2));
  glLineWidth(1.0f);
  glBindVertexArray(0);
}

void fillRect(float x0, float y0, float x1, float y1, const glm::vec3& c) {
  std::vector<float> v = { x0, y0, x1, y0, x0, y1, x1, y1 };
  drawColorVec(v, GL_TRIANGLE_STRIP, c);
}

void borderRect(float x0, float y0, float x1, float y1,
                const glm::vec3& c, float thick = 1.25f) {
  std::vector<float> v = { x0, y0, x1, y0, x1, y1, x0, y1, x0, y0 };
  drawColorVec(v, GL_LINE_STRIP, c, thick);
}

void drawPanel(float x0, float y0, float x1, float y1,
               const glm::vec3& bg, const glm::vec3& border) {
  fillRect(x0, y0, x1, y1, bg);
  borderRect(x0, y0, x1, y1, border);
}

void polyLine(const std::vector<float>& pts, const glm::vec3& c, float thick) {
  if (pts.size() < 4) return;
  drawColorVec(pts, GL_LINE_STRIP, c, thick);
}

void fillCircle(float cx, float cy, float radius, const glm::vec3& color,
                int seg = 20) {
  if (radius <= 0.0f) return;
  std::vector<float> v;
  v.reserve((size_t)(seg + 2) * 2);
  v.push_back(cx); v.push_back(cy);
  for (int i = 0; i <= seg; ++i) {
    float a = 6.2831853f * (float)i / (float)seg;
    v.push_back(cx + radius * std::cos(a));
    v.push_back(cy + radius * std::sin(a));
  }
  drawColorVec(v, GL_TRIANGLE_FAN, color);
}

void circleOutline(float cx, float cy, float radius, const glm::vec3& color,
                   float thick = 1.5f, int seg = 24) {
  if (radius <= 0.0f) return;
  std::vector<float> v;
  v.reserve((size_t)(seg + 1) * 2);
  for (int i = 0; i <= seg; ++i) {
    float a = 6.2831853f * (float)i / (float)seg;
    v.push_back(cx + radius * std::cos(a));
    v.push_back(cy + radius * std::sin(a));
  }
  drawColorVec(v, GL_LINE_LOOP, color, thick);
}

// Palette (brightened for contrast on the dark dashboard).
const glm::vec3 cBg     (0.030f, 0.038f, 0.056f);
const glm::vec3 cPanel  (0.062f, 0.076f, 0.110f);
const glm::vec3 cPlot   (0.042f, 0.052f, 0.080f);   // inner graph/plot area
const glm::vec3 cBorder (0.40f,  0.48f,  0.64f);
const glm::vec3 cGrid   (0.28f,  0.34f,  0.47f);
const glm::vec3 cText   (0.96f,  0.97f,  1.00f);
const glm::vec3 cDim    (0.78f,  0.84f,  0.93f);   // secondary text (labels)
const glm::vec3 cMuted  (0.58f,  0.66f,  0.80f);   // tertiary text
const glm::vec3 cYellow (1.00f,  0.93f,  0.35f);
const glm::vec3 cGreen  (0.35f,  1.00f,  0.55f);
const glm::vec3 cCyan   (0.20f,  0.88f,  1.00f);
const glm::vec3 cPink   (1.00f,  0.60f,  0.88f);
const glm::vec3 cRed    (1.00f,  0.42f,  0.36f);
const glm::vec3 cGrey   (0.82f,  0.84f,  0.88f);   // pulse ruler / neutral

// Font metrics / text layout (text is drawn at a baseline given in
// bottom-left coordinates; ascender extends up from the baseline).
float ascPx()  { return rendererPtr ? rendererPtr->getAscenderPx()  : 44.0f; }
float descPx() { return rendererPtr ? rendererPtr->getDescenderPx() : -11.0f; }
float linePx() { return ascPx() - descPx(); }

void textBase(float x, float yBase, float scale, const glm::vec3& color,
              const std::string& s) {
  rendererPtr->RenderText(s, x, yBase, scale, color, gCanvas.w, gCanvas.h);
}

// Render a line whose ascender top sits at 'yTop'.
void textTop(float x, float yTop, float scale, const glm::vec3& color,
             const std::string& s) {
  textBase(x, yTop - ascPx() * scale, scale, color, s);
}

// Render 'value' right-aligned at xRight, label left at xLeft.
void rowText(float xLabel, float xRight, float yTop, float scale,
             const glm::vec3& labelColor, const glm::vec3& valueColor,
             const std::string& label, const std::string& value) {
  textTop(xLabel, yTop, scale, labelColor, label);
  if (!value.empty()) {
    float vw = rendererPtr->measureTextWidth(value, scale);
    textTop(xRight - vw, yTop, scale, valueColor, value);
  }
}

// Fit text inside [x0, xR]; shrink the scale when it would overflow.
void fitTextTop(float x0, float xR, float yTop, float scale,
                const glm::vec3& color, const std::string& s) {
  if (s.empty()) return;
  float w = rendererPtr->measureTextWidth(s, scale);
  if (w > (xR - x0) && w > 0.0f) {
    scale = std::max(0.24f, scale * (xR - x0) / w);
  }
  textTop(x0, yTop, scale, color, s);
}

// ---------------------------------------------------------------------------
// Layout bands (bottom-left y-up coordinates).  Every panel on the dashboard
// derives its rect from these helpers so spacing stays consistent on resize.
// ---------------------------------------------------------------------------
const float kMargin   = 14.0f;
const float kGap      = 12.0f;
const float kHeaderH  = 74.0f;    // header strip height
const float kCardsGap = 12.0f;
const float kCardsH   = 92.0f;    // metric-card strip height
const float kMidGap   = 14.0f;
const float kConsoleH = 96.0f;    // console strip height
const float kLeftW    = 352.0f;   // left column width

float layoutTop() { return (float)gCanvas.h - 6.0f; }
float headerBot() { return layoutTop() - kHeaderH; }
float cardsTop()  { return headerBot() - kCardsGap; }
float cardsBot()  { return cardsTop() - kCardsH; }
float midTop()    { return cardsBot() - kMidGap; }
float midBot()    { return kConsoleH + 18.0f; }
float leftX0()    { return kMargin; }
float leftX1()    { return kMargin + kLeftW; }
float rightX0()   { return leftX1() + kGap; }
float rightX1()   { return (float)gCanvas.w - kMargin; }

} // namespace

// ============================================================================
// Dashboard layout (render thread)
// ============================================================================

static std::string fmtInt(long long v) {
  char b[48]; snprintf(b, sizeof(b), "%lld", v); return std::string(b);
}
static std::string fmtReal(double v, int prec) {
  char b[48]; snprintf(b, sizeof(b), "%.*f", prec, v); return std::string(b);
}

// ---------------------------------------------------------------------------
// Metric-card help: label, accent colour and popup text (six cards).
// ---------------------------------------------------------------------------
struct CardHelp {
  const char* label;
  glm::vec3   color;
  const char* text;
};
static const CardHelp kCardHelp[6] = {
  { "Light frames", cYellow,
    "One completed light frame of the dynamics: each W-layer advances its "
    "pulsing shell by one lattice cell (radius r -> r+1). Every per-frame "
    "census and every timeline point is sampled at a completed light frame." },
  { "Ticks", cYellow,
    "Internal automaton steps. One tick is one automaton::simulation() call; "
    "several ticks build up a light frame, driving the diffusion, "
    "convolution and relocation phases of the CA." },
  { "Elapsed", cYellow,
    "Wall-clock seconds since this run started (or was reset with R)." },
  { "Active cells", cCyan,
    "Cells currently marked active on the wavefront of the selected W-layer. "
    "It is the size of the pulsating shell at this light frame and grows "
    "with the shell radius." },
  { "Mean |u|", cGreen,
    "Mean of |u| - the radial in-phase amplitude - over the active cells of "
    "the selected layer. A measure of how strongly the shell field has "
    "built up (zero before the wave forms)." },
  { "Frames/s", cPink,
    "Completed light frames per second. It reflects the throughput of the "
    "simulation for the current lattice size (L and W)." },
};

// Help entries for the two graph panels (ids 6 and 7).
static const CardHelp kPanelHelp[2] = {
  { "Radial shell profile", cGreen,
    "Radial structure of the pulsating shell of the selected W-layer, as a "
    "function of radius r.  Green: mean |u| envelope (auto-referenced).  "
    "Cyan: signed u profile around the mid axis.  Red: accumulated sieve-gate "
    "(s2B) hits per shell.  Grey ruler: current pulse radius r(t)." },
  { "Emergent timeline", cPink,
    "Emergent patterns over completed light frames: active wavefront cells "
    "(cyan), pB channel (green), sB channel (pink) and mean |u| (yellow).  "
    "Each series is normalised to its own window maximum so the trends stay "
    "readable together." },
};

static const char* helpLabel(int id) {
  if (id >= 0 && id < 6) return kCardHelp[id].label;
  return kPanelHelp[id - 6].label;
}
static glm::vec3 helpColor(int id) {
  if (id >= 0 && id < 6) return kCardHelp[id].color;
  return kPanelHelp[id - 6].color;
}
static const char* helpText(int id) {
  if (id >= 0 && id < 6) return kCardHelp[id].text;
  return kPanelHelp[id - 6].text;
}

static std::vector<std::string> wrapToWidth(const std::string& text,
                                            float scale, float maxW) {
  std::vector<std::string> out;
  std::istringstream iss(text);
  std::string word;
  std::string line;
  while (iss >> word) {
    std::string test = line.empty() ? word : line + " " + word;
    if (!line.empty() && rendererPtr->measureTextWidth(test, scale) > maxW) {
      out.push_back(line);
      line = word;
    } else {
      line = test;
    }
  }
  if (!line.empty()) out.push_back(line);
  return out;
}

static bool pointInRect(float x, float y, const RectF& r) {
  return x >= r.x0 && x <= r.x1 && y >= r.y0 && y <= r.y1;
}

static RectF computePopupRect(const std::vector<std::string>& lines,
                              float scale) {
  const float pw = std::min(660.0f, (float)gCanvas.w - 60.0f);
  const float lineH = linePx() * scale + 7.0f;
  const float ph = 52.0f + (float)lines.size() * lineH + 34.0f;
  const float cx = (float)gCanvas.w * 0.5f;
  const float cy = (float)gCanvas.h * 0.5f + 40.0f;
  RectF r;
  r.x0 = cx - pw * 0.5f;
  r.x1 = cx + pw * 0.5f;
  r.y0 = cy - ph * 0.5f;
  r.y1 = cy + ph * 0.5f;
  if (r.x0 < 10.0f) { r.x1 += 10.0f - r.x0; r.x0 = 10.0f; }
  if (r.x1 > (float)gCanvas.w - 10.0f) {
    r.x0 -= r.x1 - ((float)gCanvas.w - 10.0f);
    r.x1 = (float)gCanvas.w - 10.0f;
  }
  if (r.y1 > (float)gCanvas.h - 6.0f) {
    const float d = r.y1 - ((float)gCanvas.h - 6.0f);
    r.y1 -= d; r.y0 -= d;
  }
  return r;
}

static void drawHelpPopup() {
  if (sHelpCard < 0 || sHelpCard > 7) {
    sPopupRectValid = false;
    return;
  }

  const char* label = helpLabel(sHelpCard);
  const glm::vec3 accent = helpColor(sHelpCard);
  const std::string text = helpText(sHelpCard);
  const float scale = 0.40f;
  const float pw = std::min(660.0f, (float)gCanvas.w - 60.0f);
  std::vector<std::string> lines =
      wrapToWidth(text, scale, pw - 84.0f);
  if (lines.empty()) {
    sPopupRectValid = false;
    return;
  }

  const RectF r = computePopupRect(lines, scale);
  sPopupRect = r;
  sPopupRectValid = true;

  const float lineH = linePx() * scale + 7.0f;

  drawPanel(r.x0, r.y0, r.x1, r.y1, cPanel, accent);

  const float tScale = 0.5f;
  textTop(r.x0 + 18.0f, r.y1 - 14.0f, tScale, accent, label);

  const std::string tag = "HELP";
  const float tagW = rendererPtr->measureTextWidth(tag, 0.30f);
  textTop(r.x1 - 18.0f - tagW, r.y1 - 13.0f, 0.30f, cMuted, tag);

  const float sepY = r.y1 - 14.0f - linePx() * tScale - 9.0f;
  fillRect(r.x0 + 14.0f, sepY, r.x1 - 14.0f, sepY + 1.0f, cGrid);

  // Body text.
  float y = sepY - 14.0f;
  for (const auto& ln : lines) {
    textTop(r.x0 + 24.0f, y, scale, cText, ln);
    y -= lineH;
  }

  // Footer hint.
  const std::string hint = "click outside or press Esc to close";
  const float hintW = rendererPtr->measureTextWidth(hint, 0.30f);
  textTop(r.x0 + (r.x1 - r.x0) * 0.5f - hintW * 0.5f,
          r.y0 + 14.0f, 0.30f, cMuted, hint);
}

// Draws a small '?' help icon at (cx, cy) with hover/open highlighting.
static void drawHelpIconAt(int index, float cx, float cy) {
  const bool hl = (sHoverCard == index) || (sHelpCard == index);
  fillCircle(cx, cy, kIconRadius,
             hl ? cYellow : glm::vec3(0.36f, 0.45f, 0.60f));
  if (sHelpCard == index) {
    circleOutline(cx, cy, kIconRadius + 2.5f, cYellow, 1.5f);
  }
  const std::string q = "?";
  const float qScale = 0.30f;
  const float qW = rendererPtr->measureTextWidth(q, qScale);
  const float qY = cy + linePx() * qScale * 0.5f - 2.0f;
  textTop(cx - qW * 0.5f, qY, qScale, glm::vec3(0.04f, 0.06f, 0.10f), q);
}

// ---------------------------------------------------------------------------
// Hover + click handling for the help icons.
// ---------------------------------------------------------------------------
static int hitHelpIcon(double xpos, float yBL) {
  const float rHit = kIconRadius + 7.0f;
  for (int i = 0; i < 6; ++i) {
    const float dx = (float)xpos - sCardIcons[i].cx;
    const float dy = yBL - sCardIcons[i].cy;
    if (dx * dx + dy * dy <= rHit * rHit) return i;
  }
  for (int i = 0; i < 2; ++i) {
    const float dx = (float)xpos - sPanelIcons[i].cx;
    const float dy = yBL - sPanelIcons[i].cy;
    if (dx * dx + dy * dy <= rHit * rHit) return 6 + i;
  }
  return -1;
}

static void refreshHover(double xpos, double ypos) {
  int h;
  glfwGetFramebufferSize(sWindow, nullptr, &h);
  if (h <= 0) return;
  sHoverCard = hitHelpIcon(xpos, (float)h - (float)ypos);
}

static void cursorPosCallback(GLFWwindow*, double xpos, double ypos) {
  refreshHover(xpos, ypos);
}

static void mouseButtonCallback(GLFWwindow*, int button, int action, int) {
  if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;

  double xpos, ypos;
  glfwGetCursorPos(sWindow, &xpos, &ypos);
  int w, h;
  glfwGetFramebufferSize(sWindow, &w, &h);
  if (h <= 0) return;
  const float yBL = (float)h - (float)ypos;

  // Clicking inside an open popup keeps it open.
  if (sHelpCard >= 0 && sPopupRectValid &&
      pointInRect((float)xpos, yBL, sPopupRect)) {
    refreshHover(xpos, ypos);
    return;
  }

  // Clicking a '?' icon toggles that popup.
  const int hit = hitHelpIcon(xpos, yBL);
  if (hit >= 0) {
    sHelpCard = (sHelpCard == hit) ? -1 : hit;
    sPopupRectValid = false;
    sHoverCard = hit;
    return;
  }

  // Anywhere else closes the popup.
  sHelpCard = -1;
  sPopupRectValid = false;
  sHoverCard = -1;
}

static void renderHeader() {
  const float x0 = leftX0(), x1 = rightX1();
  const float yTop = layoutTop(), yBot = headerBot();
  drawPanel(x0, yBot, x1, yTop, cPanel, cBorder);

  // Row 1: title (left) + live status (right).
  const float titleTop = yTop - 12.0f;
  const float titleScale = 0.5f;
  textTop(x0 + 14, titleTop, titleScale, cYellow,
          "STATISTICS OBSERVATORY");

  const float titleW = rendererPtr->measureTextWidth(
      "STATISTICS OBSERVATORY", titleScale);
  textTop(x0 + 14.0f + titleW + 14.0f, titleTop, 0.34f, cMuted,
          "emergent-pattern monitor");

  const bool paused = pauseSimulation;
  const std::string status = paused ? "PAUSED" : "RUNNING";
  const glm::vec3 statusCol = paused ? cRed : cGreen;
  const float statusScale = 0.46f;
  const float statusW = rendererPtr->measureTextWidth(status, statusScale);
  const float dotSize = 9.0f;
  const float sRight = x1 - 14.0f;
  fillRect(sRight - statusW - 18.0f - dotSize, titleTop + 4.0f,
           sRight - statusW - 18.0f, titleTop + 4.0f + dotSize, statusCol);
  textTop(sRight - statusW, titleTop - 1.0f, statusScale, statusCol, status);

  // Row 2: controls (left) + geometry (right).
  const float row2Top = titleTop - linePx() * titleScale - 11.0f;

  unsigned nW = automaton::W_USED;
  unsigned wsel = std::min((unsigned)sSelW.load(), nW ? nW - 1u : 0u);
  char geo[128];
  snprintf(geo, sizeof(geo), "L=%u   W=%u   sieve S=%d   layer %u/%u",
           automaton::EL, nW, automaton::s2b_target,
           nW ? wsel + 1u : 0u, nW);
  const float geoScale = 0.38f;
  const float geoW = rendererPtr->measureTextWidth(geo, geoScale);
  textTop(x1 - 14.0f - geoW, row2Top, geoScale, cText, geo);

  const std::string help =
      "Space  pause   |   L  next W-layer   |   R  reset run   |   Esc  exit";
  const float helpScale = 0.34f;
  const float helpW = rendererPtr->measureTextWidth(help, helpScale);
  const float usable = x1 - x0 - 28.0f;
  if (helpW + geoW + 30.0f <= usable) {
    textTop(x0 + 14, row2Top, helpScale, cDim, help);
  }
}

static void renderCards() {
  const int n = 6;
  const float x0 = leftX0(), x1 = rightX1();
  const float gap = 8.0f;
  const float cardW = (x1 - x0 - gap * (n - 1)) / (float)n;
  const float top = cardsTop(), bot = cardsBot();

  struct Metric { const char* label; std::string value; glm::vec3 color; };
  std::vector<Metric> m(n);
  m[0] = { "Light frames", fmtInt((long long)sFrames.load()), cYellow };
  m[1] = { "Ticks",        fmtInt((long long)sTicks.load()), cYellow };
  m[2] = { "Elapsed",      fmtReal(sElapsedSec.load(), 1) + " s", cYellow };
  m[3] = { "Active cells", fmtInt(sActive.load()), cCyan };
  m[4] = { "Mean |u|",     fmtReal(sMeanAbsU.load(), 3), cGreen };
  m[5] = { "Frames/s",     fmtReal(sFrameRate.load(), 2), cPink };

  for (int i = 0; i < n; ++i) {
    const float cx0 = x0 + (float)i * (cardW + gap);
    const float cx1 = cx0 + cardW;
    drawPanel(cx0, bot, cx1, top, cPanel, cBorder);

    textTop(cx0 + 10, top - 13, 0.36f, cText, m[i].label);

    const float valScale = 0.52f;
    const float valTop = bot + 16.0f + linePx() * valScale;
    fitTextTop(cx0 + 10, cx1 - 10, valTop, valScale, m[i].color, m[i].value);

    // '?' help icon in the top-right corner of the card.
    const float icx = cx1 - 18.0f;
    const float icy = top - 18.0f;
    sCardIcons[i].cx = icx;
    sCardIcons[i].cy = icy;
    drawHelpIconAt(i, icx, icy);
  }
}

static void renderLeftPanels() {
  const float x0 = leftX0(), x1 = leftX1();
  const float yBotR = midBot(), yTopR = midTop();
  const float totalH = yTopR - yBotR;
  if (totalH < 140.0f) return;

  const float gapP = 12.0f;
  const float p1H = (totalH - gapP) * 0.55f;
  const float p2H = totalH - gapP - p1H;
  const float s = 0.36f;
  const float rowH = linePx() * s;
  const float pitch = rowH + 8.0f;
  const float lx = x0 + 14.0f;
  const float rx = x1 - 14.0f;

  // ---- Panel A: convolution / sieve ---------------------------------------
  const float aTop = yTopR;
  const float aBot = aTop - p1H;
  drawPanel(x0, aBot, x1, aTop, cPanel, cBorder);

  const float aTitleTop = aTop - 12.0f;
  textTop(x0 + 14, aTitleTop, 0.42f, cText, "ENCOUNTERS / SIEVE");
  // The small subtitle lives on its own line below the title so that it can
  // never overflow the right edge of the panel.
  const float aSubTop = aTitleTop - linePx() * 0.42f - 6.0f;
  textTop(x0 + 14, aSubTop, 0.30f, cMuted, "run deltas, all W layers");
  const float aUnder = aSubTop - linePx() * 0.30f - 8.0f;
  fillRect(x0 + 8, aUnder, x1 - 8, aUnder + 1.0f, cGrid);

  std::vector<std::pair<std::string, std::string>> rowsA;
  rowsA.push_back({ "calls (active passes)", fmtInt(sCalls.load()) });
  rowsA.push_back({ "s2B gate passes",       fmtInt(sS2b.load()) });
  rowsA.push_back({ "pairs / self-guard",
                    fmtInt(sPairs.load()) + " / " + fmtInt(sSelf.load()) });
  rowsA.push_back({ "collapse (force)",      fmtInt(sCollapse.load()) });
  rowsA.push_back({ "adiabatic (adiah)",     fmtInt(sAdiah.load()) });
  rowsA.push_back({ "repel",                 fmtInt(sRepel.load()) });

  long long calls = sCalls.load();
  long long s2b = sS2b.load();
  std::string thr = "--";
  if (calls > 0) {
    double r = (double)s2b / (double)calls;
    if (r > 1e-9) {
      char b[64];
      snprintf(b, sizeof(b), "%.6f  (1/%.1f)", r, 1.0 / r);
      thr = b;
    } else {
      thr = "0";
    }
  }
  rowsA.push_back({ "s2B/calls", thr });

  float yA = aUnder - 15.0f;
  const float rowsBotA = aBot + 10.0f;
  float pitchA = pitch;
  if (rowsA.size() > 1) {
    const float maxP = (yA - rowsBotA - rowH) / (float)(rowsA.size() - 1);
    pitchA = std::min(pitch, std::max(12.0f, maxP));
  }
  for (size_t i = 0; i < rowsA.size(); ++i) {
    const std::string& lab = rowsA[i].first;
    glm::vec3 vc = cYellow;
    if (lab == "pairs / self-guard") vc = cPink;
    else if (lab == "s2B/calls")     vc = cCyan;
    rowText(lx, rx, yA, s, cDim, vc, lab, rowsA[i].second);
    yA -= pitchA;
  }

  // ---- Panel B: selected W-layer census ------------------------------------
  const float bTop = aBot - gapP;
  const float bBot = bTop - p2H;
  unsigned nW = automaton::W_USED;
  unsigned wsel = std::min((unsigned)sSelW.load(), nW ? nW - 1u : 0u);
  drawPanel(x0, bBot, x1, bTop, cPanel, cBorder);

  const float bTitleTop = bTop - 12.0f;
  char tB[96];
  snprintf(tB, sizeof(tB), "W-LAYER %u/%u CENSUS",
           wsel + (nW ? 1u : 0u), nW);
  textTop(x0 + 14, bTitleTop, 0.42f, cText, tB);
  const float bSubTop = bTitleTop - linePx() * 0.42f - 6.0f;
  const std::string subB = "frame " + fmtInt((long long)sFrames.load());
  textTop(x0 + 14, bSubTop, 0.30f, cMuted, subB);
  const float bUnder = bSubTop - linePx() * 0.30f - 8.0f;
  fillRect(x0 + 8, bUnder, x1 - 8, bUnder + 1.0f, cGrid);

  std::vector<std::pair<std::string, std::string>> rowsB;
  rowsB.push_back({ "pulse radius",   fmtInt(sPulseR.load()) });
  rowsB.push_back({ "active cells",   fmtInt(sActive.load()) });
  rowsB.push_back({ "pB cells",       fmtInt(sPB.load()) });
  rowsB.push_back({ "sB cells",       fmtInt(sSB.load()) });
  rowsB.push_back({ "s2B cells",      fmtInt(sS2bCells.load()) });
  rowsB.push_back({ "mean |u| (active)", fmtReal(sMeanAbsU.load(), 3) });

  float yB = bUnder - 15.0f;
  const float rowsBotB = bBot + 10.0f;
  float pitchB = pitch;
  if (rowsB.size() > 1) {
    const float maxP = (yB - rowsBotB - rowH) / (float)(rowsB.size() - 1);
    pitchB = std::min(pitch, std::max(12.0f, maxP));
  }
  for (size_t i = 0; i < rowsB.size(); ++i) {
    const std::string& lab = rowsB[i].first;
    glm::vec3 vc = cYellow;
    if (lab == "pulse radius")        vc = cCyan;
    else if (lab == "pB cells")       vc = cGreen;
    else if (lab == "sB cells")       vc = cPink;
    else if (lab == "mean |u| (active)") vc = cGreen;
    rowText(lx, rx, yB, s, cDim, vc, lab, rowsB[i].second);
    yB -= pitchB;
  }
}

static void drawRadialGraph(float x0, float yB, float x1, float yT) {
  drawPanel(x0, yB, x1, yT, cPanel, cBorder);

  unsigned nW = automaton::W_USED;
  unsigned wsel = std::min((unsigned)sSelW.load(), nW ? nW - 1u : 0u);
  char title[128];
  snprintf(title, sizeof(title),
           "RADIAL SHELL PROFILE   (layer %u/%u, frame %llu)",
           wsel + (nW ? 1u : 0u), nW,
           (unsigned long long)sFrames.load());
  const float titleTop = yT - 12.0f;
  fitTextTop(x0 + 12, x1 - 52, titleTop, 0.40f, cText, title);

  // '?' help icon in the top-right corner of this graph panel.
  sPanelIcons[0].cx = x1 - 22.0f;
  sPanelIcons[0].cy = yT - 16.0f;
  drawHelpIconAt(6, sPanelIcons[0].cx, sPanelIcons[0].cy);

  const unsigned gsAll = sinc_overlay::graphSize();
  if (!sinc_overlay::ready() || gsAll < 2) {
    textTop(x0 + 14, titleTop - 40.0f, 0.34f, cDim,
            "waiting for a completed light frame...");
    return;
  }

  const auto& prof = sinc_overlay::profile();
  const auto& trig = sinc_overlay::triggerRate();
  const auto& am   = sinc_overlay::andMask();
  const unsigned g =
      std::min(gsAll, (unsigned)std::min(prof.size(),
                std::min(trig.size(), am.size())));
  if (g < 2) return;

  const float titleH = linePx() * 0.40f;
  const float capH = 38.0f;
  const float plY1 = titleTop - titleH - 16.0f;   // plot top
  const float plY0 = yB + capH;                   // plot bottom
  const float plotH = plY1 - plY0;
  if (plotH < 24.0f) return;

  const float mid = plY0 + plotH * 0.5f;
  const float stepX = (x1 - x0 - 24.0f) / (float)(g - 1);
  const float gx0 = x0 + 12.0f, gx1 = x1 - 12.0f;

  // Inner plot background.
  fillRect(gx0, plY0, gx1, plY1, cPlot);

  // Vertical guides at every quarter of the radius axis.
  for (int q = 1; q <= 3; ++q) {
    float px = gx0 + (float)(g - 1) * (float)q * 0.25f * stepX;
    fillRect(px, plY0, px + 0.5f, plY1, cGrid);
  }
  // Horizontal guides (0, mid, 1).
  fillRect(gx0, plY0, gx1, plY0 + 1.0f, cGrid);
  fillRect(gx0, mid, gx1, mid + 1.0f, cGrid);
  fillRect(gx0, plY1 - 1.0f, gx1, plY1, cGrid);

  // Red: gate-hit scatter per shell radius.
  for (unsigned r = 0; r < g; ++r) {
    if (am[r] <= 0.001f) continue;
    float px = gx0 + (float)r * stepX;
    float py = plY0 + std::min(am[r], 1.0f) * plotH;
    fillRect(px - 2.0f, py - 2.0f, px + 2.0f, py + 2.0f, cRed);
  }

  // Cyan: signed trigger rate, centred on the middle guide.
  std::vector<float> pv;
  for (unsigned r = 0; r < g; ++r) {
    float px = gx0 + (float)r * stepX;
    float v = std::max(-1.0f, std::min(1.0f, trig[r]));
    float py = mid + v * plotH * 0.5f;
    pv.push_back(px); pv.push_back(py);
  }
  polyLine(pv, cCyan, 2.0f);

  // Green: |u| envelope (0..1 against the running reference).
  pv.clear();
  for (unsigned r = 0; r < g; ++r) {
    float px = gx0 + (float)r * stepX;
    float py = plY0 + std::min(prof[r], 1.0f) * plotH;
    pv.push_back(px); pv.push_back(py);
  }
  polyLine(pv, cGreen, 2.2f);

  // Pulse radius ruler.
  const unsigned pr = sinc_overlay::currentRadius();
  if (pr < g) {
    float px = gx0 + (float)pr * stepX;
    std::vector<float> rv = { px, plY0, px, plY1 };
    drawColorVec(rv, GL_LINES, cGrey, 1.5f);
  }

  // Caption with colour chips.
  float cx = x0 + 12.0f;
  const char* caps[4] = { "|u| envelope", "signed u", "gate hits", "pulse r" };
  const glm::vec3 cols[4] = { cGreen, cCyan, cRed, cGrey };
  for (int i = 0; i < 4; ++i) {
    fillRect(cx, yB + 13, cx + 9, yB + 22, cols[i]);
    textTop(cx + 13, yB + 17, 0.30f, cText, caps[i]);
    cx += 13.0f + rendererPtr->measureTextWidth(caps[i], 0.30f) + 18.0f;
  }
  const std::string axis = "radius r ->";
  textTop(gx1 - rendererPtr->measureTextWidth(axis, 0.28f),
          yB + 17, 0.28f, cMuted, axis);
}

static void drawTimelineGraph(float x0, float yB, float x1, float yT) {
  drawPanel(x0, yB, x1, yT, cPanel, cBorder);

  unsigned nW = automaton::W_USED;
  unsigned wsel = std::min((unsigned)sSelW.load(), nW ? nW - 1u : 0u);
  char title[128];
  snprintf(title, sizeof(title),
           "EMERGENT TIMELINE   (layer %u/%u, one point per light frame)",
           wsel + (nW ? 1u : 0u), nW);
  const float titleTop = yT - 12.0f;
  fitTextTop(x0 + 12, x1 - 52, titleTop, 0.40f, cText, title);

  // '?' help icon in the top-right corner of this graph panel.
  sPanelIcons[1].cx = x1 - 22.0f;
  sPanelIcons[1].cy = yT - 16.0f;
  drawHelpIconAt(7, sPanelIcons[1].cx, sPanelIcons[1].cy);

  std::vector<float> active, pb, sb, meanU;
  {
    std::lock_guard<std::mutex> lock(seriesMutex);
    active = gSeries.active;
    pb = gSeries.pb;
    sb = gSeries.sb;
    meanU = gSeries.meanU;
  }
  const size_t n = active.size();
  if (n < 2) {
    textTop(x0 + 14, titleTop - 40.0f, 0.34f, cDim, "collecting frames...");
    return;
  }

  const float titleH = linePx() * 0.40f;
  const float capH = 38.0f;
  const float plY1 = titleTop - titleH - 16.0f;
  const float plY0 = yB + capH;
  const float plotH = plY1 - plY0;
  if (plotH < 24.0f) return;
  const float gx0 = x0 + 12.0f, gx1 = x1 - 12.0f;
  const float stepX = (n > 1) ? (gx1 - gx0) / (float)(n - 1) : 0.0f;

  // Inner plot background + horizontal guides (0, mid, 1).
  fillRect(gx0, plY0, gx1, plY1, cPlot);
  fillRect(gx0, plY0, gx1, plY0 + 1.0f, cGrid);
  fillRect(gx0, plY0 + plotH * 0.5f, gx1, plY0 + plotH * 0.5f + 1.0f, cGrid);
  fillRect(gx0, plY1 - 1.0f, gx1, plY1, cGrid);

  const float* series[4] = { active.data(), pb.data(), sb.data(), meanU.data() };
  const glm::vec3 cols[4] = { cCyan, cGreen, cPink, cYellow };

  for (int k = 0; k < 4; ++k) {
    float mx = 1.0f;
    for (size_t i = 0; i < n; ++i)
      mx = std::max(mx, series[k][i]);
    mx *= 1.08f;
    std::vector<float> pv;
    pv.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
      float px = gx0 + (float)i * stepX;
      float py = plY0 + std::min(1.0f, series[k][i] / mx) * plotH;
      pv.push_back(px); pv.push_back(py);
    }
    polyLine(pv, cols[k], 1.8f);
  }

  // Caption slots with colour chips and the current values.
  char caps[4][40];
  snprintf(caps[0], sizeof(caps[0]), "active %lld", (long long)sActive.load());
  snprintf(caps[1], sizeof(caps[1]), "pB %lld",     (long long)sPB.load());
  snprintf(caps[2], sizeof(caps[2]), "sB %lld",     (long long)sSB.load());
  snprintf(caps[3], sizeof(caps[3]), "mean|u| %s",  fmtReal(sMeanAbsU.load(), 2).c_str());

  const std::string tail = "last " + std::to_string(n) + " frames";
  const float tailW = rendererPtr->measureTextWidth(tail, 0.28f);
  const float usableW = (gx1 - gx0) - tailW - 36.0f;
  const bool showTail = usableW > 260.0f;
  const float slotW = (gx1 - gx0 - (showTail ? tailW + 28.0f : 0.0f)) / 4.0f;

  for (int i = 0; i < 4; ++i) {
    const float csx = gx0 + (float)i * slotW;
    fillRect(csx + 1, yB + 13, csx + 10, yB + 22, cols[i]);
    fitTextTop(csx + 14, csx + slotW - 6, yB + 17, 0.30f, cText, caps[i]);
  }
  if (showTail) {
    textTop(gx1 - tailW, yB + 17, 0.28f, cMuted, tail);
  }
}

static void renderConsole() {
  const float x0 = leftX0(), x1 = rightX1();
  const float y1 = kConsoleH, y0 = 6.0f;
  // Recessed footer: darker panel, neutral text (no terminal-green look).
  drawPanel(x0, y0, x1, y1, cPlot, cBorder);
  textTop(x0 + 14, y1 - 7, 0.28f, cMuted, "LOG");

  std::vector<std::string> lines;
  {
    std::lock_guard<std::mutex> lock(consoleMutex);
    lines = consoleLines;
  }
  if (lines.empty()) return;

  const float s = 0.28f;
  const float rowH = linePx() * s;
  const float titleZone = linePx() * 0.28f + 10.0f;
  const float firstTop = y1 - 7.0f - titleZone;
  const float pitch = rowH + 5.0f;
  size_t maxFit = (size_t)std::max(0.0f,
      (firstTop - (y0 + 8.0f)) / pitch);
  maxFit = std::min(maxFit, lines.size());
  if (maxFit == 0 && !lines.empty()) maxFit = 1;

  const glm::vec3 logCol(0.72f, 0.79f, 0.88f);   // neutral light grey-blue
  for (size_t i = 0; i < maxFit; ++i) {
    textTop(x0 + 16, firstTop - (float)i * pitch, s, logCol,
            lines[lines.size() - 1 - i]);
  }
}

// ============================================================================
// Render frame
// ============================================================================

void display(GLFWwindow* window) {
  int width, height;
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);

  glClearColor(cBg.r, cBg.g, cBg.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  if (!rendererPtr && textRenderer) rendererPtr = textRenderer;
  if (!rendererPtr) return;

  gCanvas.w = width;
  gCanvas.h = height;
  gCanvas.ortho =
      glm::ortho(0.0f, (float)width, 0.0f, (float)height);

  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  renderHeader();
  renderCards();
  renderLeftPanels();

  // Right-hand graph column: timeline below, radial profile above.
  const float gx0 = rightX0(), gx1 = rightX1();
  const float gyB = midBot(), gyT = midTop();
  const float gArea = gyT - gyB;
  if (gArea > 150.0f) {
    const float gapG = 12.0f;
    const float timeH = gArea * 0.42f;
    const float radialH = gArea - timeH - gapG;
    drawTimelineGraph(gx0, gyB, gx1, gyB + timeH);
    drawRadialGraph(gx0, gyB + timeH + gapG, gx1, gyB + timeH + gapG + radialH);
  } else if (gArea > 80.0f) {
    drawRadialGraph(gx0, gyB, gx1, gyT);
  }

  renderConsole();

  drawHelpPopup();

  glEnable(GL_DEPTH_TEST);
}

// ============================================================================
// Input handling
// ============================================================================

void keyCallback(GLFWwindow* window, int key, int, int action, int) {
  if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

  switch (key) {
    case GLFW_KEY_ESCAPE:
      if (sHelpCard >= 0) {
        sHelpCard = -1;
        sPopupRectValid = false;
      } else {
        stop();
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      }
      break;

    case GLFW_KEY_SPACE:
      pauseSimulation = !pauseSimulation;
      consolePrintf(pauseSimulation ? "Paused.\n" : "Resumed.\n");
      break;

    case GLFW_KEY_L: {
      unsigned nW = automaton::W_USED;
      if (nW > 1) {
        unsigned next = (sSelW.load() + 1) % nW;
        sSelW.store(next);
        consolePrintf("W-layer %u/%u selected.\n", next + 1, nW);
      }
      break;
    }

    case GLFW_KEY_R:
      resetRequested.store(true);
      consolePrintf("Reset requested...\n");
      break;
  }
}

// ============================================================================
// Lifecycle management
// ============================================================================

bool isRunning() {
  return simulationThread.joinable() && simulationRunning;
}

bool start(GLFWwindow* window, TextRenderer* sharedRenderer) {
  // Auto-cleanup if already running.
  if (simulationThread.joinable()) {
    consolePrintf("Statistics thread already running - stopping first...\n");
    stop();
  }

  rendererPtr = sharedRenderer ? sharedRenderer : textRenderer;
  if (!rendererPtr) {
    std::cerr << "[Stats] ERROR: No TextRenderer available!\n";
    return false;
  }

  glfwSetKeyCallback(window, keyCallback);
  sWindow = window;
  glfwSetMouseButtonCallback(window, mouseButtonCallback);
  glfwSetCursorPosCallback(window, cursorPosCallback);

  stopSimulation = false;
  pauseSimulation = false;
  resetRequested = false;
  sSelW.store(0);
  sHelpCard = -1;
  sHoverCard = -1;
  sPopupRectValid = false;
  {
    std::lock_guard<std::mutex> lock(seriesMutex);
    gSeries = Series();
  }
  {
    std::lock_guard<std::mutex> lock(consoleMutex);
    consoleLines.clear();
  }

  consolePrintf("Initializing statistics observatory...\n");

  try {
    simulationThread = std::thread(SimulationLoop);
  } catch (const std::exception& e) {
    std::cerr << "[Stats] ERROR: Failed to start thread: " << e.what() << "\n";
    return false;
  }

  return true;
}

void stop() {
  if (!simulationThread.joinable()) return;

  consolePrintf("Stopping statistics observatory...\n");
  stopSimulation = true;

  if (simulationThread.joinable()) {
    simulationThread.join();
  }

  simulationRunning = false;
  consolePrintf("Statistics observatory stopped.\n");
}

void cleanup() {
  stop();
  {
    std::lock_guard<std::mutex> lock(seriesMutex);
    gSeries = Series();
  }
  {
    std::lock_guard<std::mutex> lock(consoleMutex);
    consoleLines.clear();
  }
  rendererPtr = nullptr;
}

} // namespace stats

// ============================================================================
// Global entry points (kept for compatibility with main.cpp)
// ============================================================================

int runStatistics(GLFWwindow* window, TextRenderer* renderer) {
  if (!window || !renderer) {
    std::cerr << "[Stats] ERROR: Invalid window or renderer!\n";
    return -1;
  }

  if (!stats::start(window, renderer)) {
    std::cerr << "[Stats] ERROR: Failed to start statistics mode!\n";
    return -1;
  }
  return 0;
}

void stopStatistics() {
  stats::stop();
}

bool isStatisticsRunning() {
  return stats::isRunning();
}

