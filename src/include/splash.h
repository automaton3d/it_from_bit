// splash.h - single-window splash / setup screen
//
// The splash runs inside the GLFW window created by main.cpp and owns the
// initial configuration of a run: lattice side (L), winding layers (W),
// scenario and "start paused".  main.cpp drives it through initialize() /
// render() / cleanup(); the selection itself is read from the widgets inside
// splash.cpp (one single reader, shared by the three buttons and by the Enter
// shortcut) and persisted through saveConfig() (see config.h).
//
// Coordinate conventions of this GUI, which are a standing trap when editing
// layouts here:
//   * 2D drawing through Renderer2D / drawQuad2D uses ProjectionManager's 2D
//     ortho with the origin at the TOP-LEFT (y grows downwards);
//   * TextRenderer::RenderText builds its own ortho with the origin at the
//     BOTTOM-LEFT, so every text baseline is mirrored before it is drawn;
//   * Cortina::isMouseOver and Button::contains expect a BOTTOM-UP mouse
//     position (winH() - glfwY), which is what the callbacks pass.
#pragma once

#include <string>
#include <vector>

#include <GLFW/glfw3.h>

/* -------------------------------------------------------------
   Public data the rest of the program may read after the splash
   ------------------------------------------------------------- */
namespace splash {
    // Selection that was last applied (kept in sync by launch()).
    extern int lattice_size;   // L
    extern int numLayers;      // W

    extern std::vector<std::string> scenarioOptions;

    // Life cycle, driven by main.cpp.
    int  initialize(GLFWwindow* window);
    void render();
    void cleanup();

    // Removed here: runSplash(), display(), resetDefaults(), getSelectedScenario()
    // and getStartPaused().  They were declared (the last two as inline wrappers
    // around getSelectedScenarioImpl()/getStartPausedImpl()) but had no
    // definition in any translation unit and no caller anywhere, so they were
    // a link error waiting to happen.
}
