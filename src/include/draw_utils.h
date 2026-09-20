// draw_utils.h

#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

void drawLine2D_new(
    float x1, float y1,
    float x2, float y2,
    const glm::vec3& c1,
    const glm::vec3& c2,
    const glm::mat4& mvp);

// Break a long string into multiple lines of max 'width' characters
std::vector<std::string> wrapText(const std::string& text, size_t width);

// ===================================================================
// MODERN 2D DRAWING – use these everywhere
// ===================================================================

// In draw_utils.h – add these declarations:
void drawQuad3D(const glm::vec3& v0, const glm::vec3& v1,
                const glm::vec3& v2, const glm::vec3& v3,
                const glm::vec3& color,
                const glm::mat4& mvp,
                float alpha = 1.0f);

void drawPoints3D(const std::vector<glm::vec3>& points,
                  const glm::vec3& color,
                  const glm::mat4& mvp,
                  float size = 3.0f);

// Filled rectangle
void drawQuad2D(float x1, float y1, float x2, float y2,
                const glm::vec3& color,
                const glm::mat4& proj);

// Closed outline (polygon border)
void drawLineLoop2D(const std::vector<glm::vec2>& points,
                    const glm::vec3& color,
                    const glm::mat4& proj,
                    float thickness = 1.0f);

// Filled polygon / circle / pie
void drawTriangleFan2D(const std::vector<glm::vec2>& points,
                       const glm::vec3& color,
                       const glm::mat4& proj);

// Single line (thin, always works)
void drawLine2D(float x1, float y1, float x2, float y2,
                const glm::vec3& color,
                const glm::mat4& proj,
                float thickness = 1.0f);

// Polyline (connected line segments)
void drawLineStrip2D(const std::vector<glm::vec2>& points,
                     const glm::vec3& color,
                     const glm::mat4& proj,
                     float thickness = 1.0f);

// Optional: clean shutdown (call on exit if you like)
void draw2DShutdown();

// ===================================================================
// LEGACY OVERLOADS – remove when no longer needed
// ===================================================================

void drawTriangle2D(const std::vector<glm::vec2>& verts,
                    const glm::vec3& color,
                    int winW, int winH);

void drawTriangleFan2D(const std::vector<glm::vec2>& verts,
                       const glm::vec3& color,
                       int winW, int winH);

void drawLineLoop2D(const std::vector<glm::vec2>& points,
                    const glm::vec3& color,
                    int winW, int winH,
                    float thickness = 1.0f);

void drawQuads(
    const std::vector<glm::vec3>& verts,
    const glm::vec3& color,
    const glm::mat4& mvp);