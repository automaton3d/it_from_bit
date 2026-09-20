// hud.cpp
#include "hud.h"
#include "text_renderer.h"
#include "shader.h"

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <iostream>
#include <sstream>
#include <string>

#include "GUI.h"
#include "globals.h"
#include "logo.h"
#include "replay_progress.h"
#include "help.h"
#include "projection_manager.h"
#include "tomography.h"
#include "Renderer2D.h"
#include "draw_utils.h"
#include "sinc_overlay.h"
#include <vector>
#include <cmath>

extern int textureSamplerLoc;

namespace framework {

TextRenderer hudText;

extern bool showAboutDialog;

double tbegin = 0;

std::unique_ptr<LayerList> layerList;

Logo* logo = nullptr;
ReplayProgressBar* replayProgress = nullptr;
ProgressBar* progress = nullptr;

Button* aboutCloseButton = nullptr;

int windowWidth  = 800;
int windowHeight = 600;

bool helpHover = false;
bool showHelp  = false;

int barWidths[3] = {0, 0, 0};

extern Tickbox* scenarioHelpToggle;

// ------------------------------------------------------------
// 2D projection helper
// ------------------------------------------------------------
const glm::mat4& proj2D()
{
    return ProjectionManager::instance().get2DOrtho();
}

// ------------------------------------------------------------
// Init HUD
// ------------------------------------------------------------
bool initHUD(AppContext& ctx,
             const std::string& fontPath,
             int fontSize,
             unsigned int shader,
             int screenW,
             int screenH)
{
    if (!hudText.init(fontPath, fontSize, shader))
    {
        std::cerr << "Failed to initialize HUD text renderer\n";
        return false;
    }

    initializeWidgets(ctx);

    tbegin = glfwGetTime();

    logo = new Logo(std::string("logo_bar.png"));

    gHelpLink = new Button(
        0.0f, 0.0f,
        200.0f, 40.0f,
        "Help",
        false
    );

    aboutCloseButton = new Button(
        0.0f,
        0.0f,
        200.0f,
        60.0f,
        "Close",
        true
    );

    return true;
}

// ------------------------------------------------------------
// Render elapsed time
// ------------------------------------------------------------
void renderElapsedTime()
{
    double now = glfwGetTime();
    double seconds = now - tbegin;

    char buf[64];

    snprintf(buf, sizeof(buf),
             "Elapsed %.1f s",
             seconds);

    hudText.RenderText(
        buf,
        20.0f,
        40.0f,
        0.25f,
        glm::vec3(1.0f)
    );
}

void renderAboutDialog()
{
    if (!showAboutDialog) return;

    const int paneW = 520;
    const int paneH = 320;
    const int paneX = (windowWidth  - paneW) / 2;
    const int paneY = (windowHeight - paneH) / 2;

    const glm::mat4& proj = proj2D();

    drawPanel((float)paneX, (float)paneY, (float)paneW, (float)paneH,
              glm::vec3(0.05f, 0.05f, 0.1f), glm::vec3(0.4f, 0.4f, 1.0f), 2.0f, proj);

    // About text
    const std::string about = "Cellular Automaton Visualizer\n\nVersion 1.0\n\n"
        "Real-time 3D cellular automaton\nwith recording, replay & tomography\n\n"
        "Built with OpenGL + GLFW\n(c) 2025";

    std::istringstream iss(about);
    std::string line;
    int lineY = paneY + 40;

    while (std::getline(iss, line))
    {
        for (const auto& wline : wrapText(line, 48))
        {
            hudText.RenderText(wline, (float)(paneX + 20), 
                               (float)(windowHeight - lineY), 0.7f, 
                               glm::vec3(1.0f), windowWidth, windowHeight);
            lineY += 24;
        }
    }

    // Close button — drawn inline (without Button::draw)
    const float btnW = 150.0f, btnH = 42.0f;
    float btnX = paneX + (paneW - btnW) / 2.0f;
    float btnY = paneY + paneH - 75.0f;

    // Button background + border
    drawPanel(btnX, btnY, btnW, btnH,
              glm::vec3(0.2f, 0.4f, 0.8f),
              glm::vec3(1.0f, 1.0f, 1.0f),
              2.0f, proj);

    // Centered "Close" text (scale 0.7, same as About text)
    float closeTxtScale = 0.7f;
    float closeTxtW = hudText.measureTextWidth("Close", closeTxtScale);
    float closeTxtX = btnX + (btnW - closeTxtW) * 0.5f;
    float closeTxtY = windowHeight - (btnY + btnH * 0.5f) - 8.0f;

    hudText.RenderText("Close", closeTxtX, closeTxtY, closeTxtScale,
                       glm::vec3(1.0f), windowWidth, windowHeight);

    // Click — GLFW gives mouseY top-down; contains() expects bottom-up
    double mx_d, my_d;
    glfwGetCursorPos(glfwGetCurrentContext(), &mx_d, &my_d);
    int mx = (int)mx_d, my = (int)my_d;

    // Manual hit-test (top-down coords directly)
    bool insideX = mx >= btnX && mx <= (btnX + btnW);
    bool insideY = my >= btnY && my <= (btnY + btnH);

    if (insideX && insideY &&
        glfwGetMouseButton(glfwGetCurrentContext(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        showAboutDialog = false;
    }

    // Enter
    static bool enterWasPressed = false;
    bool enterNow = (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_ENTER) == GLFW_PRESS) ||
                    (glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_KP_ENTER) == GLFW_PRESS);
    if (enterNow && !enterWasPressed) showAboutDialog = false;
    enterWasPressed = enterNow;
}

// ------------------------------------------------------------
// Sinc / r·sin(r) overlay (bottom-left of the 3D view)
// ------------------------------------------------------------
static void renderSincOverlay(int screenW, int screenH)
{
    (void)screenW;

    if (!sinc_overlay::ready())
        return;

    const std::vector<float>& u  = sinc_overlay::profile();
    const std::vector<float>& tr = sinc_overlay::triggerRate();
    const std::vector<float>& ph = sinc_overlay::peakHistory();
    const std::vector<float>& am = sinc_overlay::andMask();

    const unsigned graphSize = sinc_overlay::graphSize();
    if (u.empty() || graphSize == 0)
        return;

    const glm::mat4& P = proj2D();

    const float overlayW = 250.0f;
    const float graphH   = 100.0f;
    const float ox       = 230.0f;                         // left x position
    const float oy       = (float)screenH - 30.0f;          // baseline at bottom-left
    const float top      = oy - graphH;

    const float scaleX = (graphSize > 1)
                            ? overlayW / (float)(graphSize - 1)
                            : overlayW;

    // Dark background panel
    drawQuad2D(ox - 4.0f, top - 4.0f, ox + overlayW + 4.0f, oy + 4.0f,
               glm::vec3(0.06f, 0.06f, 0.09f), P);

    // Axes
    drawLine2D_new(ox, oy, ox + overlayW, oy,
                   glm::vec3(0.35f), glm::vec3(0.35f), P);
    drawLine2D_new(ox, top, ox, oy,
                   glm::vec3(0.35f), glm::vec3(0.35f), P);

    // Green: radial sinc(r) displacement profile
    {
        std::vector<glm::vec2> pts;
        pts.reserve(graphSize);
        for (unsigned r = 0; r < graphSize; ++r)
        {
            float x = ox + (float)r * scaleX;
            float y = oy - u[r] * graphH;
            if (y < top) y = top;
            if (y > oy) y = oy;
            pts.emplace_back(x, y);
        }
        drawLineStrip2D(pts, glm::vec3(0.0f, 0.85f, 0.3f), P, 1.5f);
    }

    // Cyan: trigger rate (profile / u_peak) after convergence
    if (!tr.empty())
    {
        std::vector<glm::vec2> pts;
        pts.reserve(graphSize);
        for (unsigned r = 0; r < graphSize; ++r)
        {
            float x = ox + (float)r * scaleX;
            float y = oy - tr[r] * graphH;
            if (y < top) y = top;
            if (y > oy) y = oy;
            pts.emplace_back(x, y);
        }
        drawLineStrip2D(pts, glm::vec3(0.0f, 0.78f, 1.0f), P, 1.5f);
    }

    // Yellow: peak-history time series
    if (!ph.empty())
    {
        const float scaleX2 = (ph.size() > 1)
                                ? overlayW / (float)(ph.size() - 1)
                                : overlayW;
        std::vector<glm::vec2> pts;
        pts.reserve(ph.size());
        for (size_t i = 0; i < ph.size(); ++i)
        {
            float x = ox + (float)i * scaleX2;
            float y = oy - ph[i] * graphH;
            if (y < top) y = top;
            if (y > oy) y = oy;
            pts.emplace_back(x, y);
        }
        drawLineStrip2D(pts, glm::vec3(1.0f, 0.9f, 0.0f), P, 1.5f);
    }

    // Red: and_count[r] scatter (triggered && active) per shell radius
    {
        glm::vec3 red(0.9f, 0.2f, 0.2f);
        for (unsigned r = 0; r < graphSize; ++r)
        {
            if (am[r] <= 0.001f)
                continue;
            float x = ox + (float)r * scaleX;
            float y = oy - am[r] * graphH;
            drawQuad2D(x - 1.5f, y - 1.5f, x + 1.5f, y + 1.5f, red, P);
        }
    }

    // Gray vertical line at the current pulse radius
    unsigned pr = sinc_overlay::currentRadius();
    if (pr < graphSize)
    {
        float x = ox + (float)pr * scaleX;
        drawLine2D_new(x, top, x, oy,
                       glm::vec3(0.35f), glm::vec3(0.35f), P);
    }
}

// ------------------------------------------------------------
// Render HUD
// ------------------------------------------------------------
void renderHUD(int screenW, int screenH)
{
    windowWidth  = screenW;
    windowHeight = screenH;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const glm::mat4& P = proj2D();

    // --------------------------------------------------------
    // Setup texture shader
    // --------------------------------------------------------
    glUseProgram(textureProgram2D);

    glUniformMatrix4fv(
        textureMvpLoc,
        1,
        GL_FALSE,
        &P[0][0]
    );

    // --------------------------------------------------------
    // Restore Renderer2D pipeline
    // --------------------------------------------------------
    Renderer2D::use();
    Renderer2D::setMVP(P);

    // --------------------------------------------------------
    // Dialogs
    // --------------------------------------------------------
    renderAboutDialog();

    // === SIDE PANELS - DRAWN BEFORE EVERYTHING ELSE ===
    const int panelPadding = 60;
    const int bottomMargin = 170;

    int leftH  = screenH - bottomMargin;
    int rightH = screenH - bottomMargin;

    glm::vec3 panelBg       = glm::vec3(0.042f, 0.042f, 0.068f);   // slightly deeper
    glm::vec3 panelBorder   = glm::vec3(0.28f, 0.31f, 0.42f);      // refined border
    glm::vec3 panelBorderHL = glm::vec3(0.48f, 0.52f, 0.65f);      // highlight sutil (opcional)

    // Left panel
    drawPanel(35, panelPadding, 170, leftH, panelBg, panelBorder, 2.0f, P);

    // Right panel
    drawPanel(screenW - 260, panelPadding, 250, rightH, panelBg, panelBorder, 2.0f, P);
                
    if (scenarioHelpToggle &&
        scenarioHelpToggle->getState())
    {
        renderScenarioHelpPane();
    }

    // --------------------------------------------------------
    // Time
    // --------------------------------------------------------
    renderElapsedTime();

    // --------------------------------------------------------
    // Logo
    // --------------------------------------------------------
    if (logo && logo->valid())
    {
        logo->draw(
            screenW - 250,
            screenH - 350,
            1.0f
        );
    }

    // --------------------------------------------------------
    // Widgets
    // --------------------------------------------------------
    renderSectionLabels();

    render3Dboxes();

    renderDelays();

    renderViewpointRadios();

    renderProjectionRadios();

    renderTomoControls();

    renderSineVisitedToggle(screenW, screenH);

    renderTomoRadios();

    renderSimulationStats();

    renderEra();

    renderComputeStats();

    renderLayerInfo();

    renderLayers();

    renderHelpText();

    renderSliders();

    tomography::renderControls();

    renderPauseOverlay();

    renderHyperlink();

    renderScenarioHelpToggle();

    renderGizmo();

    // --------------------------------------------------------
    // Sinc / r·sin(r) overlay
    // --------------------------------------------------------
    renderSincOverlay(screenW, screenH);

    // --------------------------------------------------------
    // Progress
    // --------------------------------------------------------
    if (currentMode == SIMULATION)
    {
        if (gConfig.simulation.scenario >= 0 &&
            progress)
        {
            unsigned long long safeTimer;

            {
                std::lock_guard<std::mutex> lock(timerMutex);
                safeTimer = timer;
            }

            if (progress->getWidth() > 0)
            {
                progress->resize(screenW, screenH);

                progress->update(
                    safeTimer,
                    automaton::FRAME
                );

                progress->draw(
                    safeTimer,
                    automaton::FRAME,
                    screenW,
                    screenH
                );
            }
        }
    }

    // --------------------------------------------------------
    // Restore state
    // --------------------------------------------------------
    glDepthMask(GL_TRUE);

    glEnable(GL_DEPTH_TEST);
}

// ------------------------------------------------------------
// Hyperlink
// ------------------------------------------------------------
void renderHyperlink()
{
    if (!gHelpLink || !textRenderer)
        return;

    int w = gViewport[2];
    int h = gViewport[3];

    if (w <= 0 || h <= 0)
        return;

    // Reposition at bottom-center each frame
    const float linkW = 100.0f;
    const float linkH = 30.0f;
    float linkX = (w - linkW) / 2.0f;
    float linkY = h - 45.0f;

    gHelpLink->setPosition(linkX, linkY);
    gHelpLink->setSize(linkW, linkH);

    gHelpLink->drawAsHyperlink(*textRenderer, helpHover, w, h);
}

// ------------------------------------------------------------
// Cleanup
// ------------------------------------------------------------
void cleanup()
{
    delete gHelpLink;
    gHelpLink = nullptr;
    delete aboutCloseButton;
    aboutCloseButton = nullptr;
}

} // namespace framework