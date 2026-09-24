// GUI_2D.cpp
// No per-frame allocations, core-profile safe

#include "GUI.h"
#include "globals.h"
#include "layers.h"
#include "progress.h"
#include "replay_progress.h"
#include "logo.h"
#include "splash.h"
#include "hslider.h"
#include "vslider.h"
#include "text_renderer.h"
#include "model/simulation.h"
#include "color_utils.h"
#include "draw_utils.h"
#include "help.h"
#include "projection_manager.h"
#include "config.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sstream>
#include <vector>

namespace automaton
{
    extern unsigned EL;
    extern unsigned W_USED;
}

namespace splash
{
    extern std::vector<std::string> scenarioOptions;
}

extern Mode currentMode;

namespace framework
{
    using namespace automaton;

    extern TextRenderer hudText;

    // References to variables declared in GUI.cpp and globals.h
    extern bool showHelp;
    extern std::vector<std::string> scenarioHelpTexts;
    extern HSlider hslider;
    extern VSlider vslider;
    extern int windowWidth;
    extern int windowHeight;
    extern std::unique_ptr<LayerList> layerList;

    // ------------------------------------------------------------------
    // The status row
    //
    // Scenario, Compute, Light, Tick, Era and Mode are one line of displays between the two
    // side panels, across the width of the 3D window -- the space the panels leave between
    // them, not the whole window -- so a wider 3D view widens the cells instead of leaving
    // gaps between the displays, and a narrower one shrinks them instead of letting two
    // displays run into each other.  All six fields share one font size (see
    // renderStatusRow), the cell boundaries are marked with dividers drawn level with the
    // text, and a field that does not fit its cell falls back to its bare value and then to
    // an ellipsis instead of running over its neighbour.
    //
    // The line's y is fed to the text renderer as `height - kStatusLineY`, i.e. the row sits
    // kStatusLineY below the *top* edge: RenderText counts y upwards from the bottom of the
    // window, while the 2D helpers used for the dividers count downwards from the top.  The
    // two spaces are opposite, and getting the conversion backwards is what put the dividers
    // at the far end of the window (experiments\row_gauge.cpp measures both ends now).
    //
    // Every display used to sit at a hard-coded x (scenario 230, compute 420, light/tick
    // 760, era 1100, mode 1300), which lined up at one window width only -- and put Mode
    // outside the window below ~1400 px.  To change the partition, change the contents of
    // the cells and kStatusCells.
    // ------------------------------------------------------------------
    namespace
    {
        constexpr int   kStatusCells  = 6;
        constexpr float kStatusPad    = 10.0f;   // inset of the text inside its own cell
        constexpr float kStatusGap    = 16.0f;   // clearance of the row from each side panel
        constexpr float kStatusMinScale = 0.5f;  // the row is never drawn smaller than this
        // kStatusLineY (the row's distance from the top edge) and the panel geometry are
        // in GUI.h, where the build gauge in experiments\row_gauge.cpp can read them too.

        constexpr int kCellScenario = 0;
        constexpr int kCellCompute  = 1;
        constexpr int kCellLight    = 2;
        constexpr int kCellTick     = 3;
        constexpr int kCellEra      = 4;
        constexpr int kCellMode     = 5;
        static_assert(kCellMode < kStatusCells, "status row: cell index outside the row");

        struct StatusRow
        {
            float x0 = 0.0f;      // left edge of the band: the 3D window's left edge
            float x1 = 1.0f;      // right edge of the band
            float y  = 0.0f;

            bool  valid()         const { return (x1 - x0) > 2.0f * kStatusPad; }
            float cellW()         const { return (x1 - x0) / float(kStatusCells); }
            float cellLeft(int c) const { return x0 + cellW() * float(c); }
            float textX(int c)    const { return cellLeft(c) + kStatusPad; }
            float avail()         const { return cellW() - 2.0f * kStatusPad; }
        };

        StatusRow statusRow()
        {
            StatusRow row;
            row.x0 = kPanelLeftX + kPanelLeftW + kStatusGap;
            row.x1 = (float)gViewport[2] - (kPanelRightW + kPanelRightInset) - kStatusGap;
            row.y  = (float)gViewport[3] - kStatusLineY;
            return row;
        }

        // Does `text` fit inside `avail` pixels at scale `s`?
        bool fitsAt(const std::string& text, float s, float avail)
        {
            if (avail <= 0.0f || s <= 0.0f)
                return true;

            const float w = hudText.measureTextWidth(text, s);
            return w <= 0.0f || w <= avail;
        }

        // The scale at which `text` would exactly fill `avail` pixels (1.0 when it already
        // fits).  The row takes the smallest value over its fields, so every display of the
        // row ends up at the same font size.
        float neededScale(const std::string& text, float avail)
        {
            if (avail <= 0.0f)
                return 1.0f;

            const float w = hudText.measureTextWidth(text, 1.0f);

            if (w <= 0.0f)
                return 1.0f;

            return avail / w;
        }

        // Last resort: cut the text short with an ellipsis until it fits.
        std::string cutToFit(const std::string& text, float s, float avail)
        {
            std::string cut = text;

            while (!cut.empty() && !fitsAt(cut + "...", s, avail))
                cut.pop_back();

            return cut.empty() ? std::string("...") : cut + "...";
        }

        // Draws one field inside its own cell at the row's scale, in this order of
        // preference: as it is; as its bare value (`alt`, when the field carries a label and
        // the value alone fits -- "Full simulation" reads better than "Scenario: Full...");
        // finally cut short with an ellipsis.  The scale itself is never touched here: it
        // belongs to the whole row, so that no two displays end up at different sizes.
        void drawField(const std::string& text, const std::string& alt,
                       const StatusRow& row, int cell, float scale,
                       const glm::vec3& color)
        {
            if (!row.valid())
                return;

            const std::string* shown = &text;

            if (!fitsAt(*shown, scale, row.avail()))
            {
                if (!alt.empty() && fitsAt(alt, scale, row.avail()))
                {
                    shown = &alt;
                }
                else
                {
                    hudText.RenderText(cutToFit(*shown, scale, row.avail()),
                                       row.textX(cell), row.y, scale, color,
                                       gViewport[2], gViewport[3]);
                    return;
                }
            }

            hudText.RenderText(*shown, row.textX(cell), row.y, scale, color,
                               gViewport[2], gViewport[3]);
        }

        // The two readouts that belong to a run and sit inside the right-hand panel, as they
        // did before: the original x = 1700 and x = 1730 are 40 and 70 px inside the panel's
        // left edge (which starts at width - 260), so they are written panel-relative and
        // stay inside the panel at any window width.  The scales are the original ones too
        // (1.0 for L / W, 0.5 for the layer line) -- these two are not part of the status
        // row's six cells, whose fields share a single scale among themselves.
        void drawRightReadouts()
        {
            char s[64];

            const float panelLeft = (float)gViewport[2] -
                                    (kPanelRightW + kPanelRightInset);
            const float height    = (float)gViewport[3];

            std::snprintf(s, sizeof(s), "L = %u  W = %u", EL, W_USED);

            hudText.RenderText(s,
                               panelLeft + 40.0f,
                               height - 95.0f,
                               1.0f,
                               glm::vec3(1.0f),
                               gViewport[2], gViewport[3]);

            if (!layerList || !textRenderer)
                return;

            std::snprintf(s, sizeof(s), "(Current layer = %u)",
                          layerList->getSelected());

            hudText.RenderText(s,
                               panelLeft + 70.0f,
                               height - 120.0f,
                               0.5f,
                               glm::vec3(1.0f),
                               gViewport[2], gViewport[3]);
        }

        // The cell boundaries: thin dividers in the side panels' border grey, running level
        // with the text.
        //
        // The two spaces are opposite and this is the trap: RenderText builds its own
        // projection with y upwards from the *bottom* of the window, while drawLine2D_new
        // draws through Renderer2D with ProjectionManager::get2DOrtho, i.e. y downwards
        // from the *top*.  So the line's extent is taken in the text's own space (the
        // ascender above the y passed to RenderText, the negative descender below it) and
        // each end is converted with t = height - y.  Measured with experiments\row_gauge.cpp:
        // at kStatusLineY = 80 and scale 1 the glyphs occupy 634.9 .. 661.7 counted from the
        // bottom, and this band comes out as 631.9 .. 664.7 of the same space.
        void drawStatusDividers(const StatusRow& row, float scale)
        {
            if (!row.valid())
                return;

            const glm::vec3 c(0.28f, 0.31f, 0.42f);

            const float ascender  = hudText.getAscenderPx() * scale;    // positive
            const float descender = hudText.getDescenderPx() * scale;   // negative
            const float pad       = 3.0f;
            const float height    = (float)gViewport[3];

            // In the text's space (y upwards from the bottom) the glyphs span
            // [row.y + descender, row.y + ascender]; the drawing space counts from the top.
            const float yTop    = height - (row.y + ascender + pad);    // nearer the top
            const float yBottom = height - (row.y + descender - pad);   // nearer the bottom

            const glm::mat4& P = ProjectionManager::instance().get2DOrtho();

            for (int cell = 1; cell < kStatusCells; ++cell)
            {
                const float x = row.cellLeft(cell);
                drawLine2D_new(x, yTop, x, yBottom, c, c, P);
            }
        }
    }

    // ------------------------------------------------------------------
    // The status row: the six displays, one font size, one frame
    // ------------------------------------------------------------------
    //
    // One function builds the whole row, because all of its fields are drawn at a *single*
    // scale: with a per-field fit the displays came out at different sizes (0.67 next to
    // 0.98 next to 1.0) and the line looked like it mixed fonts.  The scale is the smallest
    // the non-empty fields need -- never above 1.0, never below kStatusMinScale -- and a
    // field that still does not fit at that scale falls back to its bare value and then to
    // an ellipsis, so no display ever leaves its cell.
    void renderStatusRow()
    {
        if (gViewport[2] <= 0 || gViewport[3] <= 0)
            return;

        const StatusRow row = statusRow();

        if (!row.valid())
            return;

        const bool running = gConfig.simulation.scenario >= 0;

        std::string field[kStatusCells];
        std::string value[kStatusCells];   // bare value, the fallback of a labelled cell

        char s[64];

        // Compute runs on the GPU or on the CPU, never on both, so only the option in use
        // is shown (the cell used to print both words with the idle one dimmed).
        field[kCellCompute] = std::string("Compute: ") + (GPUEnabled ? "GPU" : "CPU");

        std::snprintf(s, sizeof(s), "Light: %llu", timer / automaton::FRAME);
        field[kCellLight] = s;

        // During replay `timer` counts replay frames, so there is no tick count to show and
        // the cell stays empty rather than carrying a number that means something else.
        if (currentMode != REPLAY)
        {
            std::snprintf(s, sizeof(s), "Tick: %llu", timer);
            field[kCellTick] = s;
        }

        // Scenario, Era and Mode belong to a run and are shown only when there is one --
        // the condition the three displays carried before they were folded into the row.
        if (running)
        {
            if (FRAME != 0)
            {
                const unsigned long long eraLen =
                    2ull * (unsigned long long)(RMAX > 0u ? RMAX : 1u);

                std::snprintf(s, sizeof(s), "Era: %llu",
                              timer / (unsigned long long)FRAME / eraLen + 1ull);
                field[kCellEra] = s;
            }

            if (gConfig.simulation.scenario <
                (int)splash::scenarioOptions.size())
            {
                value[kCellScenario] =
                    splash::scenarioOptions[gConfig.simulation.scenario];
                field[kCellScenario] = "Scenario: " + value[kCellScenario];
            }

            field[kCellMode] = std::string("Mode: ") +
                               (currentMode == REPLAY ? "Replay" : "Simulation");
        }

        // One font size for the whole row: the smallest the non-empty fields need.
        float scale = 1.0f;

        for (int cell = 0; cell < kStatusCells; ++cell)
        {
            if (field[cell].empty())
                continue;

            const float need = neededScale(field[cell], row.avail());

            if (need < scale)
                scale = need;
        }

        if (scale > 1.0f)
            scale = 1.0f;

        if (scale < kStatusMinScale)
            scale = kStatusMinScale;

        drawStatusDividers(row, scale);

        for (int cell = 0; cell < kStatusCells; ++cell)
        {
            if (!field[cell].empty())
                drawField(field[cell], value[cell], row, cell, scale, glm::vec3(1.0f));
        }

        // The two readouts that belong to the run, inside the right-hand panel.
        if (running)
            drawRightReadouts();
    }

    void renderLayers()
    {
        if (layerList) {
            layerList->render(hudText);
            layerList->update(hudText);
        }
    }

    void renderHelpText()
    {
        if (!showHelp)
            return;

        const float scale = 0.4f;
        int totalLines = (int)ui_help.size() + 1 + (int)record_help.size();
        int lineHeight = 18;
        int bottomMargin = 40;

        // Measure longest line to right-align block before the right panel
        float maxWidth = 0;
        for (auto& s : ui_help)
            maxWidth = std::max(maxWidth, hudText.measureTextWidth(s, scale));
        for (auto& s : record_help)
            maxWidth = std::max(maxWidth, hudText.measureTextWidth(s, scale));

        // Position so text block ends just before the right panel (screenW - 260)
        int rightX = gViewport[2] - 270 - (int)maxWidth;

        glm::vec3 color(0.6f);

        int line = 0;

        // UI help (keyboard shortcuts)
        for (size_t i = 0; i < ui_help.size(); ++i) {
            int y = bottomMargin + (totalLines - 1 - line) * lineHeight;
            hudText.RenderText(
                ui_help[i],
                (float)rightX,
                (float)y,
                scale,
                color,
                gViewport[2],
                gViewport[3]
            );
            line++;
        }

        line++; // blank separator

        // Record/replay help
        for (size_t i = 0; i < record_help.size(); ++i) {
            int y = bottomMargin + (totalLines - 1 - line) * lineHeight;
            hudText.RenderText(
                record_help[i],
                (float)rightX,
                (float)y,
                scale,
                color,
                gViewport[2],
                gViewport[3]
            );
            line++;
        }
    }

    void drawPanel(float x, float y,
                   float w, float h,
                   const glm::vec3& bgColor,
                   const glm::vec3& borderColor,
                   float borderThickness,
                   const glm::mat4& proj)
    {
        drawQuad2D(
            x, y,
            x + w, y + h,
            bgColor,
            proj
        );

        drawLine2D_new(
            x, y,
            x + w, y,
            borderColor,
            borderColor,
            proj
        );

        drawLine2D_new(
            x + w, y,
            x + w, y + h,
            borderColor,
            borderColor,
            proj
        );

        drawLine2D_new(
            x + w, y + h,
            x, y + h,
            borderColor,
            borderColor,
            proj
        );

        drawLine2D_new(
            x, y + h,
            x, y,
            borderColor,
            borderColor,
            proj
        );
    }

    void renderScenarioHelpPane()
    {
        if (gConfig.simulation.scenario < 0 ||
            gConfig.simulation.scenario >=
            (int)scenarioHelpTexts.size())
            return;

        const int paneX = 230;
        const int paneY = 150;
        const int paneW = 500;
        const int paneH = 300;

        const glm::mat4& proj =
            ProjectionManager::instance().get2DOrtho();

        drawPanel(
            (float)paneX,
            (float)paneY,
            (float)paneW,
            (float)paneH,
            glm::vec3(0.05f, 0.05f, 0.1f),
            glm::vec3(0.4f, 0.4f, 1.0f),
            2.0f,
            proj
        );

        std::istringstream iss(
            scenarioHelpTexts[
                gConfig.simulation.scenario
            ]
        );

        std::string line;

        int lineY = paneY + 40;

        while (std::getline(iss, line))
        {
            auto wrapped = wrapText(line, 60);

            for (const auto& wline : wrapped)
            {
                int flippedY =
                    windowHeight - lineY;

                hudText.RenderText(
                    wline,
                    (float)(paneX + 20),
                    (float)flippedY,
                    0.8f,
                    glm::vec3(1.0f),
                    windowWidth,
                    windowHeight
                );

                lineY += 25;
            }
        }
    }




  void drawRoundedRect(float x, float y, float w, float h, float radius)
  {
    const int seg = 20;
    const float PI = 3.14159265358979323846f;

    std::vector<glm::vec2> pts;
    pts.reserve(seg * 4 + 4);

    // TL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // TR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI / 2.0f;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // BR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    // BL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + 3.0f * PI / 2.0f;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    pts.push_back(pts[0]);

    const glm::mat4& proj =
      ProjectionManager::instance().get2DOrtho();

    drawTriangleFan2D(
      pts,
      glm::vec3(0.18f, 0.18f, 0.18f),
      proj
    );
  }

  void drawRoundedRectOutline(float x, float y,
                              float w, float h,
                              float radius)
  {
    const int seg = 20;
    const float PI = 3.14159265358979323846f;

    std::vector<glm::vec2> pts;
    pts.reserve(seg * 4 + 4);

    // TL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // TR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI / 2.0f;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + radius - radius * sinf(a)
      );
    }

    // BR
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + PI;
      pts.emplace_back(
        x + w - radius + radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    // BL
    for (int i = 0; i <= seg; ++i) {
      float a = (PI / 2.0f) * (float)i / seg + 3.0f * PI / 2.0f;
      pts.emplace_back(
        x + radius - radius * cosf(a),
        y + h - radius + radius * sinf(a)
      );
    }

    pts.push_back(pts[0]);

    const glm::mat4& proj =
      ProjectionManager::instance().get2DOrtho();

    drawLineLoop2D(
      pts,
      glm::vec3(0.45f, 0.45f, 0.45f),
      proj,
      1.0f
    );
  }

  void renderSliders()
  {
    if (tomoEnable && tomoEnable->getState())
      hslider.draw();

    vslider.draw();
  }

  void renderPauseOverlay()
  {
    if (!paused)
      return;

    int vw = gViewport[2];
    int vh = gViewport[3];

    const glm::mat4& proj =
      ProjectionManager::instance().get2DOrtho();

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float boxW = 200.0f;
    float boxH = 70.0f;

    float centerX = vw * 0.25f;
    float centerY_fromTop = vh * 0.25f;

    float boxTop    = centerY_fromTop - boxH * 0.5f;
    float boxBottom = centerY_fromTop + boxH * 0.5f;
    float boxLeft   = centerX - boxW * 0.5f;
    float boxRight  = centerX + boxW * 0.5f;

    drawQuad2D(
      boxLeft - 8.0f,
      boxTop - 8.0f,
      boxRight + 8.0f,
      boxBottom + 8.0f,
      glm::vec3(0.0f),
      proj
    );

    drawQuad2D(
      boxLeft - 4.0f,
      boxTop - 4.0f,
      boxRight + 4.0f,
      boxBottom + 4.0f,
      glm::vec3(0.4f, 0.6f, 1.0f),
      proj
    );

    drawQuad2D(
      boxLeft,
      boxTop,
      boxRight,
      boxBottom,
      glm::vec3(0.0f, 0.0f, 0.18f),
      proj
    );

    float textX = centerX - 50.0f;
    float textY = vh - centerY_fromTop - 9.0f;

    hudText.RenderText(
      "PAUSED",
      textX,
      textY,
      1.0f,
      glm::vec3(1.0f, 0.84f, 0.0f),
      vw,
      vh
    );
  }

  void renderSectionLabels()
  {
    glm::vec3 color(0.8f, 0.8f, 1.0f);

    if (textRenderer)
    {
      hudText.RenderText(
        "Data 3D",
        50.0f,
        gViewport[3] - d3Dpos + 15,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "Delays",
        50.0f,
        gViewport[3] - delaysPos + 15,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "View",
        50.0f,
        gViewport[3] - viewsPos + 20,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "Projection",
        50.0f,
        gViewport[3] - projPos + 20,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );

      hudText.RenderText(
        "Tomography",
        50.0f,
        gViewport[3] - tomoPos + 15,
        0.8f,
        color,
        gViewport[2],
        gViewport[3]
      );
    }
  }

} // namespace framework