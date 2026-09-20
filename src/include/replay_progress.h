/*
 * replay_progress.h
 *
 * Progress bar widget for replay mode
 */

#ifndef REPLAY_PROGRESS_H
#define REPLAY_PROGRESS_H

//#include <glad/glad.h>

class ReplayProgressBar
{
public:
  // Constructor: initializes bar with screen dimensions
  explicit ReplayProgressBar(int screenWidth, int screenHeight);

  // Default constructor for deferred initialization
  ReplayProgressBar();

  // Updates progress based on current frame and total frames
  void update(unsigned long long currentFrame, unsigned long long totalFrames);

  // Renders the progress bar
  void render();

  // Adjusts bar position
  void setPosition(int screenWidth, int screenHeight, int progressY);

  // Adjusts bar size
  void setSize(int width, int height);

  // Checks if mouse is over the bar
  bool isMouseOver(int mouseX, int mouseY, int windowHeight) const;

  // Returns frame corresponding to mouse position (with coordinate conversion)
  unsigned long long getFrameAtPosition(int mouseX, int mouseY, int windowHeight) const;

  // Getters
  int getX() const { return barX_; }
  int getY() const { return barY_; }
  int getWidth() const { return barWidth_; }
  int getHeight() const { return barHeight_; }
  float getProgress() const { return progress_; }
  unsigned long long getCurrentFrame() const { return currentFrame_; }
  unsigned long long getTotalFrames() const { return totalFrames_; }

private:
  int barX_, barY_;          // position
  int barWidth_, barHeight_; // dimensions
  int pointerX_;             // pointer position
  float progress_;           // normalized progress [0..1]
  unsigned long long currentFrame_;
  unsigned long long totalFrames_;
};

#endif // REPLAY_PROGRESS_H
