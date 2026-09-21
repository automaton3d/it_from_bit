/*
 * splash.cpp - Single-window version
 * Runs inside existing GLFW window passed from main.cpp
 */

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <windows.h>

#include "model/simulation.h"
#include "button.h"
#include "draw_utils.h"
#include "cortina.h"
#include "globals.h"
#include "logo.h"
#include "text_renderer.h"
#include "tickbox.h"
#include "shader.h"
#include "projection_manager.h"
#include "splash.h"
#include "config.h"
#include "Renderer2D.h"
#include "help.h"
#include "subregion_box.h"
#include "subregion_modal.h"

// Declaration of updateProjection from the framework namespace
namespace framework {
    void updateProjection();
}

// External globals from main.cpp
extern TextRenderer* textRenderer;
extern unsigned int textProgram;

// ---------------------------------------------------------------------
// Forward declarations of input callbacks
// ---------------------------------------------------------------------
static int winW();
static int winH();
static void getMousePos(int& mx, int& my);
static int myTopDown(int my_bottom);

void mouseButtonCallback(GLFWwindow*, int, int, int);
void scrollCallback(GLFWwindow*, double, double);
void keyCallback(GLFWwindow*, int, int, int, int);
void passiveMotionCallback(GLFWwindow*, double, double);

// ---------------------------------------------------------------------
// Helpers (2D projection, panels, title)
// ---------------------------------------------------------------------
static const glm::mat4& proj2D() { return ProjectionManager::instance().get2DOrtho(); }
static int winW() { return ProjectionManager::instance().getWidth(); }
static int winH() { return ProjectionManager::instance().getHeight(); }

int Cortina::getSelectedIndex() const
{
    return selectedIndex;
}

namespace splash {

// Which mode the setup screen is starting.
enum class LaunchTarget { Simulation, Statistics, Replay };

// The selection the setup screen is showing right now.  ONE reader, shared by
// the three buttons and by the Enter shortcut: before this existed, Enter
// started a run with splash::lattice_size / splash::numLayers (the values of
// the last *click*) and silently ignored what the dropdowns displayed.
struct Selection
{
    int  L           = 21;     // lattice side (odd)
    int  W           = 10;     // winding layers
    int  scenario    = 0;      // index into scenarioOptions
    bool startPaused = false;  // "Start Paused" tickbox

    // Subregion of the lattice, inclusive lattice indices.  The widget phase of
    // the feature: the numbers are read here, printed, and shown on screen, but
    // the model still allocates the whole L^3 x W lattice (see README).
    glm::ivec3 subLo = glm::ivec3(0, 0, 0);
    glm::ivec3 subHi = glm::ivec3(20, 20, 20);
};

// Grid offered by the lattice-side dropdown: kSizeMin, +2, ... up to kSizeMax.
const int kSizeMin  = 5;
const int kSizeMax  = 89;
const int kSizeStep = 2;

const std::vector<int>& layerValues();

// Pixel geometry of the setup screen, recomputed from the window size.  The
// splash window is created 600x600 but it is resizable, so nothing below
// hard-codes a position: the old code mixed the window *width* into a vertical
// coordinate (`drawDarkPanel(w - 260, w - 290, 220, 80)`) and drifted as soon as
// the window was not square.
struct Layout
{
    int   w = 0, h = 0;
    float titleY   = 0.0f;
    float logoX    = 0.0f, logoY = 0.0f, logoScale = 1.0f, logoH = 0.0f;
    float panelX   = 0.0f, panelY = 0.0f, panelW = 0.0f, panelH = 0.0f;
    float colX     = 0.0f, colW = 0.0f;
    float labelY[3]    = { 0.0f, 0.0f, 0.0f };
    float dropdownY[3] = { 0.0f, 0.0f, 0.0f };
    float dropdownH = 30.0f;
    float presetCaptionY = 0.0f, presetY = 0.0f, presetW = 0.0f;
    float infoX = 0.0f, infoY = 0.0f, infoW = 0.0f, infoH = 0.0f;
    float pausedY = 0.0f, pausedX = 0.0f;
    float buttonX = 0.0f, buttonY[3] = { 0.0f, 0.0f, 0.0f }, buttonW = 200.0f, buttonH = 40.0f;

    // "Run" card: the Start Paused tickbox and the three mode buttons.  They
    // used to live half inside the summary card, which read as a grouping
    // mistake; the three buttons and the tickbox share one box now.
    float runX = 0.0f, runY = 0.0f, runW = 0.0f, runH = 0.0f;

    // Editable subregion of the lattice: a button that opens the 3D overlay
    // (subregion_modal.h) plus the lines that report what it selects.
    float regionLabelY = 0.0f, regionButtonY = 0.0f, regionReadoutY = 0.0f;

    float footerY = 0.0f;
    float helpX = 0.0f, helpY = 0.0f, helpW = 70.0f, helpH = 20.0f;
};

// Values the panel shows for the current selection.  Computed locally, without
// calling automaton::calculateParameters(): that prints one line per layer and
// rebuilds the centre list, which is far too heavy to run on every key press.
struct Preview
{
    unsigned long long cells = 0;
    double bytes = 0.0;
    int    rmax = 0, islandCount = 0, islandSize = 0, frame = 0;
};

// Keyboard focus ring.  Named instead of the bare 0..6 this used to be: the
// subregion cube was inserted in the middle of the ring, so every literal would
// have had to move silently.
const int kFocusSize     = 0;
const int kFocusLayers   = 1;
const int kFocusScenario = 2;
const int kFocusRegion   = 3;
const int kFocusPaused   = 4;
const int kFocusSim      = 5;
const int kFocusStats    = 6;
const int kFocusReplay   = 7;
const int kFocusCount    = 8;

// Defined in the state block below; declared here because the drawing helpers
// live above the namespace and read them.
extern Layout gLayout;
extern int    gFocus;
extern double gPhysicalBytes;
extern framework::Logo* logo_splash;
extern SubRegionBox* subregion;

static void setupUI();
static void applyLayout();
static void computeLayout(int width, int height);
static void printSubregion(const char* reason);
static void syncSubregionLattice();
static bool openRegionOverlay();
static void drawTextTD(const std::string& text, float x, float yCenter,
                       float scale, const glm::vec3& color);
static std::string withThousands(unsigned long long value);
static std::string formatBytes(double bytes);
static Preview previewParameters(int ex, int ey, int ez, int W);
static bool previewMatchesModel(const Preview& p);
// Lattice edges the run will use: the region's extents, or L when the region is
// the whole lattice.
static void effectiveEdges(int L, int& ex, int& ey, int& ez);
static int  sizeFromIndex(int index);
static int  sizeCount();
static int  closestSizeIndex(int L);
static int  closestLayerIndex(int W);
static Selection readSelectionFromUI();
static bool launch(LaunchTarget target);
static void reportStartFailure(const Selection& sel, const std::string& why);

} // namespace splash

// ---------------------------------------------------------------------
// UI drawing utilities
//
// The two panel functions that used to live here (drawDarkPanel /
// drawRaisedPanel) created and deleted a VAO+VBO pair on every call -- twice per
// frame.  They are now drawPanel() below, built from the static-VAO helpers in
// draw_utils (drawQuad2D / drawRectOutline2D).
// ---------------------------------------------------------------------

// ---------------------------------------------------------------------
// Drawing helpers for the setup screen
//
// Two conventions live here at once and they are NOT interchangeable:
//   * Renderer2D / drawQuad2D use ProjectionManager's 2D ortho, whose origin is
//     the TOP-LEFT corner (y grows downwards);
//   * TextRenderer::RenderText builds its own ortho with the origin at the
//     BOTTOM-LEFT, so every text baseline must be mirrored.  drawTextTD() is the
//     only place that conversion happens.
// ---------------------------------------------------------------------
static void fillRectTD(float x, float y, float w, float h, const glm::vec3& color)
{
    drawQuad2D(x, y, x + w, y + h, color, proj2D());
}

static void strokeRectTD(float x, float y, float w, float h, const glm::vec3& color)
{
    // drawRectOutline2D reuses draw_utils' static VAO/VBO: the panels and the
    // focus ring cost nothing but the draw calls.
    drawRectOutline2D(x, y, x + w, y + h, color, 2.0f);
}

// A panel is a filled rectangle plus its border.
static void drawPanel(float x, float y, float w, float h,
                      const glm::vec3& fill, const glm::vec3& border)
{
    fillRectTD(x, y, w, h, fill);
    strokeRectTD(x, y, w, h, border);
}

// x is the left edge, yCenter the *top-down* centreline of the line.
static void drawTextTD(const std::string& text, float x, float yCenter,
                       float scale, const glm::vec3& color)
{
    if (!textRenderer) return;

    const float ascender  = textRenderer->getAscenderPx();
    const float descender = textRenderer->getDescenderPx();
    const float halfLine  = 0.5f * (ascender - descender) * scale;
    const float baselineBU = (winH() - yCenter) + halfLine - 1.0f * scale;

    glUseProgram(textProgram);
    glUniformMatrix4fv(glGetUniformLocation(textProgram, "projection"), 1,
                       GL_FALSE, glm::value_ptr(proj2D()));
    textRenderer->RenderText(text, x, baselineBU, scale, color, winW(), winH());
    glUseProgram(0);
}

static float textWidth(const std::string& text, float scale)
{
    return textRenderer ? textRenderer->measureTextWidth(text, scale) : 0.0f;
}

static void drawTextCenteredTD(const std::string& text, float yCenter, float scale,
                               const glm::vec3& color)
{
    drawTextTD(text, (winW() - textWidth(text, scale)) * 0.5f, yCenter, scale, color);
}

static void drawTitle()
{
    drawTextCenteredTD("It from bit: a concrete attempt",
                       splash::gLayout.titleY, 0.6f,
                       glm::vec3(0.20f, 0.40f, 0.90f));
}

static void drawLogo()
{
    if (!splash::logo_splash || !splash::logo_splash->valid()) return;

    splash::logo_splash->draw((int)splash::gLayout.logoX,
                              (int)splash::gLayout.logoY,
                              splash::gLayout.logoScale);
}

static void drawPanels()
{
    const splash::Layout& lay = splash::gLayout;

    // Three cards, one per group of widgets: parameters (light "paper"), the run
    // summary (dark) and the run card (dark) with "Start Paused" and the three
    // mode buttons.  The tickbox and the Simulation button used to sit half
    // inside the summary card, so the grouping read wrong; the Replay button,
    // on shorter windows, used to fall outside its box.  All three panels come
    // from the static-VAO helpers, so nothing is allocated per frame.
    drawPanel(lay.panelX, lay.panelY, lay.panelW, lay.panelH,
              glm::vec3(0.85f, 0.85f, 0.90f), glm::vec3(0.70f, 0.70f, 0.80f));

    drawPanel(lay.infoX, lay.infoY, lay.infoW, lay.infoH,
              glm::vec3(0.28f, 0.28f, 0.28f), glm::vec3(0.35f, 0.35f, 0.40f));

    drawPanel(lay.runX, lay.runY, lay.runW, lay.runH,
              glm::vec3(0.24f, 0.24f, 0.30f), glm::vec3(0.35f, 0.35f, 0.45f));
}

// Numbers of the subregion editor, under the cube and on the light card.
static void drawSubregionReadout()
{
    if (!textRenderer || !splash::subregion) return;

    const splash::Layout&   lay = splash::gLayout;
    const splash::Selection sel = splash::readSelectionFromUI();

    // The same Summary the overlay shows, so the card and the 3D view cannot
    // disagree about the numbers.
    const SubRegionBox::Summary sum =
        splash::subregion->summarize(sel.W, (unsigned long long)sizeof(automaton::Cell));

    const glm::vec3 strong(0.14f, 0.17f, 0.30f);
    const glm::vec3 dim(0.36f, 0.39f, 0.47f);

    drawTextTD(sum.bounds, lay.colX, lay.regionReadoutY, 0.28f, strong);
    drawTextTD(sum.cells,  lay.colX, lay.regionReadoutY + 17.0f, 0.26f, dim);
}

static void drawFormLabels()
{
    const splash::Layout& lay = splash::gLayout;
    const glm::vec3 label(0.14f, 0.17f, 0.30f);
    const glm::vec3 dim(0.35f, 0.38f, 0.45f);

    drawTextTD("L — lattice side (odd)",           lay.colX, lay.labelY[0], 0.30f, label);
    drawTextTD("W — winding layers",               lay.colX, lay.labelY[1], 0.30f, label);
    drawTextTD("Scenario (single)",                lay.colX, lay.labelY[2], 0.30f, label);
    drawTextTD("presets (L/W) · suggested 21/10",  lay.colX, lay.presetCaptionY, 0.28f, dim);
    drawTextTD("subregion of the lattice (opens a 3D view)",
               lay.colX, lay.regionLabelY, 0.26f, dim);
}

static void drawRunSummary()
{
    if (!textRenderer) return;

    const splash::Layout&   lay = splash::gLayout;
    const splash::Selection sel = splash::readSelectionFromUI();

    // What the run will use: the region's extents when the region is smaller than
    // the lattice, L itself when it is the whole lattice.
    int ex = sel.L, ey = sel.L, ez = sel.L;
    splash::effectiveEdges(sel.L, ex, ey, ez);

    const splash::Preview   pv  = splash::previewParameters(ex, ey, ez, sel.W);

    // White normally; amber when the run is heavy for this machine, red when the
    // start-up guard would refuse it (> 90% of physical memory).
    glm::vec3 ramColor(0.95f, 0.95f, 0.98f);
    if (splash::gPhysicalBytes > 0.0)
    {
        if (pv.bytes > 0.90 * splash::gPhysicalBytes)
            ramColor = glm::vec3(0.95f, 0.30f, 0.30f);
        else if (pv.bytes > 0.25 * splash::gPhysicalBytes)
            ramColor = glm::vec3(0.95f, 0.68f, 0.15f);
    }

    const glm::vec3 white(0.92f, 0.93f, 0.96f);
    const glm::vec3 dim(0.70f, 0.73f, 0.80f);
    const float x = lay.infoX + 12.0f;
    const float step = 19.0f;   // keep in sync with infoLineH in computeLayout()

    float y = lay.infoY + 12.0f + 10.0f;

    drawTextTD("L = " + std::to_string(sel.L) + "    W = " + std::to_string(sel.W),
               x, y, 0.30f, white);
    y += step;

    // The run's own lattice: the region's edges.  With the whole lattice selected
    // this reads exactly as before.
    {
        const bool whole = (ex == sel.L && ey == sel.L && ez == sel.L);
        const std::string dims = std::to_string(ex) + " x " + std::to_string(ey) +
                                 " x " + std::to_string(ez);
        drawTextTD(std::string("lattice = ") + dims + "   x W " + std::to_string(sel.W) +
                   (whole ? "   (= L^3)" : "   (region)"),
                   x, y, 0.30f, white);
        y += step;
        drawTextTD("cells = " + splash::withThousands(pv.cells), x, y, 0.30f, white);
        y += step;
    }

    drawTextTD("lattice RAM = " + splash::formatBytes(pv.bytes), x, y, 0.30f, ramColor);
    y += step;
    drawTextTD("RMAX = " + std::to_string(pv.rmax) +
               "    islands = " + std::to_string(pv.islandCount) +
               " x " + std::to_string(pv.islandSize), x, y, 0.30f, white);
    y += step;
    drawTextTD("light frame = " + splash::withThousands((unsigned long long)pv.frame) + " ticks",
               x, y, 0.30f, white);
    y += step;
    drawTextTD("physical memory = " + splash::formatBytes(splash::gPhysicalBytes),
               x, y, 0.28f, dim);
    y += step;

    // Subregion readout: same numbers the widget prints, so the screen and the
    // log cannot disagree.  With the whole lattice selected this is the same
    // lattice the run would have used anyway, and the model runs exactly that.
    if (splash::subregion)
    {
        const unsigned long long cells    = splash::subregion->cellsPerLayer();
        const unsigned long long l3       = (unsigned long long)sel.L *
                                            (unsigned long long)sel.L *
                                            (unsigned long long)sel.L;
        const unsigned long long region   = cells * (unsigned long long)sel.W;
        const unsigned long long fullCell = l3 * (unsigned long long)sel.W;
        const double             fraction = (fullCell > 0)
            ? (100.0 * (double)region / (double)fullCell) : 100.0;

        const double subBytes  = 3.0 * (double)region *
                                 (double)sizeof(automaton::Cell);
        const double fullBytes = 3.0 * (double)fullCell *
                                 (double)sizeof(automaton::Cell);

        char pct[32];
        std::snprintf(pct, sizeof(pct), "%.1f%%", fraction);

        const glm::vec3 sub(0.55f, 0.85f, 1.00f);
        drawTextTD("subregion = " + splash::subregion->boundsLine(),
                   x, y, 0.28f, sub);
        y += step;
        drawTextTD(std::string("subregion ") + pct + " of the lattice -> cells " +
                   splash::withThousands(region) + " of " + splash::withThousands(fullCell),
                   x, y, 0.28f, sub);
        y += step;
        drawTextTD("run RAM " + splash::formatBytes(subBytes) + " of " +
                   splash::formatBytes(fullBytes) + " (3 lattices)",
                   x, y, 0.28f, sub);
    }
}

static void drawFooter()
{
    const splash::Layout& lay = splash::gLayout;

    // Short enough to leave the Help link (bottom-right) clear.  The subregion
    // keys are listed by the cube itself, next to its numbers.
    drawTextCenteredTD("Enter: start    Tab: move    Left-Right: change    Esc: quit",
                       lay.footerY, 0.28f, glm::vec3(0.33f, 0.36f, 0.44f));
}

static void drawFocusRing()
{
    const splash::Layout& lay = splash::gLayout;
    const glm::vec3 ring(0.15f, 0.45f, 0.90f);
    const float pad = 4.0f;
    const int f = splash::gFocus;

    if (f >= splash::kFocusSize && f <= splash::kFocusScenario)
    {
        strokeRectTD(lay.colX - pad, lay.dropdownY[f] - pad,
                     lay.colW + 2 * pad, lay.dropdownH + 2 * pad, ring);
    }
    else if (f == splash::kFocusRegion)
    {
        strokeRectTD(lay.colX - pad, lay.regionButtonY - pad,
                     lay.colW + 2 * pad, 34.0f + 2 * pad, ring);
    }
    else if (f == splash::kFocusPaused)
    {
        const float labelW = textWidth("Start Paused", 0.32f);
        strokeRectTD(lay.pausedX - pad, lay.pausedY - pad,
                     18.0f + 8.0f + labelW + 2 * pad, 18.0f + 2 * pad, ring);
    }
    else if (f >= splash::kFocusSim && f <= splash::kFocusReplay)
    {
        const int b = f - splash::kFocusSim;
        strokeRectTD(lay.buttonX - pad, lay.buttonY[b] - pad,
                     lay.buttonW + 2 * pad, lay.buttonH + 2 * pad, ring);
    }
}

// ---------------------------------------------------------------------
// Namespace splash: state, setup, API
// ---------------------------------------------------------------------
namespace splash {
    int lattice_size = 21;
    int numLayers = 10;
    bool shouldExit = false;
    bool helpHover = false;

    // Layout recomputed from the window size, keyboard focus and the physical
    // memory figure shown in the run summary (read once, in initialize()).
    Layout gLayout;
    int    gFocus = 0;
    int    gFocusBeforeRegion = 0;   // ring position to restore when the overlay closes
    double gPhysicalBytes = 0.0;

    Button* simBtn = nullptr;
    Button* statBtn = nullptr;
    Button* replayBtn = nullptr;
    Button* helpLink = nullptr;
    Button* presetSmall = nullptr;
    Button* presetMid = nullptr;
    Button* presetLarge = nullptr;
    Button* regionButton = nullptr;      // opens the 3D overlay
    Tickbox* startPausedBox = nullptr;
    SubRegionBox* subregion = nullptr;   // the region itself (see subregion_box.h)
    SubRegionModal* regionModal = nullptr;
    Cortina* sizeDropdown = nullptr;
    Cortina* layerDropdown = nullptr;
    Cortina* scenarioDropdown = nullptr;
    framework::Logo* logo_splash = nullptr;

    GLFWwindow* window = nullptr;

    // Single scenario: the full simulation dynamics (formerly "scenario 7").
    // The debug scenarios of the reference GUI were scaffolding and are
    // removed; future scenarios can be appended here and in
    // framework::scenarioHelpTexts (help.cpp).
    std::vector<std::string> scenarioOptions = {
        "Full simulation"
    };

    void close_drops_callback(int) {
        if (sizeDropdown) sizeDropdown->close();
        if (layerDropdown) layerDropdown->close();
        if (scenarioDropdown) scenarioDropdown->close();
    }

    // ------------------------------------------------------------------
    // Parameter lists and index mapping
    //
    // ONE source of truth for both dropdowns and for every consumer of the
    // selection.  The old code repeated the layer list four times (once here
    // and once per button handler), recomputed "5 + 2 * index" by hand in three
    // of them, and did it in none of them on the Enter shortcut.
    // ------------------------------------------------------------------
    const std::vector<int>& layerValues()
    {
        static const std::vector<int> kLayerValues = {
            10,12,14,16,18,20,24,28,32,38,44,52,60,70,82,96,112,130,
            150,174,200,230,264,300,320,340,360,364
        };
        return kLayerValues;
    }

    static int sizeFromIndex(int index)
    {
        const int count = (kSizeMax - kSizeMin) / kSizeStep + 1;
        if (index < 0)      index = 0;
        if (index >= count) index = count - 1;
        return kSizeMin + kSizeStep * index;
    }

    static int closestSizeIndex(int L)
    {
        const int count = (kSizeMax - kSizeMin) / kSizeStep + 1;
        int best = 0;
        int bestDistance = -1;

        for (int i = 0; i < count; ++i)
        {
            const int distance = std::abs(sizeFromIndex(i) - L);
            if (bestDistance < 0 || distance < bestDistance)
            {
                bestDistance = distance;
                best = i;
            }
        }
        return best;
    }

    static int closestLayerIndex(int W)
    {
        const std::vector<int>& values = layerValues();
        int best = 0;
        int bestDistance = -1;

        for (size_t i = 0; i < values.size(); ++i)
        {
            const int distance = std::abs(values[(int)i] - W);
            if (bestDistance < 0 || distance < bestDistance)
            {
                bestDistance = distance;
                best = (int)i;
            }
        }
        return best;
    }

    static Selection readSelectionFromUI()
    {
        Selection sel;

        if (sizeDropdown)
            sel.L = sizeFromIndex(sizeDropdown->getSelectedIndex());

        if (layerDropdown)
        {
            const std::vector<int>& values = layerValues();
            const int index = layerDropdown->getSelectedIndex();
            if (index >= 0 && index < (int)values.size())
                sel.W = values[index];
        }

        if (scenarioDropdown)
            sel.scenario = scenarioDropdown->getSelectedIndex();

        if (startPausedBox)
            sel.startPaused = startPausedBox->getState();

        // The subregion model is the source of the requested bounds; its lattice
        // size follows the L dropdown through syncSubregionLattice().
        if (subregion)
        {
            sel.subLo = glm::ivec3(subregion->x0(), subregion->y0(), subregion->z0());
            sel.subHi = glm::ivec3(subregion->x1(), subregion->y1(), subregion->z1());
        }
        else
        {
            const int last = sel.L - 1;
            sel.subLo = glm::ivec3(0, 0, 0);
            sel.subHi = glm::ivec3(last, last, last);
        }

        return sel;
    }

    // The lattice side lives in the L dropdown and the cube follows it.  Called
    // once per frame before drawing; setLatticeSize() returns early when the
    // side did not change, so this costs a comparison.
    static void syncSubregionLattice()
    {
        if (!subregion || !sizeDropdown) return;

        const int L = sizeFromIndex(sizeDropdown->getSelectedIndex());
        if (subregion->setLatticeSize(L))
            printSubregion("lattice side changed");
    }

    // The lattice edges the run will use: the region's extents, or L itself when
    // the region is the whole lattice (so a plain run is unchanged).  The panel
    // and the launcher share it, so the numbers on screen are the ones allocated.
    static void effectiveEdges(int L, int& ex, int& ey, int& ez)
    {
        ex = ey = ez = L;

        if (!subregion) return;

        ex = subregion->x1() - subregion->x0() + 1;
        ey = subregion->y1() - subregion->y0() + 1;
        ez = subregion->z1() - subregion->z0() + 1;
    }

    // Opens the 3D overlay that edits the region (subregion_modal.h).  Control
    // comes back on Enter or Esc; the region is edited live, so there is nothing
    // to confirm.
    static bool openRegionOverlay()
    {
        if (!regionModal || !subregion) return false;

        // Remember where the ring was: closing the overlay puts it back, so the
        // key that closed it does not immediately reopen it.
        gFocusBeforeRegion = gFocus;

        regionModal->open(subregion, winW(), winH());
        regionModal->setRunContext(readSelectionFromUI().W,
                                   (unsigned long long)sizeof(automaton::Cell));

        std::cout << "[Splash] Region overlay opened ("
                  << subregion->handleName(subregion->activeHandle())
                  << " face active)" << std::endl;
        return true;
    }

    // One stdout line per change.  This is the point of the widget step: the
    // numbers are printed, and visible on screen, BEFORE anything in the model
    // reads them (the integration is the next step, see README).
    static void printSubregion(const char* reason)
    {
        if (!subregion) return;

        const Selection sel = readSelectionFromUI();

        std::cout << subregion->report(sel.W, (unsigned long long)sizeof(automaton::Cell));

        if (reason && *reason)
            std::cout << "   [" << reason << "; face "
                      << subregion->handleName(subregion->activeHandle()) << "]";

        std::cout << std::endl;
    }

    static int sizeCount()
    {
        return (kSizeMax - kSizeMin) / kSizeStep + 1;
    }

    // 92610 -> "92,610": the summary reads better with separators.
    static std::string withThousands(unsigned long long value)
    {
        const std::string digits = std::to_string(value);
        std::string out;
        out.reserve(digits.size() + digits.size() / 3);

        for (size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && ((digits.size() - i) % 3) == 0)
                out.push_back(',');
            out.push_back(digits[i]);
        }
        return out;
    }

    static std::string formatBytes(double bytes)
    {
        const double gib = 1024.0 * 1024.0 * 1024.0;
        const double mib = 1024.0 * 1024.0;

        if (bytes <= 0.0)
            return "unknown";

        char buf[64];
        if (bytes >= gib)
            std::snprintf(buf, sizeof(buf), "%.2f GB", bytes / gib);
        else if (bytes >= mib)
            std::snprintf(buf, sizeof(buf), "%.1f MB", bytes / mib);
        else
            std::snprintf(buf, sizeof(buf), "%.1f KB", bytes / 1024.0);

        return buf;
    }

    static Preview previewParameters(int ex, int ey, int ez, int W)
    {
        Preview p;

        // The lattice the run uses: the region's edges.  These are the numbers
        // automaton::configureLatticeFromRegion will produce -- a cubic region
        // through calculateParameters (edges = L), anything else through
        // tryAllocateTube (schedule scale = long edge, RMAX = short side).
        p.cells = (unsigned long long)ex * (unsigned long long)ey *
                  (unsigned long long)ez * (unsigned long long)W;
        p.bytes = (double)p.cells * 3.0 * (double)sizeof(automaton::Cell);

        const int L = ex;                       // schedule scale (EL)
        p.rmax  = ((ey < ez) ? ey : ez) / 2;    // short side (RMAX)

        p.islandCount = 9 * L;
        p.islandSize  = (p.islandCount > 0) ? (W / p.islandCount) : 0;
        if (p.islandSize == 0) p.islandSize = 1;

        // Light-frame length.  Mirrors the schedule built by
        // automaton::calculateParameters / tryAllocateTube (src/model/initSim.cpp):
        // the live panel cannot call them, because they print one line per layer
        // and rebuild the centre list.  CENTER only enters through CONTRACT, which
        // is not part of FRAME, so it is not needed here.  The copy is re-checked
        // against the model by previewMatchesModel() on every start.
        const int rmax = p.rmax;
        const int gx = W + 2 * rmax;
        const int gy = gx + 2 * rmax;
        const int gz = gy + 2 * rmax;
        const int s1 = gz + rmax;
        const int s2 = s1 + 3 * (L - 1);
        const int s3 = s2 + 3 * (L - 1);
        const int s4 = s3 + 2 * W;
        const int s5 = s4 + 3 * (L - 1);
        const int s6 = s5 + (L - 1);
        const int s7 = s6 + (L - 1);
        const int s8 = s7 + (L - 1);
        p.frame = s8 + 1 + 3 * (L - 1);

        return p;
    }

    static bool previewMatchesModel(const Preview& p)
    {
        return p.rmax        == (int)automaton::RMAX         &&
               p.islandCount == (int)automaton::ISLAND_COUNT &&
               p.islandSize  == (int)automaton::ISLAND_SIZE  &&
               p.frame       == (int)automaton::FRAME;
    }

    // ------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------
    static void computeLayout(int width, int height)
    {
        Layout lay;
        lay.w = width;
        lay.h = height;

        const float margin = (width < 720) ? 16.0f : 30.0f;
        const float pad    = 20.0f;

        // Vertical metrics.  The two columns are measured BEFORE placing
        // anything: on a 600x600 window the (nearly square) logo would otherwise
        // push the Replay button out of the window -- the bug the first capture
        // of this screen showed.
        const float labelBand  = 20.0f;   // room for a label above a combo box
        const float rowGap     = 12.0f;
        const float presetCapH = 26.0f;
        const float presetH    = 26.0f;
        const float infoLineH  = 19.0f;
        const int   infoLines  = 10;      // seven run numbers + three subregion lines
        const float infoPadY   = 12.0f;
        const float pausedRowH = 24.0f;
        const float buttonH    = 36.0f;
        const float buttonGap  = 8.0f;
        const float runPadX    = 12.0f;
        const float runPadY    = 12.0f;

        const float rowH = labelBand + lay.dropdownH + rowGap;

        // Subregion editor: one button that opens the 3D overlay, plus the two
        // lines that report what it selects.
        const float regionGap      = 14.0f;
        const float regionLabelH   = 20.0f;
        const float regionButtonH  = 34.0f;
        const float regionReadoutH = 54.0f;

        // Run card: Start Paused + the three mode buttons, one box.
        const float pausedRowPad = 10.0f;
        const float runH = runPadY + pausedRowH + pausedRowPad +
                           3.0f * buttonH + 2.0f * buttonGap + runPadY;

        const float leftColumnH  = pad + 3.0f * rowH + presetCapH + presetH +
                                   regionGap + regionLabelH + regionButtonH +
                                   regionReadoutH + pad;
        const float infoH        = infoLines * infoLineH + 2.0f * infoPadY;
        const float rightColumnH = pad + infoH + 14.0f + runH + pad;
        const float contentH     = (leftColumnH > rightColumnH) ? leftColumnH : rightColumnH;

        lay.infoH = infoH;

        // Vertical placement: the panel keeps room for the content and the logo
        // takes what is left of the band between the title and the panel.
        lay.titleY  = 32.0f;
        lay.logoY   = lay.titleY + 20.0f;
        lay.footerY = (float)height - 26.0f;

        const float footerBand  = 30.0f;
        const float panelTopMax = lay.footerY - footerBand - contentH - 2.0f * pad;

        float logoAspect = 0.28f;   // fallback if the logo failed to load
        if (logo_splash && logo_splash->width() > 0)
            logoAspect = (float)logo_splash->height() / (float)logo_splash->width();
        if (logoAspect <= 0.01f) logoAspect = 0.28f;

        const float logoDesignW = 200.0f;   // the width Logo::draw() targets
        const float logoMaxW    = (float)width * 0.34f;
        float logoScale = (logoMaxW < logoDesignW) ? (logoMaxW / logoDesignW) : 1.0f;
        float logoH     = logoDesignW * logoScale * logoAspect;

        const float maxLogoH = panelTopMax - lay.logoY - 14.0f;
        const float minLogoH = 40.0f;

        if (maxLogoH >= minLogoH && logoH > maxLogoH)
        {
            logoH = maxLogoH;
            logoScale = logoH / (logoDesignW * logoAspect);
        }
        if (maxLogoH >= minLogoH && logoH < minLogoH)
        {
            logoH = minLogoH;
            logoScale = logoH / (logoDesignW * logoAspect);
        }

        lay.logoScale = logoScale;
        lay.logoH     = logoH;
        lay.logoX     = ((float)width - logoDesignW * logoScale) * 0.5f;

        lay.panelX = margin;
        lay.panelY = lay.logoY + logoH + 14.0f;
        if (lay.panelY > panelTopMax) lay.panelY = panelTopMax;
        lay.panelW = (float)width - 2.0f * margin;
        lay.panelH = contentH;

        // Horizontal placement: parameters left, summary + actions right.
        const float innerX = lay.panelX + pad;
        const float innerW = lay.panelW - 2.0f * pad;
        const float colGap = 22.0f;
        const float leftW  = (innerW - colGap) * 0.46f;
        const float rightW = (innerW - colGap) - leftW;

        lay.colX = innerX;
        lay.colW = leftW;

        float y = lay.panelY + pad;
        for (int i = 0; i < 3; ++i)
        {
            lay.labelY[i]    = y + labelBand * 0.5f;   // label centreline
            lay.dropdownY[i] = y + labelBand;          // top of the combo box
            y += rowH;
        }

        lay.presetCaptionY = y + presetCapH * 0.5f;
        lay.presetY        = y + presetCapH;
        lay.presetW        = (leftW - 2.0f * 8.0f) / 3.0f;
        y += presetCapH + presetH + regionGap;

        // The button opens the 3D overlay; its numbers go just under it, still
        // inside the light card.
        lay.regionLabelY   = y + regionLabelH * 0.5f;
        lay.regionButtonY  = y + regionLabelH;
        lay.regionReadoutY = lay.regionButtonY + regionButtonH + 20.0f;

        lay.infoX = innerX + leftW + colGap;
        lay.infoY = lay.panelY + pad;
        lay.infoW = rightW;

        // The run card is the box under the summary card: "Start Paused" and the
        // three mode buttons live together there, so none of them is left
        // dangling outside a box and the tickbox is out of the summary card.
        // It is dark because the Tickbox colours are global and tuned for a dark
        // background (its label is near-white).
        lay.runX = lay.infoX;
        lay.runY = lay.infoY + lay.infoH + 14.0f;
        lay.runW = rightW;
        lay.runH = runH;

        lay.pausedX = lay.runX + runPadX;
        lay.pausedY = lay.runY + runPadY + 4.0f;

        lay.buttonX = lay.runX + runPadX;
        lay.buttonW = rightW - 2.0f * runPadX;
        lay.buttonH = buttonH;

        float by = lay.runY + runPadY + pausedRowH + pausedRowPad;
        for (int i = 0; i < 3; ++i)
        {
            lay.buttonY[i] = by;
            by += buttonH + buttonGap;
        }

        lay.helpW = 70.0f;
        lay.helpH = 20.0f;
        lay.helpX = (float)width - margin - lay.helpW;
        lay.helpY = lay.footerY - 10.0f;

        gLayout = lay;
    }

    // Push the computed geometry into the widgets: after creation, and again
    // whenever the window size changes.
    static void applyLayout()
    {
        const Layout& lay = gLayout;

        if (sizeDropdown)
        {
            sizeDropdown->setPosition(lay.colX, lay.dropdownY[0]);
            sizeDropdown->setSize(lay.colW, lay.dropdownH);
        }
        if (layerDropdown)
        {
            layerDropdown->setPosition(lay.colX, lay.dropdownY[1]);
            layerDropdown->setSize(lay.colW, lay.dropdownH);
        }
        if (scenarioDropdown)
        {
            scenarioDropdown->setPosition(lay.colX, lay.dropdownY[2]);
            scenarioDropdown->setSize(lay.colW, lay.dropdownH);
        }

        const float presetH = 26.0f;
        if (presetSmall)
        {
            presetSmall->setPosition(lay.colX, lay.presetY);
            presetSmall->setSize(lay.presetW, presetH);
        }
        if (presetMid)
        {
            presetMid->setPosition(lay.colX + lay.presetW + 8.0f, lay.presetY);
            presetMid->setSize(lay.presetW, presetH);
        }
        if (presetLarge)
        {
            presetLarge->setPosition(lay.colX + 2.0f * (lay.presetW + 8.0f), lay.presetY);
            presetLarge->setSize(lay.presetW, presetH);
        }

        if (startPausedBox)
            startPausedBox->setPosition((int)lay.pausedX, (int)lay.pausedY);

        if (regionButton)
        {
            regionButton->setPosition(lay.colX, lay.regionButtonY);
            regionButton->setSize(lay.colW, 34.0f);
        }

        if (simBtn)
        {
            simBtn->setPosition(lay.buttonX, lay.buttonY[0]);
            simBtn->setSize(lay.buttonW, lay.buttonH);
        }
        if (statBtn)
        {
            statBtn->setPosition(lay.buttonX, lay.buttonY[1]);
            statBtn->setSize(lay.buttonW, lay.buttonH);
        }
        if (replayBtn)
        {
            replayBtn->setPosition(lay.buttonX, lay.buttonY[2]);
            replayBtn->setSize(lay.buttonW, lay.buttonH);
        }

        if (helpLink)
        {
            helpLink->setPosition(lay.helpX, lay.helpY);
            helpLink->setSize(lay.helpW, lay.helpH);
        }
    }

    // ------------------------------------------------------------------
    // Start-up: one code path for the three buttons and for the Enter key
    // ------------------------------------------------------------------
    namespace {

        constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;

        // lattice_curr + lattice_draft + lattice_partner, each ex*ey*ez*W cells.
        // sizeof(automaton::Cell) comes from the model, never hard-coded
        // (it is 160 bytes with MSVC x64 today).
        double estimateLatticeBytes(int ex, int ey, int ez, int W)
        {
            const double cells = (double)ex * (double)ey * (double)ez * (double)W;
            return cells * 3.0 * (double)sizeof(automaton::Cell);
        }

        // Refuses requests that cannot possibly succeed before the allocator is
        // touched: the dropdowns reach L = 89, W = 364, which needs ~117 GB and
        // could only end in a bad_alloc.  The bound is deliberately loose (90%
        // of physical memory) so it cannot block a run that would have worked.
        // The volume measured is the one the run actually allocates, so a small
        // region can make an otherwise impossible L x W fit.
        bool requestFitsInMemory(int ex, int ey, int ez, int W, std::string& why)
        {
            MEMORYSTATUSEX memory = {};
            memory.dwLength = sizeof(memory);
            if (!GlobalMemoryStatusEx(&memory))
                return true;   // no information: let the allocator decide

            const double needed   = estimateLatticeBytes(ex, ey, ez, W);
            const double physical = (double)memory.ullTotalPhys;

            if (needed > 0.9 * physical)
            {
                char buf[192];
                std::snprintf(buf, sizeof(buf),
                              "the lattice alone needs %.2f GB, more than 90%% of the %.2f GB of physical memory",
                              needed / kGiB, physical / kGiB);
                why = buf;
                return false;
            }
            return true;
        }

    } // anonymous namespace

    // Reports what the run wanted to allocate, not what the dropdowns say: with a
    // region the two differ, and the region is what was refused.
    static void reportStartFailure(const Selection& sel, const std::string& why)
    {
        int ex = sel.L, ey = sel.L, ez = sel.L;
        effectiveEdges(sel.L, ex, ey, ez);

        const unsigned long long cells =
            (unsigned long long)ex * (unsigned long long)ey *
            (unsigned long long)ez * (unsigned long long)sel.W;

        const char* reason = !why.empty() ? why.c_str()
                                          : automaton::lastAllocationError.c_str();
        if (!reason || !*reason)
            reason = "unspecified allocation failure";

        char buf[640];
        std::snprintf(buf, sizeof(buf),
                      "Cannot start with L = %d, W = %d.\n\n"
                      "run lattice = %d x %d x %d x W %d\n"
                      "cells = %llu\n"
                      "lattice memory (3 lattices, %llu bytes/cell) = %.2f GB\n\n"
                      "Reason: %s",
                      sel.L, sel.W,
                      ex, ey, ez, sel.W,
                      cells,
                      (unsigned long long)sizeof(automaton::Cell),
                      estimateLatticeBytes(ex, ey, ez, sel.W) / kGiB,
                      reason);

        std::cerr << "[Splash] " << buf << std::endl;
        MessageBoxA(NULL, buf, "Allocation Error", MB_OK | MB_ICONERROR);
    }

    static bool launch(LaunchTarget target)
    {
        const Selection sel = readSelectionFromUI();

        std::cout << "[Splash] Start: L = " << sel.L
                  << ", W = " << sel.W
                  << ", scenario = " << sel.scenario
                  << ", startPaused = " << (sel.startPaused ? "yes" : "no")
                  << std::endl;

        // The region is what the run uses: its extents become the lattice edges,
        // and only that volume is allocated (automaton::configureLatticeFromRegion).
        // With the whole lattice selected this is L^3 x W, exactly as before.
        int ex = sel.L, ey = sel.L, ez = sel.L;
        effectiveEdges(sel.L, ex, ey, ez);

        const int x0 = subregion ? subregion->x0() : 0;
        const int x1 = subregion ? subregion->x1() : sel.L - 1;
        const int y0 = subregion ? subregion->y0() : 0;
        const int y1 = subregion ? subregion->y1() : sel.L - 1;
        const int z0 = subregion ? subregion->z0() : 0;
        const int z1 = subregion ? subregion->z1() : sel.L - 1;

        if (subregion)
        {
            std::cout << subregion->report(sel.W, (unsigned long long)sizeof(automaton::Cell))
                      << std::endl;
            std::cout << "[Subregion] run lattice = " << ex << " x " << ey << " x " << ez
                      << " x W " << sel.W
                      << ", " << (subregion->isFull() ? "the whole lattice"
                                                      : "a smaller periodic lattice")
                      << std::endl;
        }

        // Keep the published values in sync with what the screen displays.
        lattice_size = sel.L;
        numLayers    = sel.W;

        std::string why;
        if (!requestFitsInMemory(ex, ey, ez, sel.W, why) ||
            !automaton::configureLatticeFromRegion((unsigned)sel.W, x0, x1, y0, y1, z0, z1))
        {
            reportStartFailure(sel, why);
            return false;
        }

        // The live panel derives its numbers locally (it cannot call the model
        // functions on every key press); check that copy against the model the
        // call above just configured.  This has to come after the configuration:
        // before it, it would compare against the previous run's parameters (or
        // against zeros on the first start).
        if (!previewMatchesModel(previewParameters(ex, ey, ez, sel.W)))
        {
            std::cerr << "[Splash] WARNING: the panel preview disagrees with the model "
                         "parameters for lattice " << ex << " x " << ey << " x " << ez
                      << ", W = " << sel.W << std::endl;
        }

        // The selection becomes the configuration the next run opens with.
        gConfig.simulation.lattice = sel.L;
        gConfig.simulation.layers  = sel.W;

        // The region too, so the setup screen reopens on it (keys
        // simulation.subX0..subZ1).
        if (subregion)
        {
            gConfig.simulation.subX0 = subregion->x0();
            gConfig.simulation.subX1 = subregion->x1();
            gConfig.simulation.subY0 = subregion->y0();
            gConfig.simulation.subY1 = subregion->y1();
            gConfig.simulation.subZ0 = subregion->z0();
            gConfig.simulation.subZ1 = subregion->z1();
        }

        if (target == LaunchTarget::Statistics)
            gConfig.simulation.scenario = 0;   // statistics always ran the single scenario
        else if (target == LaunchTarget::Simulation)
            gConfig.simulation.scenario = sel.scenario;

        // `paused` is the flag the simulation thread actually reads (core.cpp,
        // replay.cpp).  The mouse path used to write the dead global
        // `initPaused` instead, so "Start Paused" only worked through Enter.
        if (target != LaunchTarget::Statistics)
            paused = sel.startPaused;

        if (!gConfigPath.empty())
            saveConfig(gConfigPath);

        switch (target)
        {
            case LaunchTarget::Simulation: currentMode = SIMULATION; break;
            case LaunchTarget::Statistics: currentMode = STATISTICS; break;
            case LaunchTarget::Replay:     currentMode = REPLAY;     break;
        }

        glfwMaximizeWindow(window);

        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        ProjectionManager::instance().setViewport(w, h);

        // The Enter shortcut always refreshed the projection; the buttons did not.
        framework::updateProjection();

        shouldExit = true;
        return true;
    }

    // Widgets are created once and then positioned by applyLayout(); every
    // position below comes from the computed layout, so a resize keeps working.
    static void createWidgets()
    {
        simBtn    = new Button(0, 0, 200, 40, "Simulation");
        statBtn   = new Button(0, 0, 200, 40, "Statistics");
        replayBtn = new Button(0, 0, 200, 40, "Replay");
        helpLink  = new Button(0, 0, 70, 20, "Help");

        // Quick presets (L/W): the middle one is the value the paper suggests.
        presetSmall = new Button(0, 0, 80, 26, "15/12");
        presetMid   = new Button(0, 0, 80, 26, "21/10");
        presetLarge = new Button(0, 0, 80, 26, "31/20");

        std::vector<std::string> sizes, layers;
        for (int s = kSizeMin; s <= kSizeMax; s += kSizeStep)
            sizes.push_back(std::to_string(s));
        for (int l : layerValues())
            layers.push_back(std::to_string(l));

        sizeDropdown     = new Cortina(0, 0, 200, 30, sizes, 0, close_drops_callback);
        layerDropdown    = new Cortina(0, 0, 200, 30, layers, 0, close_drops_callback);
        scenarioDropdown = new Cortina(0, 0, 200, 30, scenarioOptions, 0, close_drops_callback);

        startPausedBox = new Tickbox(0, 0, "Start Paused");

        // The region of the lattice: a button that opens the 3D overlay, and the
        // model the overlay edits.  Its side follows the L dropdown
        // (syncSubregionLattice) and its numbers are printed on every change.
        regionButton = new Button(0, 0, 200, 34, "Select region...");
        subregion    = new SubRegionBox();
        regionModal  = new SubRegionModal();

        logo_splash = new framework::Logo("logo_bar.png");
    }

    static void setupUI()
    {
        createWidgets();

        // The layout needs the logo texture, because its aspect sets the height
        // of the logo band: createWidgets() must have run first.
        computeLayout(winW(), winH());
        applyLayout();

        // Seed the dropdowns from automaton.cfg, which the last run wrote back:
        // the setup screen opens on the run that was actually used.  Values the
        // UI cannot represent snap to the closest offered one.
        sizeDropdown->setSelectedIndex(closestSizeIndex(gConfig.simulation.lattice));
        layerDropdown->setSelectedIndex(closestLayerIndex(gConfig.simulation.layers));

        if (gConfig.simulation.scenario >= 0 &&
            gConfig.simulation.scenario < (int)scenarioOptions.size())
        {
            scenarioDropdown->setSelectedIndex(gConfig.simulation.scenario);
        }
        else
        {
            // -1 (or out of range) means "the splash screen decides".
            scenarioDropdown->setSelectedIndex(0);
        }

        gFocus = kFocusSize;

        // The cube starts on the whole lattice of the L the screen opened with,
        // then takes the region automaton.cfg carried (an absent region means the
        // whole lattice, which loadConfig already resolved against that same L).
        // restore() adjusts anything the widget cannot represent.
        syncSubregionLattice();

        if (subregion)
        {
            subregion->restore(gConfig.simulation.subX0, gConfig.simulation.subX1,
                               gConfig.simulation.subY0, gConfig.simulation.subY1,
                               gConfig.simulation.subZ0, gConfig.simulation.subZ1);

            if (!subregion->isFull())
                std::cout << "[Subregion] opened with the region saved in "
                          << (gConfigPath.empty() ? std::string("the defaults") : gConfigPath)
                          << std::endl;
        }

        // Reports the starting point once, so the log shows it.
        printSubregion("setup");
    }

    int initialize(GLFWwindow* win)
    {
        window = win;
        shouldExit = false;
        helpHover = false;

        // Physical memory: shown in the run summary and used to colour it, and
        // read once here because it does not change while the app runs.
        gPhysicalBytes = 0.0;
        MEMORYSTATUSEX memory = {};
        memory.dwLength = sizeof(memory);
        if (GlobalMemoryStatusEx(&memory))
            gPhysicalBytes = (double)memory.ullTotalPhys;

        glfwSetMouseButtonCallback(window, mouseButtonCallback);
        glfwSetScrollCallback(window, scrollCallback);
        glfwSetKeyCallback(window, keyCallback);
        glfwSetCursorPosCallback(window, passiveMotionCallback);

        setupUI();
        return 0;
    }

    void cleanup()
    {
        delete simBtn; simBtn = nullptr;
        delete statBtn; statBtn = nullptr;
        delete replayBtn; replayBtn = nullptr;
        delete helpLink; helpLink = nullptr;
        delete presetSmall; presetSmall = nullptr;
        delete presetMid; presetMid = nullptr;
        delete presetLarge; presetLarge = nullptr;
        delete startPausedBox; startPausedBox = nullptr;
        delete regionButton; regionButton = nullptr;
        delete regionModal; regionModal = nullptr;
        delete subregion; subregion = nullptr;
        delete sizeDropdown; sizeDropdown = nullptr;
        delete layerDropdown; layerDropdown = nullptr;
        delete scenarioDropdown; scenarioDropdown = nullptr;
        delete logo_splash; logo_splash = nullptr;
    }


    void render()
    {
        glClearColor(0.95f, 0.95f, 0.97f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // The window is resizable: recompute the geometry when it changes.
        if (winW() != gLayout.w || winH() != gLayout.h)
        {
            computeLayout(winW(), winH());
            applyLayout();
        }

        const int w = winW();
        const int h = winH();

        drawTitle();
        drawLogo();
        drawPanels();
        drawFormLabels();

        // The subregion editor follows the L dropdown, so it is synced before it
        // is drawn; the ring around its button is the ordinary focus ring.
        syncSubregionLattice();
        drawSubregionReadout();

        if (regionButton) regionButton->draw(*textRenderer, w, h);

        if (helpLink) helpLink->drawAsHyperlink(*textRenderer, helpHover, w, h);

        if (simBtn)    simBtn->draw(*textRenderer, w, h);
        if (statBtn)   statBtn->draw(*textRenderer, w, h);
        if (replayBtn) replayBtn->draw(*textRenderer, w, h);
        if (presetSmall) presetSmall->draw(*textRenderer, w, h);
        if (presetMid)   presetMid->draw(*textRenderer, w, h);
        if (presetLarge) presetLarge->draw(*textRenderer, w, h);

        if (scenarioDropdown) scenarioDropdown->render(textRenderer);
        if (layerDropdown)    layerDropdown->render(textRenderer);
        if (sizeDropdown)     sizeDropdown->render(textRenderer);

        if (startPausedBox) {
            startPausedBox->setFontScale(0.32f);
            startPausedBox->draw(*textRenderer);
        }

        // The summary goes on the dark card, and the focus ring and the keyboard
        // hints last, so that nothing can cover them.
        drawRunSummary();
        drawFocusRing();
        drawFooter();

        // The 3D overlay goes last: it dims the whole window and draws its own
        // view, its gizmo and its readouts on top of the setup screen.
        if (regionModal && regionModal->isOpen())
        {
            const Selection overlaySel = readSelectionFromUI();
            regionModal->setRunContext(overlaySel.W,
                                       (unsigned long long)sizeof(automaton::Cell));
            regionModal->render(textRenderer, w, h);
        }
    }
} // namespace splash

// ---------------------------------------------------------------------
// Presets: move the dropdowns only.  Nothing is allocated and no mode starts,
// so the user still sees the updated summary before pressing Enter.
// ---------------------------------------------------------------------
static void applyPreset(int L, int W, int focusIndex)
{
    if (splash::sizeDropdown)  splash::sizeDropdown->setSelectedIndex(splash::closestSizeIndex(L));
    if (splash::layerDropdown) splash::layerDropdown->setSelectedIndex(splash::closestLayerIndex(W));

    splash::gFocus = focusIndex;

    std::cout << "[Splash] Preset: L = " << L << ", W = " << W << std::endl;
}

// ---------------------------------------------------------------------
// Mouse, scroll, key, and motion callbacks
// ---------------------------------------------------------------------
static void getMousePos(int& mx, int& my)
{
    double x, y;
    glfwGetCursorPos(splash::window, &x, &y);
    mx = static_cast<int>(x);
    my = winH() - static_cast<int>(y);
}

static int myTopDown(int my_bottom)
{
    return winH() - my_bottom;
}

void mouseButtonCallback(GLFWwindow*, int button, int action, int)
{
    // While the 3D overlay is open it owns every mouse event: it orbits, zooms
    // and drags the faces, and the setup widgets underneath must not react.
    if (splash::regionModal && splash::regionModal->isOpen())
    {
        if (action == GLFW_RELEASE)
        {
            const SubRegionModal::Result r = splash::regionModal->onMouseRelease();
            if (r.changed) splash::printSubregion("3D overlay");
        }
        else if (action == GLFW_PRESS)
        {
            const int b = (button == GLFW_MOUSE_BUTTON_LEFT)   ? 0
                        : (button == GLFW_MOUSE_BUTTON_MIDDLE) ? 2 : -1;
            if (b >= 0)
            {
                double x, y;
                glfwGetCursorPos(splash::window, &x, &y);
                const SubRegionModal::Result r =
                    splash::regionModal->onMousePress((float)x, (float)y, b);
                if (r.changed) splash::printSubregion("3D overlay");
            }
        }
        return;
    }

    if (button != GLFW_MOUSE_BUTTON_LEFT) return;

    // One click, one action.  Everything below acts on the PRESS only: the
    // release arrives at the same spot and used to repeat the action, which
    // undid it -- the tickbox toggled straight back off, and a dropdown opened
    // and immediately closed again.
    if (action != GLFW_PRESS) return;

    double xpos, ypos;
    glfwGetCursorPos(splash::window, &xpos, &ypos);
    int mx = static_cast<int>(xpos);
    int my_raw = static_cast<int>(ypos);
    int my_dropdown = winH() - my_raw;
    int my_button = winH() - my_raw;

    // Clicking a control also moves the keyboard focus to it, so mouse and
    // keyboard navigation cannot disagree about where the focus ring is.
    bool handled = false;
    if (splash::sizeDropdown && splash::sizeDropdown->handleMouseClick(mx, my_dropdown))
    {
        splash::gFocus = splash::kFocusSize;
        handled = true;
    }
    if (!handled && splash::layerDropdown && splash::layerDropdown->handleMouseClick(mx, my_dropdown))
    {
        splash::gFocus = splash::kFocusLayers;
        handled = true;
    }
    if (!handled && splash::scenarioDropdown && splash::scenarioDropdown->handleMouseClick(mx, my_dropdown))
    {
        splash::gFocus = splash::kFocusScenario;
        handled = true;
    }

    if (!handled) {
        if (splash::sizeDropdown) splash::sizeDropdown->close();
        if (splash::layerDropdown) splash::layerDropdown->close();
        if (splash::scenarioDropdown) splash::scenarioDropdown->close();
    }

    // All three mode buttons go through the same reader (readSelectionFromUI) and
    // the same allocator path, so they cannot drift from the Enter key or from
    // each other again.
    if (splash::simBtn && splash::simBtn->contains(mx, my_button, winH())) {
        splash::gFocus = splash::kFocusSim;
        splash::launch(splash::LaunchTarget::Simulation);
    }
    else if (splash::statBtn && splash::statBtn->contains(mx, my_button, winH())) {
        splash::gFocus = splash::kFocusStats;
        splash::launch(splash::LaunchTarget::Statistics);
    }
    else if (splash::replayBtn && splash::replayBtn->contains(mx, my_button, winH())) {
        splash::gFocus = splash::kFocusReplay;
        splash::launch(splash::LaunchTarget::Replay);
    }
    else if (splash::presetSmall && splash::presetSmall->contains(mx, my_button, winH())) {
        applyPreset(15, 12, splash::kFocusSize);
    }
    else if (splash::presetMid && splash::presetMid->contains(mx, my_button, winH())) {
        applyPreset(21, 10, splash::kFocusSize);
    }
    else if (splash::presetLarge && splash::presetLarge->contains(mx, my_button, winH())) {
        applyPreset(31, 20, splash::kFocusSize);
    }
    else if (splash::regionButton && splash::regionButton->contains(mx, my_button, winH())) {
        splash::gFocus = splash::kFocusRegion;
        splash::openRegionOverlay();
    }
    else if (splash::helpLink && splash::helpLink->contains(mx, my_button, winH())) {
        // Same link (and same URL) as the HUD hyperlink; see help.cpp.
        framework::openHelpPage();
    }

    if (splash::startPausedBox && textRenderer) {
        const int myTopDown = winH() - my_button;

        // The whole tickbox responds, label included (see Tickbox::hitTest).
        if (splash::startPausedBox->hitTest(mx, myTopDown, *textRenderer))
        {
            splash::startPausedBox->toggle();
            splash::gFocus = splash::kFocusPaused;

            std::cout << "[Splash] Start Paused = "
                      << (splash::startPausedBox->getState() ? "yes" : "no")
                      << std::endl;
        }
    }
}

void scrollCallback(GLFWwindow*, double xoffset, double yoffset)
{
    // The wheel zooms the overlay while it is open.
    if (splash::regionModal && splash::regionModal->isOpen())
    {
        splash::regionModal->onScroll((float)yoffset);
        return;
    }

    int mx, my;
    getMousePos(mx, my);
    if (splash::sizeDropdown && splash::sizeDropdown->isExpanded())
        splash::sizeDropdown->handleMouseScroll(mx, my, yoffset);
    if (splash::layerDropdown && splash::layerDropdown->isExpanded())
        splash::layerDropdown->handleMouseScroll(mx, my, yoffset);
    if (splash::scenarioDropdown && splash::scenarioDropdown->isExpanded())
        splash::scenarioDropdown->handleMouseScroll(mx, my, yoffset);
}

void keyCallback(GLFWwindow*, int key, int, int action, int mods)
{
    const bool pressed = (action == GLFW_PRESS);
    const bool repeat  = (action == GLFW_REPEAT);
    if (!pressed && !repeat) return;

    // The subregion keys belong to the 3D overlay now: here the ring only starts
    // it with Enter, so the arrows keep moving the focus as everywhere else.
    if (splash::regionModal && splash::regionModal->isOpen())
    {
        const SubRegionModal::Result r = splash::regionModal->onKey(key, mods);

        if (r.consumed)
        {
            if (r.changed) splash::printSubregion("3D overlay");

            if (!splash::regionModal->isOpen())
            {
                splash::printSubregion("overlay closed");

                // Back to where the ring was.  If that was the region button, it
                // moves to Simulation, so the footer's "Enter: start" still holds
                // (otherwise Enter would reopen the overlay instead of starting).
                splash::gFocus = splash::gFocusBeforeRegion;
                if (splash::gFocus == splash::kFocusRegion)
                    splash::gFocus = splash::kFocusSim;
            }
            return;
        }
    }

    if (pressed)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            // Quit from the setup screen (the main loop watches gAppShouldExit).
            gAppShouldExit = true;
            return;
        }

        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER)
        {
            // The ring on the region button starts the 3D overlay; on one of the
            // mode buttons it starts that mode; otherwise it starts the
            // simulation, with exactly the selection on screen.
            if (splash::gFocus == splash::kFocusRegion && splash::openRegionOverlay())
                return;

            if (splash::gFocus == splash::kFocusStats)       splash::launch(splash::LaunchTarget::Statistics);
            else if (splash::gFocus == splash::kFocusReplay) splash::launch(splash::LaunchTarget::Replay);
            else                                             splash::launch(splash::LaunchTarget::Simulation);
            return;
        }

        if (key == GLFW_KEY_TAB)
        {
            const int dir = (mods & GLFW_MOD_SHIFT) ? -1 : 1;
            splash::gFocus = (splash::gFocus + dir + splash::kFocusCount) % splash::kFocusCount;
            return;
        }

        if (key == GLFW_KEY_UP)
        {
            splash::gFocus = (splash::gFocus + splash::kFocusCount - 1) % splash::kFocusCount;
            return;
        }

        if (key == GLFW_KEY_DOWN)
        {
            splash::gFocus = (splash::gFocus + 1) % splash::kFocusCount;
            return;
        }
    }

    // Left/Right change the value of the focused control (key repeat allowed).
    int step = 0;
    if (key == GLFW_KEY_LEFT)  step = -1;
    if (key == GLFW_KEY_RIGHT) step = 1;
    if (step == 0) return;

    switch (splash::gFocus)
    {
        case splash::kFocusSize:
            if (splash::sizeDropdown)
                splash::sizeDropdown->setSelectedIndex(splash::sizeDropdown->getSelectedIndex() + step);
            break;

        case splash::kFocusLayers:
            if (splash::layerDropdown)
                splash::layerDropdown->setSelectedIndex(splash::layerDropdown->getSelectedIndex() + step);
            break;

        case splash::kFocusScenario:
            if (splash::scenarioDropdown)
                splash::scenarioDropdown->setSelectedIndex(splash::scenarioDropdown->getSelectedIndex() + step);
            break;

        case splash::kFocusPaused:
            if (splash::startPausedBox)
            {
                splash::startPausedBox->toggle();

                // Same line the mouse click prints, so the log shows the state
                // whichever way it was changed.
                std::cout << "[Splash] Start Paused = "
                          << (splash::startPausedBox->getState() ? "yes" : "no")
                          << std::endl;
            }
            break;

        default:
            break;   // the cube has its own block above; the buttons start on Enter
    }
}

void passiveMotionCallback(GLFWwindow*, double xpos, double ypos)
{
    int mx = (int)xpos;
    int my = winH() - (int)ypos;
    int my_top = myTopDown(my);

    // The overlay takes the motion while it is open: it orbits, drags a face and
    // highlights the handle under the cursor.
    if (splash::regionModal && splash::regionModal->isOpen())
    {
        const SubRegionModal::Result r =
            splash::regionModal->onMouseMove((float)xpos, (float)ypos);
        if (r.changed) splash::printSubregion("3D overlay");
        return;
    }

    if (splash::helpLink) splash::helpHover = splash::helpLink->contains(mx, my_top, winH());
    if (splash::sizeDropdown) splash::sizeDropdown->updateHover(mx, my);
    if (splash::layerDropdown) splash::layerDropdown->updateHover(mx, my);
    if (splash::scenarioDropdown) splash::scenarioDropdown->updateHover(mx, my);
}