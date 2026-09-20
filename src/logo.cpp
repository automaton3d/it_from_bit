/*
 * logo.cpp (clean version)
 */

#include "logo.h"
#include "stb_image.h"
#include <iostream>
#include <vector>
#include <atomic>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "globals.h"
#include "projection_manager.h"


extern unsigned int textureProgram2D;
extern int textureMvpLoc;
extern int textureSamplerLoc;

namespace framework
{


  Logo::Logo(const std::string& path)
  {
    // Candidate paths to search
    std::vector<std::string> searchPaths = {
        path,
        "fonts/" + path,
        "bin/" + path,
        "../" + path,
        "../bin/" + path,
        "../../bin/" + path,
        "../../assets/" + path,
        "../assets/" + path
    };

    unsigned char* data = nullptr;
    int channels = 0;
    for (const auto& tryPath : searchPaths) {
        data = stbi_load(tryPath.c_str(), &mWidth_, &mHeight_, &channels, 4);
        if (data) {
            break;
        }
    }

    if (!data) {
        std::cerr << "Logo: failed to load '" << path << "' from any location\n";
        for (const auto& p : searchPaths)
            std::cerr << "  - " << p << "\n";
        mTexture_ = 0;
        mWidth_   = 0;
        mHeight_  = 0;
        return;
    }

    glGenTextures(1, &mTexture_);
    glBindTexture(GL_TEXTURE_2D, mTexture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, mWidth_, mHeight_, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);
  }

  Logo::~Logo()
  {
    if (mTexture_) glDeleteTextures(1, &mTexture_);
  }



  void Logo::draw(int x, int y, float scale) const
  {
      if (!mTexture_) {
          std::cerr << "Logo::draw() - texture not loaded\n";
          return;
      }
      if (!textureProgram2D) {
          std::cerr << "Logo::draw() - textureProgram2D is 0 (shader not initialized)\n";
          return;
      }

      GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
      glDisable(GL_DEPTH_TEST);

      // 🔎 Step 4: disable blending to rule out alpha transparency
      glDisable(GL_BLEND);

      // Target width ~200px (same as splash screen design)
      const int targetW = 200;
      const int targetH = static_cast<int>(mHeight_ * targetW / (float)mWidth_);
      const int w = static_cast<int>(targetW * scale);
      const int h = static_cast<int>(targetH * scale);

      struct Vertex { float x, y, u, v; };
      Vertex verts[4] = {
          { (float)x,       (float)y,       0.0f, 1.0f }, // top-left
          { (float)(x + w), (float)y,       1.0f, 1.0f }, // top-right
          { (float)(x + w), (float)(y + h), 1.0f, 0.0f }, // bottom-right
          { (float)x,       (float)(y + h), 0.0f, 0.0f }  // bottom-left
      };

      GLuint vao, vbo;
      glGenVertexArrays(1, &vao);
      glGenBuffers(1, &vbo);

      glBindVertexArray(vao);
      glBindBuffer(GL_ARRAY_BUFFER, vbo);
      glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(2 * sizeof(float)));
      glEnableVertexAttribArray(1);

      // Use the texture shader
      glUseProgram(textureProgram2D);

      // Set MVP matrix
      const glm::mat4& ortho = ProjectionManager::instance().get2DOrtho();
      glUniformMatrix4fv(textureMvpLoc, 1, GL_FALSE, glm::value_ptr(ortho));

      // Bind texture
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, mTexture_);
      glUniform1i(textureSamplerLoc, 0);

      // Draw quad
      glBindVertexArray(vao);
      glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
      glBindVertexArray(0);

      // Cleanup
      glDeleteBuffers(1, &vbo);
      glDeleteVertexArrays(1, &vao);

      if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
  }




} // namespace framework
