// splash.h - Single-window version (2025 fixed)
#pragma once

#include <GLFW/glfw3.h>

// Forward declaration — this is the only function main.cpp should call
int runSplash(GLFWwindow* existingWindow);
void display();

/* -------------------------------------------------------------
   Public data that main.cpp needs after splash screen
   ------------------------------------------------------------- */
namespace splash {
    // User-selected lattice parameters
    extern int lattice_size;
    extern int numLayers;

    extern std::vector<std::string> scenarioOptions;

    // Selected scenario index.  The model currently defines a SINGLE scenario
    // (the full simulation dynamics); the list is kept extensible so future
    // scenarios can be appended to scenarioOptions / scenarioHelpTexts.
    inline int getSelectedScenario() {
        extern int getSelectedScenarioImpl(); // implemented in splash.cpp
        return getSelectedScenarioImpl();
    }

    // Whether "Start Paused" was checked
    inline bool getStartPaused() {
        extern bool getStartPausedImpl();
        return getStartPausedImpl();
    }

    // Call this if you want to reset to defaults (optional)
    void resetDefaults();


    int initialize(GLFWwindow* window);
    void cleanup();
}
