// scene.h
#pragma once
#include "app_context.h"

// Constants for scene geometry
namespace SceneConstants {
    constexpr int GRID_SIZE = 10;
    constexpr float GRID_COLOR = 0.5f;
    constexpr float AXIS_LENGTH = 5.0f;
    constexpr float AXIS_LINE_WIDTH = 3.0f;
    constexpr float CUBE_SCALE = 2.0f;
    constexpr float CUBE_HEIGHT = 1.0f;
    constexpr float CUBE_ROTATION_SPEED = 0.5f;
}

// Initialize all scene geometry
void initScene(AppContext& ctx);

// Render the scene
void renderScene(AppContext& ctx);

// Cleanup scene resources
void cleanupScene(AppContext& ctx);
