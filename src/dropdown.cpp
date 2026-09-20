#include "dropdown.h"
#include "projection_manager.h"
#include "text_renderer.h"
#include "draw_utils.h"
#include <algorithm>
#include <iostream>

Dropdown::Dropdown(float x, float y, float w, float h,
                   const std::vector<std::string>& opts,
                   int winH, // <- Added parameter
                   const std::string& header)
    : x(x), y(y), width(w), height(h), isOpen_(false),
      options_(opts), selectedIndex_(0), scrollOffset_(0),
      header_(header), selectionChanged_(false), hoverIndex_(-1),
      windowHeight(winH) // <- Initialize the member variable
{
    if (!options_.empty()) selectedIndex_ = 0;
}

std::string Dropdown::getSelectedItem() const {
    if (selectedIndex_ >= 0 && selectedIndex_ < (int)options_.size())
        return options_[selectedIndex_];
    return "";
}

void Dropdown::selectByValue(const std::string& value)
{
    for (size_t i = 0; i < options_.size(); ++i)
        if (options_[i] == value) {
            selectedIndex_ = (int)i;
            scrollOffset_ = 0;
            return;
        }
}

bool Dropdown::containsHeader(int mx, int my) const
{
    int winH = ProjectionManager::instance().getHeight();

    // Header geometry is in top-down space (y = distance from top)
    float headerTopY_topdown    = y;
    float headerBottomY_topdown = y + height;

    // Convert to bottom-up (GLFW mouse Y)
    float headerTopY    = winH - headerBottomY_topdown;  // higher on screen
    float headerBottomY = winH - headerTopY_topdown;     // lower on screen

    bool hit = (mx >= x && mx <= x + width &&
                my >= headerBottomY && my <= headerTopY);

    // Optional debug
    printf("containsHeader: %d (my=%d, range=[%.0f, %.0f])\n", hit, my, headerBottomY, headerTopY);

    return hit;
}

bool Dropdown::containsDropdown(int mx, int my) const
{
    if (!isOpen_) return false;

    int winH = ProjectionManager::instance().getHeight();
    const int VISIBLE = 6;
    int actual = std::min((int)options_.size() - scrollOffset_, VISIBLE);

    // List starts immediately below header in top-down space
    float listTopY_topdown    = y + height;
    float listBottomY_topdown = listTopY_topdown + actual * height;

    // Convert to bottom-up
    float listTopY    = winH - listBottomY_topdown;      // higher on screen (top of list)
    float listBottomY = winH - listTopY_topdown;         // lower on screen (bottom of list)

    bool hit = (mx >= x && mx <= x + width &&
                my >= listBottomY && my <= listTopY);

    // Optional debug
    printf("\tcontainsDropdown: %d (my=%d, range=[%.0f, %.0f])\n", hit, my, listBottomY, listTopY);

    return hit;
}

int Dropdown::getItemIndexAt(int mx, int my) const
{
    if (!isOpen_) return -1;

    int winH = ProjectionManager::instance().getHeight();
    const int VISIBLE = 6;

    float itemTopY_topdown = y + height;  // top of first visible item (top-down)

    int end = std::min((int)options_.size(), scrollOffset_ + VISIBLE);

    for (int i = scrollOffset_; i < end; ++i)
    {
        float itemBottomY_topdown = itemTopY_topdown + height;

        float itemTopY    = winH - itemBottomY_topdown;
        float itemBottomY = winH - itemTopY_topdown;

        if (my >= itemBottomY && my <= itemTopY)
            return i;

        itemTopY_topdown = itemBottomY_topdown;
    }
    return -1;
}

bool Dropdown::isMouseOver(int mx, int my) const
{
    return containsHeader(mx, my) || containsDropdown(mx, my);
}

void Dropdown::scroll(int dir)
{
    const int VISIBLE = 6;
    int maxScroll = std::max(0, (int)options_.size() - VISIBLE);
    scrollOffset_ = std::clamp(scrollOffset_ + dir, 0, maxScroll);
}

// dropdown.cpp – replace these three functions (and ONLY these)

bool Dropdown::handleClick(int mx, int my)
{
    // 1. Header click – always has priority
    if (containsHeader(mx, my))
    {
        isOpen_ = !isOpen_;        // toggle open/close
        hoverIndex_ = -1;
        selectionChanged_ = false;
        return true;
    }

    // 2. Click on an open list item?
    if (isOpen_)
    {
        int idx = getItemIndexAt(mx, my);
        if (idx != -1)
        {
            selectedIndex_ = idx;
            isOpen_ = false;
            hoverIndex_ = -1;
            selectionChanged_ = true;
            return true;
        }
    }

    // Clicked somewhere else → let the global handler close everything
    return false;
}

void Dropdown::toggle()
{
    isOpen_ = !isOpen_;
    hoverIndex_ = -1;
}

void Dropdown::open()
{
    isOpen_ = true;
    hoverIndex_ = -1;
}

void Dropdown::close()
{
    isOpen_ = false;
    hoverIndex_ = -1;
}

void Dropdown::updateHover(int mx, int my)
{
    if (!isOpen_) { hoverIndex_ = -1; return; }
    hoverIndex_ = getItemIndexAt(mx, my);
}

bool Dropdown::wasJustSelected()
{
    bool r = selectionChanged_;
    selectionChanged_ = false;
    return r;
}

void Dropdown::draw(TextRenderer& renderer)
{
    const glm::mat4& P = ProjectionManager::instance().get2DOrtho();
    int winW = ProjectionManager::instance().getWidth();
    int winH = ProjectionManager::instance().getHeight();

    // Header background
    drawQuad2D(x, y, x + width, y + height, glm::vec3(0.92f), P);

    drawLineLoop2D(
        {{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}},
        glm::vec3(0.35f), P, 1.5f
    );

    // Header text
    std::string headerText = header_.empty() ? getSelectedItem() : header_;
    float textY = winH - (y + height * 0.5f) - 7;

    renderer.RenderText(headerText, x + 8, textY, 0.32f,
                        glm::vec3(0.0f), winW, winH);

    // Arrow
    float ax = x + width - 16;
    float ay = y + height * 0.5f;

    std::vector<glm::vec2> arrow;
    if (isOpen_)
        arrow = {{ax-6,ay+4}, {ax+6,ay+4}, {ax,ay-4}}; // down
    else
        arrow = {{ax-6,ay-4}, {ax+6,ay-4}, {ax,ay+4}}; // up

    drawTriangleFan2D(arrow, glm::vec3(0,0,0), P);

    if (!isOpen_ || options_.empty()) return;

    const int VISIBLE = 6;
    int start = scrollOffset_;
    int end = std::min((int)options_.size(), start + VISIBLE);
    int count = end - start;

    float listTop    = y + height;
    float listBottom = listTop + height * count;

    drawQuad2D(x, listTop, x + width, listBottom, glm::vec3(1), P);

    drawLineLoop2D(
        {{x, listTop}, {x + width, listTop},
         {x + width, listBottom}, {x, listBottom}},
         glm::vec3(0.35f), P, 1.5f
    );

    float curY = listTop;

    for (int i = start; i < end; ++i)
    {
        bool selected = (i == selectedIndex_);
        bool hovered  = (i == hoverIndex_);

        if (selected || hovered)
        {
            glm::vec3 bg = selected ?
                glm::vec3(0.68f,0.85f,0.95f) :
                glm::vec3(0.95f);

            drawQuad2D(x, curY, x + width, curY + height, bg, P);
        }

        float itemTextY = winH - (curY + height * 0.5f) - 7;
        renderer.RenderText(options_[i], x + 8, itemTextY, 0.32f,
                            glm::vec3(0,0,0), winW, winH);

        curY += height;
    }
}

int Dropdown::getSelectedIndex() const {
    return selectedIndex_;
}

void Dropdown::setSelectedIndex(int idx) {
    if (idx >= 0 && idx < (int)options_.size())
        selectedIndex_ = idx;
}
