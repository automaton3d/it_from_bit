#ifndef VOXEL_H
#define VOXEL_H

#include "shader.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

// ==========================================================
// Voxel Shaders
// ==========================================================

static const char* voxelVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

static const char* voxelFragmentShaderSource = R"(
#version 330 core
uniform vec4 voxelColor;
out vec4 FragColor;

void main()
{
    FragColor = voxelColor;
}
)";

// ==========================================================
// Color utilities
// ==========================================================

inline unsigned int makeColor(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

inline glm::vec4 unpackColor(unsigned int c)
{
    return glm::vec4(
        ((c >> 16) & 0xFF) / 255.0f,
        ((c >>  8) & 0xFF) / 255.0f,
        ( c        & 0xFF) / 255.0f,
        ((c >> 24) & 0xFF) / 255.0f
    );
}

// ==========================================================
// Cube geometry (non-indexed, 36 vertices)
// ==========================================================

static const float VOXEL_VERTICES[] = {
    // Back face
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    // Front face
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    // Left face
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    // Right face
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    // Bottom face
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,
    // Top face
    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f,  0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f
};

// ==========================================================
// VoxelMesh with cached uniform locations
// ==========================================================

struct VoxelMesh
{
    GLuint vao   = 0;
    GLuint vbo   = 0;
    GLuint shader = 0;

    // Cached uniform locations
    GLint locModel      = -1;
    GLint locView       = -1;
    GLint locProjection = -1;
    GLint locColor      = -1;
};

// Global mesh (declared extern in some .cpp if needed)
extern VoxelMesh voxelMesh;

// ==========================================================
// Initialize once at startup
// ==========================================================

inline void initVoxelMesh(VoxelMesh& mesh)
{
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);

    glBindVertexArray(mesh.vao);

    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VOXEL_VERTICES), VOXEL_VERTICES, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Compile shader
    mesh.shader = compileShader(voxelVertexShaderSource, voxelFragmentShaderSource);
    if (mesh.shader == 0) {
        throw std::runtime_error("Failed to compile voxel shader");
    }

    // Cache uniform locations
    mesh.locModel      = glGetUniformLocation(mesh.shader, "model");
    mesh.locView       = glGetUniformLocation(mesh.shader, "view");
    mesh.locProjection = glGetUniformLocation(mesh.shader, "projection");
    mesh.locColor      = glGetUniformLocation(mesh.shader, "voxelColor");
}

// ==========================================================
// Render one voxel
// ==========================================================

inline void renderVoxel(
    const VoxelMesh& mesh,
    const glm::mat4& model,
    const glm::mat4& view,
    const glm::mat4& projection,
    unsigned int packedColor)
{
    glUseProgram(mesh.shader);

    glUniformMatrix4fv(mesh.locModel,      1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(mesh.locView,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(mesh.locProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform4fv(mesh.locColor, 1, glm::value_ptr(unpackColor(packedColor)));

    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, 36);  // Draw 36 vertices (12 triangles)
}

// Cleanup function
inline void cleanupVoxelMesh(const VoxelMesh& mesh)
{
    if (mesh.vao)    glDeleteVertexArrays(1, &mesh.vao);
    if (mesh.vbo)    glDeleteBuffers(1, &mesh.vbo);
    if (mesh.shader) glDeleteProgram(mesh.shader);
}

#endif // VOXEL_H
