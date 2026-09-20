/*
 * vslider.h – MODERN VERSION (works with ProjectionManager)
 */

#ifndef VSLIDER_H_
#define VSLIDER_H_

#include <algorithm>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include "projection_manager.h"   // NEW: use centralized projection
#include "draw_utils.h"           // will use the matrix overloads

namespace framework
{
    class VSlider
    {
    private:
        float x_, y_;           // Bottom-left corner of the track (in screen pixels)
        float width_, height_;  // Track size
        float thumbY_;          // Current thumb Y position
        float thumbHeight_;     // Height of the thumb (variable)
        bool dragging_ = false;
        bool visible_  = true;

        // Helper to get current 2D ortho matrix
        const glm::mat4& proj() const { return ProjectionManager::instance().get2DOrtho(); }

    public:
        VSlider(float x = 0.0f, float y = 0.0f, float width = 20.0f, float height = 200.0f, float thumbHeight = 30.0f)
            : x_(x), y_(y), width_(width), height_(height),
              thumbY_(y), thumbHeight_(thumbHeight)
        {}

        void setVisible(bool state) { visible_ = state; }
        bool isVisible() const      { return visible_; }

        // ------------------------------------------------------------
        // DRAW – uses ProjectionManager, no more winW/winH parameters
        // ------------------------------------------------------------
        void draw() const
        {
            if (!visible_) return;

            const glm::mat4& P = proj();

            // Track background
            drawQuad2D(x_, y_, x_ + width_, y_ + height_,
                       glm::vec3(0.60f, 0.60f, 0.65f), P);

            // Thumb fill
            drawQuad2D(x_, thumbY_, x_ + width_, thumbY_ + thumbHeight_,
                       glm::vec3(0.20f, 0.20f, 0.80f), P);

            // Thumb outline (slightly larger)
            drawLineLoop2D({
                {x_ - 1.0f, thumbY_ - 1.0f},
                {x_ + width_ + 1.0f, thumbY_ - 1.0f},
                {x_ + width_ + 1.0f, thumbY_ + thumbHeight_ + 1.0f},
                {x_ - 1.0f, thumbY_ + thumbHeight_ + 1.0f}
            }, glm::vec3(0.0f, 0.7f, 1.0f), P, 2.0f);
        }

        // ------------------------------------------------------------
        // INPUT – mouse coordinates are already top-left (as in splash.cpp)
        // ------------------------------------------------------------
        void onMouseButton(int button, int action, int mouseX, int mouseY)
        {
            if (!visible_) return;

            // mouseY is top-left origin → convert to bottom-left for internal math
            int winH = ProjectionManager::instance().getHeight();
            float my = winH - mouseY;

            if (button == GLFW_MOUSE_BUTTON_LEFT)
            {
                if (action == GLFW_PRESS)
                {
                    if (mouseX >= x_ && mouseX <= x_ + width_ &&
                        my >= thumbY_ && my <= thumbY_ + thumbHeight_)
                    {
                        dragging_ = true;
                    }
                }
                else if (action == GLFW_RELEASE)
                {
                    dragging_ = false;
                }
            }
        }

        void onMouseMove(int mouseX, int mouseY)
        {
            if (!dragging_ || !visible_) return;

            int winH = ProjectionManager::instance().getHeight();
            float my = winH - mouseY;

            thumbY_ = my - thumbHeight_ / 2.0f;
            thumbY_ = std::clamp(thumbY_, y_, y_ + height_ - thumbHeight_);
        }

        // ------------------------------------------------------------
        // VALUE INTERFACE
        // ------------------------------------------------------------
        float getValue() const
        {
            if (!visible_ || height_ <= thumbHeight_) return 0.0f;
            return (thumbY_ - y_) / (height_ - thumbHeight_);
        }

        void setValue(float v)
        {
            float clamped = std::clamp(v, 0.0f, 1.0f);
            thumbY_ = y_ + clamped * (height_ - thumbHeight_);
        }

        // ------------------------------------------------------------
        // SCROLLBAR MODE (used by lists, replays, etc.)
        // ------------------------------------------------------------
        int getFirstVisibleIndex(int totalItems, int visibleItemsCount)
        {
            if (totalItems <= visibleItemsCount)
            {
                visible_ = false;
                return 0;
            }

            visible_ = true;
            float ratio = static_cast<float>(visibleItemsCount) / totalItems;
            thumbHeight_ = std::max(10.0f, height_ * ratio);

            int maxFirst = totalItems - visibleItemsCount;
            int index = static_cast<int>(getValue() * maxFirst + 0.5f);
            return std::clamp(index, 0, maxFirst);
        }

        void setScrollbarParams(int totalItems, int visibleItemsCount, int firstIndex)
        {
            if (totalItems <= visibleItemsCount)
            {
                visible_ = false;
                return;
            }

            visible_ = true;
            float ratio = static_cast<float>(visibleItemsCount) / totalItems;
            thumbHeight_ = std::max(10.0f, height_ * ratio);

            float fraction = static_cast<float>(firstIndex) / (totalItems - visibleItemsCount);
            setValue(fraction);
        }

        // ------------------------------------------------------------
        // GETTERS / SETTERS
        // ------------------------------------------------------------
        bool isDragging() const { return dragging_; }

        float getX() const      { return x_; }
        float getY() const      { return y_; }
        float getWidth() const  { return width_; }
        float getHeight() const { return height_; }

        void setPosition(float x, float y) { x_ = x; y_ = y; }
        void setSize(float w, float h)     { width_ = w; height_ = h; }
    };

} // namespace framework

#endif /* VSLIDER_H_ */
