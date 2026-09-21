/*
 * replay_progress.cpp
 */

#include "replay_progress.h"
#include "text_renderer.h"
#include "globals.h"
#include "projection_manager.h"
#include "Renderer2D.h"
#include "draw_utils.h"
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cstdio> // for snprintf

ReplayProgressBar::ReplayProgressBar(int screenWidth, int screenHeight)
{
  barHeight_ = 20;
  barWidth_ = screenWidth / 4;
  barX_ = (screenWidth - barWidth_) / 2;
  barY_ = kDefaultProgressY;   // top-down, see the note in the header
  progress_ = 0.0f;
  pointerX_ = barX_;
  currentFrame_ = 0;
  totalFrames_ = 0;
}

ReplayProgressBar::ReplayProgressBar()
{
  barX_ = 0;
  barY_ = 0;
  barWidth_ = 200;
  barHeight_ = 20;
  progress_ = 0.0f;
  pointerX_ = barX_;
  currentFrame_ = 0;
  totalFrames_ = 0;
}

void ReplayProgressBar::update(unsigned long long currentFrame, unsigned long long totalFrames)
{
  currentFrame_ = currentFrame;
  totalFrames_ = totalFrames;
  
  if (totalFrames > 0) {
    progress_ = static_cast<float>(currentFrame) / static_cast<float>(totalFrames);
    pointerX_ = barX_ + static_cast<int>(progress_ * barWidth_);
  } else {
    progress_ = 0.0f;
    pointerX_ = barX_;
  }
}

void ReplayProgressBar::render()
{
    if (gViewport[2] <= 0 || gViewport[3] <= 0) return;

    glm::mat4 ortho = ProjectionManager::instance().get2DOrtho();

    // Thin wrappers over the shared 2D helpers: the custom bodies that used to
    // live here created and deleted a VAO+VBO pair per call, and render() draws
    // several of these every frame.  Signatures kept identical for the callers
    // below.
    auto drawQuad2D = [&](float x1, float y1, float x2, float y2, const glm::vec3& color) {
        ::drawQuad2D(x1, y1, x2, y2, color, ortho);
    };

    auto drawLineLoop2D = [&](const std::vector<glm::vec2>& verts, const glm::vec3& color, float lineWidth = 2.0f) {
        ::drawLineLoop2D(verts, color, ortho, lineWidth);
    };

    // Background bar (dark gray)
    drawQuad2D(static_cast<float>(barX_), 
               static_cast<float>(barY_), 
               static_cast<float>(barX_ + barWidth_), 
               static_cast<float>(barY_ + barHeight_), 
               glm::vec3(0.3f, 0.3f, 0.3f));

    // Progress bar (blue)
    int progressWidth = static_cast<int>(progress_ * barWidth_);
    if (progressWidth > 0) {
        drawQuad2D(static_cast<float>(barX_), 
                   static_cast<float>(barY_), 
                   static_cast<float>(barX_ + progressWidth), 
                   static_cast<float>(barY_ + barHeight_), 
                   glm::vec3(0.2f, 0.6f, 0.8f));
    }

    // Border (light gray)
    drawLineLoop2D({
        {static_cast<float>(barX_ - 1), static_cast<float>(barY_)},
        {static_cast<float>(barX_ + barWidth_ + 1), static_cast<float>(barY_)},
        {static_cast<float>(barX_ + barWidth_ + 1), static_cast<float>(barY_ + barHeight_)},
        {static_cast<float>(barX_ - 1), static_cast<float>(barY_ + barHeight_)}
    }, glm::vec3(0.6f, 0.6f, 0.6f));

    // Pointer (yellow vertical line)
    drawQuad2D(static_cast<float>(pointerX_ - 2), 
               static_cast<float>(barY_ + 2), 
               static_cast<float>(pointerX_ + 2), 
               static_cast<float>(barY_ + barHeight_ - 2), 
               glm::vec3(1.0f, 1.0f, 0.0f));

    // Frame counter text
    if (textRenderer) {
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "Frame: %llu / %llu", currentFrame_, totalFrames_);

        // RenderText takes a BOTTOM-UP y (it builds its own y-up ortho), while
        // everything above is in the top-down space of the shared 2D helpers --
        // the simulation ProgressBar does the same flip for its legend text.
        const float label_x      = static_cast<float>(barX_);
        const float label_y_top  = static_cast<float>(barY_) - 25.0f;   // above the bar
        const float label_y_bu   = static_cast<float>(gViewport[3]) - label_y_top;

        textRenderer->RenderText(buffer,
            label_x,
            label_y_bu,
            0.6f,
            glm::vec3(1.0f, 1.0f, 1.0f),
            gViewport[2], gViewport[3]);
    }
    
    glUseProgram(0);
}

void ReplayProgressBar::setPosition(int screenWidth, int screenHeight, int progressY)
{
    barWidth_ = screenWidth / 4;
    barX_ = (screenWidth - barWidth_) / 2;
    barY_ = progressY;
    
    // Update pointer position based on current progress
    if (totalFrames_ > 0) {
        pointerX_ = barX_ + static_cast<int>(progress_ * barWidth_);
    }
}

void ReplayProgressBar::setSize(int width, int height)
{
  barWidth_ = width;
  barHeight_ = height;
  
  // Update pointer position based on current progress
  if (totalFrames_ > 0) {
      pointerX_ = barX_ + static_cast<int>(progress_ * barWidth_);
  }
}

bool ReplayProgressBar::isMouseOver(int mouseX, int mouseY, int windowHeight) const
{
  // Convert from top-origin to bottom-origin
  int bottomY = windowHeight - mouseY;
  
  return (mouseX >= barX_ && mouseX <= barX_ + barWidth_ &&
          bottomY >= barY_ && bottomY <= barY_ + barHeight_);
}

unsigned long long ReplayProgressBar::getFrameAtPosition(int mouseX, int mouseY, int windowHeight) const
{
  // Convert from top-origin to bottom-origin
  int bottomY = windowHeight - mouseY;
  
  // Check if mouse is within bar bounds
  if (totalFrames_ == 0 || 
      mouseX < barX_ || mouseX > barX_ + barWidth_ ||
      bottomY < barY_ || bottomY > barY_ + barHeight_)
  {
    return currentFrame_;
  }
  
  // Calculate frame based on mouse X position
  float ratio = static_cast<float>(mouseX - barX_) / static_cast<float>(barWidth_);
  ratio = (ratio < 0.0f) ? 0.0f : (ratio > 1.0f) ? 1.0f : ratio; // Clamp to [0,1]
  
  return static_cast<unsigned long long>(ratio * totalFrames_);
}
