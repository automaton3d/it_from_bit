/*
 * replay_progress.h
 *
 * Progress bar widget for replay mode
 */

#ifndef REPLAY_PROGRESS_H
#define REPLAY_PROGRESS_H

//#include <glad/glad.h>

/*
 * Progress bar of the replay mode: `Frame: n / N`, a yellow pointer and a
 * border, a quarter of the window wide.
 *
 * Coordinate conventions (they are the standing trap of this GUI):
 *   * render() draws through the shared 2D helpers in the TOP-DOWN space of
 *     ProjectionManager's 2D ortho (origin top-left, y grows downwards), so
 *     barY_ is measured from the TOP edge;
 *   * isMouseOver()/getFrameAtPosition() take a BOTTOM-UP mouse y (the
 *     `height - glfwY` value the callbacks pass to Button::contains and
 *     Cortina::isMouseOver) and convert it to that same top-down space.
 *
 * All three call sites that place the bar (constructor, GUI.cpp and
 * callback.cpp) must agree on barY_, hence the constant below.
 */
class ReplayProgressBar
{
public:
  // Distance of the bar from the top edge, in pixels.  Chosen to sit in the
  // same band as the simulation ProgressBar (which occupies y = 110..130).
  static constexpr int kDefaultProgressY = 100;

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
