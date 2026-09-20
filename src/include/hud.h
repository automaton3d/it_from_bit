// hud.h
#pragma once
#include <string>
#include "button.h"
#include <atomic>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "app_context.h"

namespace framework
{
	extern unsigned tomo_x;
	extern unsigned tomo_y;
	extern unsigned tomo_z;
    extern std::atomic<bool> stopSimThread;
    extern bool showHelp;
    extern Button* aboutCloseButton;

    void updateProjection();
    bool initHUD(AppContext& ctx, const std::string& fontPath, int fontSize, unsigned int shader,
                 int screenW, int screenH);
    void renderHUD(int screenW, int screenH);
    void sizeCallback(GLFWwindow* window, int width, int height);

}
