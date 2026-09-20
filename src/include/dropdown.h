/*
 * dropdown.h (adapted)
 */

#ifndef DROPDOWN_H_
#define DROPDOWN_H_

#include <vector>
#include <string>
#include "text_renderer.h"

class Dropdown
{
public:
    float x, y, width, height;
    bool isOpen_;

    Dropdown(float x, float y, float w, float h,
                 const std::vector<std::string>& opts,
                 int winH, // <- Added parameter
                 const std::string& header = "");

    void draw(TextRenderer& renderer);

    bool handleClick(int mx, int my);

    void scroll(int direction);
    void open();
    void close();
    void toggle();

    bool containsHeader(int mx, int my) const;
    bool containsDropdown(int mx, int my) const;
    int getItemIndexAt(int mx, int my) const;

    std::string getSelectedItem() const;
    void selectByValue(const std::string& value);
    int getSelectedIndex() const;
    void setSelectedIndex(int idx);

    bool isMouseOver(int mx, int my) const;
    void clearSelection();
    bool wasJustSelected();

    // Hover functionality
    void updateHover(int mx, int my);
    void clearHover();
    bool isExpanded() const { return isOpen_; }

private:
    std::vector<std::string> options_;
    int selectedIndex_;
    int scrollOffset_;
    std::string header_;
    bool selectionChanged_;
    int hoverIndex_;  // -1 means no hover
    int windowHeight;
};

#endif // DROPDOWN_H_
