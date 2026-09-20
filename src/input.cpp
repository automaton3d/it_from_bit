// input.cpp - FIXED VERSION with smart unified callbacks
#include <glad/glad.h>  // MUST be first, before GLFW
#include <GLFW/glfw3.h>
#include "input.h"

// Forward declare GUI callbacks from framework namespace
namespace framework {
    void buttonCallback(GLFWwindow* window, int button, int action, int mods);
    void moveCallback(GLFWwindow* window, double xpos, double ypos);
    void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
}

// Mouse tracking state
struct MouseState {
    float lastX = 480.0f;
    float lastY = 270.0f;
    bool firstMouse = true;
    bool guiWantsInput = false;  // Track if GUI is using input
};

static MouseState mouseState;

// Helper function to check if mouse is over GUI elements
// You'll need to implement this based on your GUI system
static bool isMouseOverGUI(GLFWwindow* window, double xpos, double ypos) {
    // TODO: Check if mouse is over any GUI elements (buttons, sliders, etc.)
    // For now, return false to allow camera control
    // You might need to add a function in your GUI system that checks this
    return false;
}

// Unified cursor position callback - handles BOTH GUI and 3D camera
static void unified_cursor_callback(GLFWwindow* window, double xpos, double ypos) {
    AppContext* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));

    // Always let GUI track movement
    framework::moveCallback(window, xpos, ypos);

    // Only handle 3D camera if GUI doesn't want the input
    if (!ctx) return;

    // Check if mouse is over GUI - if so, don't move camera
    if (isMouseOverGUI(window, xpos, ypos)) {
        return;
    }

    if (mouseState.firstMouse) {
        mouseState.lastX = (float)xpos;
        mouseState.lastY = (float)ypos;
        mouseState.firstMouse = false;
        return;
    }

    float xoffset = (float)(xpos - mouseState.lastX);
    float yoffset = (float)(mouseState.lastY - ypos);
    mouseState.lastX = (float)xpos;
    mouseState.lastY = (float)ypos;

    // Only handle camera movement if middle mouse is pressed
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
            ctx->camera.ProcessMiddleMousePan(xoffset, yoffset);
        } else {
            ctx->camera.ProcessMiddleMouseOrbit(xoffset, yoffset);
        }
    }
}

// Unified scroll callback - handles BOTH GUI and 3D camera
static void unified_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    // Always let the framework handle scroll.
    // The framework's scrollCallback will decide whether to zoom the perspective camera
    // (via setScrollDirection) or adjust the orthographic scale (via ortho_scale).
    framework::scrollCallback(window, xoffset, yoffset);
}

// Unified mouse button callback
static void unified_button_callback(GLFWwindow* window, int button, int action, int mods) {
    // Always let GUI handle button clicks (for UI elements)
    framework::buttonCallback(window, button, action, mods);

    // Reset first mouse flag when middle button is pressed
    if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
        mouseState.firstMouse = true;
    }
}

// Unified key callback
static void unified_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    AppContext* ctx = static_cast<AppContext*>(glfwGetWindowUserPointer(window));

    // First, let GUI handle the key
    framework::keyCallback(window, key, scancode, action, mods);

    // Then handle camera-specific keys
    if (!ctx) return;

    /*
    // P to toggle projection (with debounce)
    if (key == GLFW_KEY_P && action == GLFW_PRESS && !ctx->pPressed) {
        ctx->camera.ToggleProjection();
        ctx->pPressed = true;
    }
    if (key == GLFW_KEY_P && action == GLFW_RELEASE) {
        ctx->pPressed = false;
    }
    */
}

void setupInputCallbacks(GLFWwindow* window, AppContext* ctx) {
    glfwSetWindowUserPointer(window, ctx);

    // Register UNIFIED callbacks that handle both GUI and 3D camera
    glfwSetCursorPosCallback(window, unified_cursor_callback);
    glfwSetScrollCallback(window, unified_scroll_callback);
    glfwSetMouseButtonCallback(window, unified_button_callback);
    glfwSetKeyCallback(window, unified_key_callback);
}

void processInput(GLFWwindow* window, AppContext& ctx) {
    // This function is now mostly redundant since keys are handled in callback
    // But keep it in case you need polling-based input
}
