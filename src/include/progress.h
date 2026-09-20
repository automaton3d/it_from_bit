/*
 * progress.h (legacy-compatible, modern internals)
 *
 * Declares the progress bar widget for visualizing simulation steps.
 */

#ifndef PROGRESS_H
#define PROGRESS_H

#include <glad/glad.h>
#include <string>
#include <glm/glm.hpp>

namespace framework
{
  class ProgressBar
  {
  private:
    // Geometry and layout
    int x;              // X position (center baseline)
    int y;              // Y position (bottom baseline)
    int width;          // Total width
    int height;         // Bar height

    // Section widths and labels
    int barWidths[3];
    std::string steps[3];

    // GL resources
    GLuint vaoSections[3];
    GLuint vboSections[3];
    GLuint vaoOutline;
    GLuint vaoLegend;
    GLuint vboLegend;
    GLuint vboOutline;
    GLuint vaoPointer;
    GLuint vboPointer;
    GLuint vaoTriangles[3];
    GLuint vboTriangles[3];
    GLuint shaderProgram;
    GLint  mvpLocation;
    GLint  colorLocation;

    // Adapter state for legacy update()/render()
    float progressFrac = 0.0f;     // 0..1 fraction of FRAME
    int pointerX = 0;              // computed pointer x

    // Shaders
    const char* vertexShaderSource = R"(
      #version 330 core
      layout (location = 0) in vec2 aPos;
      uniform mat4 mvp;
      void main() {
        gl_Position = mvp * vec4(aPos, 0.0, 1.0);
      }
    )";

    const char* fragmentShaderSource = R"(
      #version 330 core
      out vec4 FragColor;
      uniform vec3 color;
      void main() {
        FragColor = vec4(color, 1.0);
      }
    )";

    // Internal helpers
    void initShaders();
    void initBuffers();
    void cleanup();
    void calculateSectionWidths(int convol, int diffusion, int reloc);
    void updateSectionVBOs(int screenWidth, int screenHeight);
    void updateOutlineVBO();

  public:
    // Constructor (legacy projects often construct with geometry; we keep it explicit)
    ProgressBar(int x, int y, int width, int height);
    ~ProgressBar();

    // Initialization with phase ranges (ENCOUNTER/DIFFUSION/RELOC)
    void init(int screenWidth, int convol, int diffusion, int reloc, int frame);

    // Modern render API
    void draw(unsigned long long timer, int frame, float windowWidth, float windowHeight);

    // Resize based on screen size (recompute projection-space quads)
    void resize(int screenWidth, int screenHeight);

    // Position + size setters
    void setPosition(int newX, int newY);
    void setSize(int newWidth, int newHeight);

    // Legacy adapter: keep these for older call sites
    void update(unsigned long long timer, int frame);
    void renderLegacy(int screenWidth, int screenHeight);

    // Getters
    int getX() const { return x; }
    int getY() const { return y; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }
  };

} // namespace framework

#endif // PROGRESS_H
