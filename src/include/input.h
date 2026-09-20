// input.h
#pragma once
#include <GLFW/glfw3.h>
#include "app_context.h"

// Setup input callbacks for the window
void setupInputCallbacks(GLFWwindow* window, AppContext* ctx);

// Process keyboard input
void processInput(GLFWwindow* window, AppContext& ctx);
