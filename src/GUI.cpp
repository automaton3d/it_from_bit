/*
 * GUI.cpp
 *
 * Implements the GUI renderer orchestration logic.
 */

#include "GUI.h"

#include "globals.h"
#include "hslider.h"
#include "vslider.h"
#include "tickbox.h"
#include "replay_progress.h"
#include "progress.h"
#include "logo.h"
#include "text_renderer.h"
#include "dropdown.h"
#include "projection.h"
#include "projection_manager.h"
#include "radio.h"
#include "layers.h"
#include "model/simulation.h"
#include "menubar.h"
#include "help.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <atomic>

namespace framework
{
    bool showAboutDialog = false;

    extern HSlider hslider;
    extern ReplayProgressBar* replayProgress;
    extern int windowWidth;
    extern int windowHeight;

    std::atomic<bool> replayFrames{false};

    // ------------------------------------------------------------
    // Resize / viewport update
    // ------------------------------------------------------------
    void resize(int width, int height)
    {
        if (height == 0)
            height = 1;

        float ratio =
            static_cast<float>(width) /
            static_cast<float>(height);

        // --------------------------------------------------------
        // OpenGL viewport
        // --------------------------------------------------------
        glViewport(0, 0, width, height);

        // --------------------------------------------------------
        // Global viewport state
        // --------------------------------------------------------
        gViewport[0] = 0;
        gViewport[1] = 0;
        gViewport[2] = width;
        gViewport[3] = height;

        // --------------------------------------------------------
        // Framework dimensions
        // --------------------------------------------------------
        windowWidth  = width;
        windowHeight = height;

        // --------------------------------------------------------
        // Projection matrices
        // --------------------------------------------------------

        // Top-down orthographic projection
        mOrtho = glm::ortho(
            0.0f,
            (float)width,
            (float)height,
            0.0f
        );

        // Perspective projection
        mPerspective = glm::perspective(
            glm::radians(45.0f),
            ratio,
            0.01f,
            100.0f
        );

        // --------------------------------------------------------
        // Synchronize ProjectionManager
        // --------------------------------------------------------
        ProjectionManager::instance().setViewport(
            width,
            height
        );

        // --------------------------------------------------------
        // Resize widgets
        // --------------------------------------------------------
        if (progress)
        {
            progress->resize(width, height);
        }

        if (replayProgress)
        {
            replayProgress->setPosition(
                width,
                height,
                height - 100
            );
        }

        if (menuBar)
        {
            menuBar->Resize(
                static_cast<float>(width),
                static_cast<float>(height)
            );
        }

        // HSlider has no resize()
        hslider.recenter(width);

        // --------------------------------------------------------
        // Update dependent projection state
        // --------------------------------------------------------
        updateProjection();
    }

    // ------------------------------------------------------------
    // Clear frame
    // ------------------------------------------------------------
    void renderClear()
    {
        glClearColor(
            0.0f,
            0.0f,
            0.0f,
            0.0f
        );

        glClearDepth(1.0f);

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );
    }

    // ------------------------------------------------------------
    // Request app exit
    // ------------------------------------------------------------
    void requestExit()
    {
        // framework::CAWindow::instance().pendingExit = true;
    }

} // namespace framework