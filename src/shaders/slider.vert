#version 460 core

layout (location = 0) in vec2 aPos;

// Uniforms for screen size (for projection)
uniform float u_screenWidth;
uniform float u_screenHeight;

void main()
{
    // Simple orthographic projection
    // Maps [0, u_screenWidth] x [0, u_screenHeight] to [-1, 1] x [-1, 1]
    float x_normalized = (aPos.x / u_screenWidth) * 2.0 - 1.0;
    float y_normalized = (aPos.y / u_screenHeight) * 2.0 - 1.0; 

    // OpenGL clip space Y is UP, so we flip the normalized Y
    gl_Position = vec4(x_normalized, y_normalized, 0.0, 1.0);
}