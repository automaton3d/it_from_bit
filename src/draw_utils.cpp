#include "draw_utils.h"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <sstream>

#include "render_pipeline.h"
#include "Renderer2D.h"
#include "projection_manager.h"

// External shader (should already exist in the project)
extern GLint uProjLoc;
// Removed 2026-09-20: `extern GLint uColorLoc;` here and its definition in
// globals.cpp.  It was never assigned (so it stayed -1) and the only user was
// drawTriangleFan2D, which now sets the colour through Renderer2D::setColor.

static GLuint vao = 0;
static GLuint vbo = 0;

static void init()
{
    if (vao) return;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                          sizeof(glm::vec2), (void*)0);

    glBindVertexArray(0);
}

static void upload(const std::vector<glm::vec2>& verts)
{
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(glm::vec2),
                 verts.data(),
                 GL_DYNAMIC_DRAW);
}

// =========================
// TRIANGLE FAN
// =========================
void drawTriangleFan2D(
    const std::vector<glm::vec2>& verts,
    const glm::vec3& color,
    const glm::mat4& proj)
{
    if (verts.empty()) return;

    init();

    Renderer2D::use();
    Renderer2D::setMVP(
        ProjectionManager::instance().get2DOrtho()
    );    
    // The colour argument used to be sent to glUniform3fv(uColorLoc, ...), but
    // uColorLoc was never assigned anywhere (it stayed -1, and -1 is a silent
    // no-op), so every fan in the project -- radio fills, the gizmo cube, the
    // origin dot, the subregion faces -- was drawn with whatever colour the
    // last drawQuad2D had left in the Renderer2D program.  setColor() is the
    // uniform drawQuad2D itself uses.
    Renderer2D::setColor(color);

    glBindVertexArray(vao);
    upload(verts);

    glDrawArrays(GL_TRIANGLE_FAN, 0, (GLsizei)verts.size());

    glBindVertexArray(0);
}

// =========================
// LINE LOOP
// =========================
void drawLineLoop2D(
    const std::vector<glm::vec2>& pts,
    const glm::vec3& color,
    const glm::mat4& proj,
    float thickness)
{
    if (pts.empty()) return;

    init();

    // Bind the 2D program, matrix and colour.  Without a program the draw call
    // is invalid in a core profile, so every outline was silently dropped: the
    // tick-boxes showed a filled square with no check mark and no border.  The
    // `proj` argument is kept for compatibility; the 2D ortho is the only
    // projection these vertices belong to.
    (void)proj;
    Renderer2D::use();
    Renderer2D::setMVP(ProjectionManager::instance().get2DOrtho());
    Renderer2D::setColor(color);

    glLineWidth(thickness);
    glBindVertexArray(vao);
    upload(pts);
    glDrawArrays(GL_LINE_LOOP, 0, (GLsizei)pts.size());
    glBindVertexArray(0);
}

// Outlined rectangle in the usual 2D space (origin top-left).  Reuses the
// static VAO/VBO above, so calling it every frame is free of GL object churn.
void drawRectOutline2D(float x1, float y1, float x2, float y2,
                       const glm::vec3& color, float thickness)
{
    drawLineLoop2D({ {x1, y1}, {x2, y1}, {x2, y2}, {x1, y2} },
                   color,
                   ProjectionManager::instance().get2DOrtho(),
                   thickness);
}

void drawLineStrip2D(
    const std::vector<glm::vec2>& pts,
    const glm::vec3& color,
    const glm::mat4& proj,
    float thickness)
{
    if (pts.size() < 2) return;
    init();
    Renderer2D::use();
    Renderer2D::setMVP(ProjectionManager::instance().get2DOrtho());
    Renderer2D::setColor(color);
    glLineWidth(thickness);
    glBindVertexArray(vao);
    upload(pts);
    glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)pts.size());
    glBindVertexArray(0);
}

void drawLine2D_new(
    float x1, float y1,
    float x2, float y2,
    const glm::vec3& c1,
    const glm::vec3& c2,
    const glm::mat4& mvp)
{
    (void)c2;

    // Two vec3 positions need their own VAO/VBO: the attribute layout lives in
    // the VAO, and the 2-float helpers above share a different one.  Created on
    // first use and kept afterwards -- this function is called many times per
    // frame (axes, arrows, HUD) and used to create/delete a pair per call.
    static GLuint vao3 = 0, vbo3 = 0;
    if (!vao3)
    {
        glGenVertexArrays(1, &vao3);
        glGenBuffers(1, &vbo3);
        glBindVertexArray(vao3);
        glBindBuffer(GL_ARRAY_BUFFER, vbo3);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    const float data[6] = { x1, y1, 0.0f, x2, y2, 0.0f };

    Renderer2D::use();
    Renderer2D::setMVP(mvp);

    // uses only ONE uniform color
    Renderer2D::setColor(c1);

    glBindVertexArray(vao3);
    glBindBuffer(GL_ARRAY_BUFFER, vbo3);
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, 2);
    glBindVertexArray(0);
}

// Note: x1,y1 is the top-left corner and x2,y2 the bottom-right
void drawQuad2D(float x1, float y1, float x2, float y2, const glm::vec3& color, const glm::mat4& projection) {
    init();
    std::vector<glm::vec2> verts = { {x1, y1}, {x2, y1}, {x1, y2}, {x2, y2} };
    Renderer2D::use();
    Renderer2D::setMVP(ProjectionManager::instance().get2DOrtho());
    Renderer2D::setColor(color);      // <-- using the header method
    glBindVertexArray(vao);
    upload(verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

std::vector<std::string> wrapText(const std::string& text, size_t width) {
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string word;
    std::string currentLine;

    while (ss >> word) {
        if (currentLine.length() + word.length() + 1 <= width) {
            if (!currentLine.empty()) currentLine += " ";
            currentLine += word;
        } else {
            lines.push_back(currentLine);
            currentLine = word;
        }
    }
    if (!currentLine.empty()) lines.push_back(currentLine);
    return lines;
}