#ifndef BUTTON_H_
#define BUTTON_H_

#include <string>

#include "text_renderer.h"

class Button
{
public:
    Button(float x,
           float y,
           float w,
           float h,
           const std::string& label,
           bool isDefault = false);

    ~Button();

    // Disable copy to prevent double-free
    Button(const Button&) = delete;
    Button& operator=(const Button&) = delete;

    void draw(TextRenderer& renderer,
              int screenWidth,
              int screenHeight);

    void drawAsHyperlink(TextRenderer& renderer,
                         bool hovered,
                         int screenWidth,
                         int screenHeight);

    bool contains(int mouseX,
                  int mouseY,
                  int screenHeight) const;

    void setPosition(float x, float y);

    void setSize(float w, float h);

    void setDefault(bool value)
    {
        isDefault_ = value;
    }

    bool getDefault() const
    {
        return isDefault_;
    }

    const std::string& getLabel() const
    {
        return label_;
    }

private:
    void setupGeometry();

    void cleanup();

    float x_;
    float y_;
    float w_;
    float h_;

    std::string label_;

    bool isDefault_;

    unsigned int shadowVAO{};
    unsigned int shadowVBO{};

    unsigned int bgVAO{};
    unsigned int bgVBO{};

    unsigned int borderVAO{};
    unsigned int borderVBO{};

    mutable unsigned int underlineVAO{};
    mutable unsigned int underlineVBO{};

    mutable bool underlineInitialized{false};
};

#endif