/*
 * progress.cpp (legacy-compatible, modern internals)
 */

#include "progress.h"
#include "text_renderer.h"
#include "globals.h"
#include "projection_manager.h"
#include "model/simulation.h"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

namespace framework
{
  // Minimal shader compile helper
  static GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);

    GLint success;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
    if (!success) {
      char infoLog[512];
      glGetShaderInfoLog(sh, 512, nullptr, infoLog);
      std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }

    return sh;
  }

  ProgressBar::ProgressBar(int px, int py, int w, int h)
    : x(px), y(py), width(w), height(h),
      shaderProgram(0),
      mvpLocation(-1), colorLocation(-1)
  {
      steps[0] = "Convolution";
      steps[1] = "Diffusion";
      steps[2] = "Relocation";

      barWidths[0] = barWidths[1] = barWidths[2] = 0;

      initShaders();
      initBuffers();
  }

  ProgressBar::~ProgressBar()
  {
      cleanup();
  }

  void ProgressBar::initShaders()
  {
      GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
      GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

      shaderProgram = glCreateProgram();
      glAttachShader(shaderProgram, vs);
      glAttachShader(shaderProgram, fs);
      glLinkProgram(shaderProgram);

      GLint success;
      glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
      if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking error: " << infoLog << std::endl;
      }

      glDeleteShader(vs);
      glDeleteShader(fs);

      mvpLocation   = glGetUniformLocation(shaderProgram, "mvp");
      colorLocation = glGetUniformLocation(shaderProgram, "color");
  }

  void ProgressBar::initBuffers()
  {
      glGenVertexArrays(3, vaoSections);
      glGenBuffers(3, vboSections);
      glGenVertexArrays(1, &vaoOutline);
      glGenBuffers(1, &vboOutline);
      glGenVertexArrays(1, &vaoPointer);
      glGenBuffers(1, &vboPointer);
      glGenVertexArrays(3, vaoTriangles);
      glGenBuffers(3, vboTriangles);

      for (int i = 0; i < 3; ++i)
      {
          glBindVertexArray(vaoSections[i]);
          glBindBuffer(GL_ARRAY_BUFFER, vboSections[i]);
          glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
          glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
          glEnableVertexAttribArray(0);

          glBindVertexArray(vaoTriangles[i]);
          glBindBuffer(GL_ARRAY_BUFFER, vboTriangles[i]);
          glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, nullptr, GL_DYNAMIC_DRAW);
          glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
          glEnableVertexAttribArray(0);
      }

      glBindVertexArray(vaoOutline);
      glBindBuffer(GL_ARRAY_BUFFER, vboOutline);
      glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);

      glBindVertexArray(vaoPointer);
      glBindBuffer(GL_ARRAY_BUFFER, vboPointer);
      glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4, nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);

      glBindVertexArray(0);
      glBindBuffer(GL_ARRAY_BUFFER, 0);

      // Legend VAO/VBO
      glGenVertexArrays(1, &vaoLegend);
      glGenBuffers(1, &vboLegend);

      glBindVertexArray(vaoLegend);
      glBindBuffer(GL_ARRAY_BUFFER, vboLegend);
      glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);
  }

  void ProgressBar::cleanup()
  {
      if (shaderProgram) glDeleteProgram(shaderProgram);
      glDeleteVertexArrays(3, vaoSections);
      glDeleteBuffers(3, vboSections);
      glDeleteVertexArrays(1, &vaoOutline);
      glDeleteBuffers(1, &vboOutline);
      glDeleteVertexArrays(1, &vaoPointer);
      glDeleteBuffers(1, &vboPointer);
      glDeleteVertexArrays(3, vaoTriangles);
      glDeleteBuffers(3, vboTriangles);

      glDeleteVertexArrays(1, &vaoLegend);
      glDeleteBuffers(1, &vboLegend);
  }

  void ProgressBar::calculateSectionWidths(int convol, int diffusion, int reloc)
  {
      int total = convol + diffusion + reloc;
      if (total <= 0) {
          barWidths[0] = barWidths[1] = barWidths[2] = 0;
          return;
      }

      barWidths[0] = (convol    * width) / total;
      barWidths[1] = (diffusion * width) / total;
      barWidths[2] = width - barWidths[0] - barWidths[1];
  }

  void ProgressBar::init(int screenWidth, int convol, int diffusion, int reloc, int frame)
  {
      calculateSectionWidths(convol, diffusion, reloc);
      resize(screenWidth, gViewport[3]);
  }

  void ProgressBar::updateSectionVBOs(int screenWidth, int screenHeight)
  {
      x = (screenWidth - width) / 2;
      int acc = 0;

      for (int i = 0; i < 3; ++i)
      {
          const float verts[8] = {
              float(x + acc),                float(y - height),
              float(x + acc + barWidths[i]), float(y - height),
              float(x + acc + barWidths[i]), float(y),
              float(x + acc),                float(y)
          };
          glBindBuffer(GL_ARRAY_BUFFER, vboSections[i]);
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
          acc += barWidths[i];
      }

      glBindBuffer(GL_ARRAY_BUFFER, 0);
      updateOutlineVBO();
  }

  void ProgressBar::updateOutlineVBO()
  {
      const float outline[8] = {
          float(x - 1),         float(y - height),
          float(x + width + 1), float(y - height),
          float(x + width + 1), float(y),
          float(x - 1),         float(y)
      };

      glBindBuffer(GL_ARRAY_BUFFER, vboOutline);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(outline), outline);
      glBindBuffer(GL_ARRAY_BUFFER, 0);
  }

  void ProgressBar::draw(unsigned long long timer, int frame, float windowWidth, float windowHeight)
  {
      int expectedX = ((int)windowWidth - width) / 2;
      if (x != expectedX) {
          x = expectedX;
          updateSectionVBOs((int)windowWidth, (int)windowHeight);
      }

      glUseProgram(shaderProgram);

      glm::mat4 ortho = ProjectionManager::instance().get2DOrtho();
      glUniformMatrix4fv(mvpLocation, 1, GL_FALSE, glm::value_ptr(ortho));

      // Colors
      const float colors[3][3] = {
          {0.30f, 0.30f, 0.00f},
          {0.50f, 0.00f, 0.00f},
          {0.00f, 0.50f, 0.00f}
      };

      // Sections
      for (int i = 0; i < 3; ++i)
      {
          glUniform3fv(colorLocation, 1, colors[i]);
          glBindVertexArray(vaoSections[i]);
          glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
      }

      // Outline
      glUniform3f(colorLocation, 0.6f, 0.6f, 0.6f);
      glBindVertexArray(vaoOutline);
      glLineWidth(2.0f);
      glDrawArrays(GL_LINE_LOOP, 0, 4);

      // Pointer
      float frac = (frame > 0) ? float(timer % frame) / float(frame) : 0.0f;
      float px = x + frac * width;
      const float pointerVerts[4] = {
          px, float(y - height + 2),
          px, float(y - 2)
      };
      glBindVertexArray(vaoPointer);
      glBindBuffer(GL_ARRAY_BUFFER, vboPointer);
      glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pointerVerts), pointerVerts);
      glUniform3f(colorLocation, 1.0f, 1.0f, 0.0f);
      glLineWidth(3.0f);
      glDrawArrays(GL_LINES, 0, 2);

      // ============================================================
      // LEGEND RECTANGLES (always drawn)
      // ============================================================
      float boxW = 14.f;
      float boxH = height * 0.6f;
      float legendStartX = x + width + 12.f;
      float legendStartY = y - height - 4.0f;// + (height - boxH) * 0.5f;

      for (int i = 0; i < 3; ++i)
      {
          float ly = legendStartY + i * (boxH + 6.f) - 10.0f;

          const float verts[8] = {
              legendStartX,      ly,
              legendStartX+boxW, ly,
              legendStartX+boxW, ly+boxH,
              legendStartX,      ly+boxH
          };

          glBindVertexArray(vaoLegend);
          glBindBuffer(GL_ARRAY_BUFFER, vboLegend);
          glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

          glUniform3fv(colorLocation, 1, colors[i]);
          glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
      }

      // ============================================================
      // LEGEND TEXT (if textRenderer exists)
      // ============================================================
      if (textRenderer)
      {
          for (int i = 0; i < 3; ++i)
          {
              float ly = legendStartY + i * (boxH + 6.f);
              float ty = windowHeight - (ly + 2.f);

              textRenderer->RenderText(
                  steps[i],
                  legendStartX + boxW + 6.f,
                  ty,
                  0.35f,
                  glm::vec3(0.9f),
                  (int)windowWidth, (int)windowHeight);
          }
      }

      glBindVertexArray(0);
      glUseProgram(0);
  }

  void ProgressBar::resize(int screenWidth, int screenHeight)
  {
      updateSectionVBOs(screenWidth, screenHeight);
  }

  void ProgressBar::setPosition(int newX, int newY)
  {
      x = newX;
      y = newY;
      resize(gViewport[2], gViewport[3]);
  }

  void ProgressBar::setSize(int newWidth, int newHeight)
  {
      width = newWidth;
      height = newHeight;
      calculateSectionWidths(1, 1, 1);
      resize(gViewport[2], gViewport[3]);
  }

  void ProgressBar::update(unsigned long long timer, int frame)
  {
      progressFrac = (frame > 0) ? float(timer % frame) / float(frame) : 0.0f;
      pointerX = x + int(progressFrac * width);
  }

  void ProgressBar::renderLegacy(int screenWidth, int screenHeight)
  {
      draw((unsigned long long)(progressFrac * screenHeight),
           screenHeight,
           (float)screenWidth,
           (float)screenHeight);
  }

} // namespace framework
