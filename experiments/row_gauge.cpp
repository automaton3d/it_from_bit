// row_gauge.cpp -- measurement aid, not part of the simulator.
//
// The status row draws its text with TextRenderer (which builds its own projection inside
// RenderText: y upwards from the *bottom* of the window) and its dividers with
// drawLine2D_new (the Renderer2D pipeline, whose projection comes from
// ProjectionManager::get2DOrtho: y downwards from the *top*).  Guessing how the two line
// up put the dividers under the text twice, so this program measures instead: it renders
// the text and one marker line per candidate y into a hidden window, reads the
// framebuffer back and prints the rows that carry ink.  glReadPixels returns the bottom
// row first, so the row index *is* the distance from the bottom edge -- the same units the
// text renderer works in.
//
// Build (from the repository root, with the repository's own flags):
//   experiments\row_gauge_build.bat        (cl line and log: build\row_gauge_build.txt)
// Run from the repository root (fonts/arial.ttf is resolved relative to it).

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "text_renderer.h"
#include "draw_utils.h"
#include "Renderer2D.h"
#include "projection_manager.h"
#include "shader.h"
#include "globals.h"
#include "GUI.h"

// The simulator defines this in src\globals.cpp, which pulls in the renderer; the gauge
// supplies it itself instead of linking the whole application.
GLint gViewport[4] = {0, 0, 0, 0};

namespace
{
    constexpr int kWidth  = 1280;
    constexpr int kHeight = 720;

    struct Extent
    {
        int lo = -1;   // rows carrying ink, counted from the bottom edge
        int hi = -1;
        int count = 0;
    };

    Extent inkRows(const std::vector<unsigned char>& px, int x0, int x1)
    {
        Extent e;

        for (int y = 0; y < kHeight; ++y)
        {
            for (int x = x0; x <= x1; ++x)
            {
                const unsigned char* p = &px[(size_t)(y * kWidth + x) * 4u];

                if (p[0] > 40 || p[1] > 40 || p[2] > 40)
                {
                    if (e.lo < 0)
                        e.lo = y;

                    e.hi = y;
                    ++e.count;
                    break;      // one hit per row is enough for the extent
                }
            }
        }

        return e;
    }

    void report(const char* what, const Extent& e)
    {
        if (e.lo < 0)
            std::printf("# %-34s : no ink\n", what);
        else
            std::printf("# %-34s : rows %d .. %d  (%d rows with ink)\n",
                        what, e.lo, e.hi, e.count);
    }
}

int main()
{
    if (!glfwInit())
    {
        std::printf("# glfwInit failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* win = glfwCreateWindow(kWidth, kHeight, "row gauge", nullptr, nullptr);

    if (!win)
    {
        std::printf("# hidden window failed\n");
        return 1;
    }

    glfwMakeContextCurrent(win);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::printf("# glad failed\n");
        return 1;
    }

    ProjectionManager::instance().setViewport(kWidth, kHeight);
    gViewport[0] = 0;
    gViewport[1] = 0;
    gViewport[2] = kWidth;
    gViewport[3] = kHeight;

    Renderer2D::init();

    const unsigned int textShader = compileTextShader();

    if (!textShader)
    {
        std::printf("# text shader failed\n");
        return 1;
    }

    TextRenderer hud;

    if (!hud.init("fonts/arial.ttf", 24, textShader))
    {
        std::printf("# font failed\n");
        return 1;
    }

    std::printf("# window %dx%d  ascender=%.2f  descender=%.2f  fontSize=%d\n",
                kWidth, kHeight, hud.getAscenderPx(), hud.getDescenderPx(),
                hud.getFontSizePx());

    for (float scale : {1.0f, 0.5f})
    {
        const float textY = (float)kHeight - framework::kStatusLineY;  // as the HUD sets it
        const float asc   = hud.getAscenderPx() * scale;
        const float desc  = hud.getDescenderPx() * scale;              // negative

        // The band drawStatusDividers() uses now: the glyph extent taken in the *text's*
        // space (y upwards from the bottom) and each end converted to the drawing space
        // (y downwards from the top) with t = height - y.
        const float yTop    = (float)kHeight - (textY + asc + 3.0f);
        const float yBottom = (float)kHeight - (textY + desc - 3.0f);

        // The band the previous (wrong) version used, kept as a reference:
        // t = row.y -+ the metrics, i.e. mirrored to the other end of the window.
        const float yTopOld    = textY - asc - 3.0f;
        const float yBottomOld = textY - desc + 3.0f;

        glViewport(0, 0, kWidth, kHeight);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // The text, exactly as the row draws it: RenderText(text, x, row.y, scale, ...).
        hud.RenderText("Era: 12", 100.0f, textY, scale, glm::vec3(1.0f),
                       kWidth, kHeight);

        const glm::mat4& P = ProjectionManager::instance().get2DOrtho();
        Renderer2D::use();
        Renderer2D::setMVP(P);

        const float marks[] = { yTop, yBottom, yTopOld, yBottomOld, textY };

        for (int i = 0; i < 5; ++i)
        {
            const float x = 300.0f + 60.0f * (float)i;
            const glm::vec3 c(0.0f, 0.0f, 1.0f);
            drawLine2D_new(x, marks[i], x + 30.0f, marks[i], c, c, P);
        }

        glFinish();

        std::vector<unsigned char> px((size_t)kWidth * kHeight * 4u, 0);
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

        std::printf("\n# ---- scale %.2f : text y = %.2f (text space, from the bottom), "
                    "ascender %.2f, descender %.2f\n", scale, textY, asc, desc);

        const Extent text = inkRows(px, 100, 260);
        report("text ink", text);
        std::printf("#   (text ink in the text's own units: %.1f .. %.1f)\n",
                    (double)textY + desc, (double)textY + asc);

        const char* names[] = { "divider top   (fixed)", "divider bottom(fixed)",
                                "divider top   (old)  ", "divider bot   (old)  ",
                                "text y itself        " };

        for (int i = 0; i < 5; ++i)
        {
            char label[64];
            std::snprintf(label, sizeof(label), "%s y=%.1f", names[i], marks[i]);
            report(label, inkRows(px, 300 + 60 * i, 330 + 60 * i));
        }
    }

    glfwTerminate();
    return 0;
}
