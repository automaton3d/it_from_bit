/*
 * hslider.h — FIXED VERSION with callback support
 */

#ifndef HSLIDER_H_
#define HSLIDER_H_

#include <glm/glm.hpp>
#include <algorithm>
#include <functional>
#include <GLFW/glfw3.h>
#include "draw_utils.h"
#include "projection_manager.h"

namespace framework
{
    class HSlider
    {
    private:
        float x_ = 0.0f, y_ = 0.0f;           // Top-left corner (top-left origin)
        float width_ = 200.0f, height_ = 20.0f;
        float thumbX_ = 0.0f;
        float thumbWidth_ = 30.0f;
        bool dragging_ = false;
        float dragOffsetX_ = 0.0f;
        bool hoverReset_ = false;

        // Callback for when value changes
        std::function<void(float)> onValueChanged_;

        const glm::mat4& proj() const { return ProjectionManager::instance().get2DOrtho(); }

    public:
        HSlider() = default;
        HSlider(float x, float y, float width, float height = 20.0f, float thumbWidth = 30.0f)
            : x_(x), y_(y), width_(width), height_(height),
              thumbX_(x), thumbWidth_(thumbWidth) {}

        void draw() const
        {
            const glm::mat4& P = proj();

            // Track
            drawQuad2D(x_, y_, x_ + width_, y_ + height_,
                       glm::vec3(0.60f, 0.60f, 0.65f), P);

            // Thumb
            drawQuad2D(thumbX_, y_, thumbX_ + thumbWidth_, y_ + height_,
                       glm::vec3(0.20f, 0.20f, 0.80f), P);

            // Thumb outline
            drawLineLoop2D({
                {thumbX_ - 1, y_ - 1},
                {thumbX_ + thumbWidth_ + 1, y_ - 1},
                {thumbX_ + thumbWidth_ + 1, y_ + height_ + 1},
                {thumbX_ - 1, y_ + height_ + 1}
            }, glm::vec3(0.0f, 0.7f, 1.0f), P, 2.0f);

            // Reset triangle ABOVE track, pointing DOWN
            float triBaseX = x_ + width_ * 0.5f;
            float triBaseY = y_ - 12.0f;  // ABOVE the slider (negative Y in top-left origin)
            float triSize  = 10.0f;

            glm::vec3 triColor = hoverReset_ ? glm::vec3(1.0f, 0.75f, 0.2f)
                                             : glm::vec3(0.9f, 0.4f, 0.1f);

            // Triangle pointing DOWN: tip at bottom
            std::vector<glm::vec2> tri = {
                {triBaseX - triSize/2, triBaseY - triSize},  // top-left
                {triBaseX + triSize/2, triBaseY - triSize},  // top-right
                {triBaseX,             triBaseY}             // bottom tip (pointing down)
            };

            std::vector<glm::vec2> fan = {
                {triBaseX, triBaseY - triSize/2},  // center
                tri[0], tri[1], tri[2], tri[0]
            };
            drawTriangleFan2D(fan, triColor, P);
            drawLineLoop2D(tri, glm::vec3(0.1f), P, 2.0f);
        }

        void onMouseButton(int button, int action, int mouseX, int mouseY)
        {
            if (button != GLFW_MOUSE_BUTTON_LEFT) return;

            if (action == GLFW_PRESS)
            {
                // Click on thumb → start dragging
                if (mouseX >= thumbX_ && mouseX <= thumbX_ + thumbWidth_ &&
                    mouseY >= y_ && mouseY <= y_ + height_)
                {
                    dragging_ = true;
                    dragOffsetX_ = mouseX - thumbX_;
                    return;
                }

                // Click on reset triangle → center slider
                float triBaseX = x_ + width_ * 0.5f;
                float triBaseY = y_ - 12.0f;
                float triSize = 10.0f;

                if (mouseX >= triBaseX - triSize/2 && mouseX <= triBaseX + triSize/2 &&
                    mouseY >= triBaseY - triSize && mouseY <= triBaseY)
                {
                    setValue(0.5f);
                    // Trigger callback when reset is clicked
                    if (onValueChanged_) {
                        onValueChanged_(0.5f);
                    }
                }
            }
            else if (action == GLFW_RELEASE)
            {
                dragging_ = false;
            }
        }

        void onMouseMove(int mouseX, int mouseY)
        {
            float triBaseX = x_ + width_ * 0.5f;
            float triBaseY = y_ - 12.0f;
            float triSize = 10.0f;

            hoverReset_ = (mouseX >= triBaseX - triSize/2 && mouseX <= triBaseX + triSize/2 &&
                           mouseY >= triBaseY - triSize && mouseY <= triBaseY);
        }

        void onMouseDrag(int mouseX, int /*mouseY*/)
        {
            if (dragging_)
            {
                float oldValue = getValue();
                thumbX_ = mouseX - dragOffsetX_;
                thumbX_ = std::clamp(thumbX_, x_, x_ + width_ - thumbWidth_);

                // Trigger callback if value changed during drag
                float newValue = getValue();
                if (onValueChanged_ && std::abs(newValue - oldValue) > 0.001f) {
                    onValueChanged_(newValue);
                }
            }
        }

        float getValue() const
        {
            if (width_ <= thumbWidth_) return 0.0f;
            return (thumbX_ - x_) / (width_ - thumbWidth_);
        }

        void setValue(float v)
        {
            float clamped = std::clamp(v, 0.0f, 1.0f);
            thumbX_ = x_ + clamped * (width_ - thumbWidth_);
        }

        void recenter(int windowWidth)
        {
            float v = getValue();
            x_ = (windowWidth - width_) * 0.5f;
            setValue(v);
        }

        // Set callback for value changes
        void setOnValueChanged(std::function<void(float)> callback) {
            onValueChanged_ = callback;
        }

        // Getters
        float getX()      const { return x_; }
        float getY()      const { return y_; }
        float getWidth()  const { return width_; }
        float getHeight() const { return height_; }
        bool  isDragging() const { return dragging_; }
        bool  isHoveringReset() const { return hoverReset_; }

        // Setters
        void setPosition(float x, float y) { float v = getValue(); x_ = x; y_ = y; setValue(v); }
        void setSize(float w, float h)     { float v = getValue(); width_ = w; height_ = h; setValue(v); }
        void setThumbWidth(float w)        { float v = getValue(); thumbWidth_ = w; setValue(v); }
    };

} // namespace framework

#endif // HSLIDER_H_
