// Cortina.cpp

#include <algorithm>
#include <vector>
#include <functional>
#include <iostream>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "cortina.h"
#include "globals.h"
#include "text_renderer.h"
#include "Renderer2D.h"

// ============================================================================
// Globals
// ============================================================================

extern GLFWwindow* window;
extern int gViewport[4];

// ============================================================================
// Constructor
// ============================================================================

Cortina::Cortina(
    float x,
    float y,
    float width,
    float height,
    const std::vector<std::string>& options,
    int initialSelection,
    std::function<void(int)> callback)
    :
    x(x),
    y(y),
    width(width),
    height(height),
    options(options),
    selectedIndex(initialSelection),
    selectionCallback(callback),
    isOpen(false)
{
    if (this->options.empty())
    {
        this->options.push_back("No Options");
        selectedIndex = 0;
    }

    if (selectedIndex < 0 ||
        selectedIndex >= (int)this->options.size())
    {
        selectedIndex = 0;
    }
}

// ============================================================================
// Selection
// ============================================================================

void Cortina::setSelectedIndex(int idx)
{
    if (idx >= 0 && idx < (int)options.size())
    {
        selectedIndex = idx;

        if (isOpen)
        {
            if (idx < scrollOffset)
            {
                scrollOffset = idx;
            }
            else if (idx >= scrollOffset + maxVisibleItems)
            {
                scrollOffset = idx - maxVisibleItems + 1;
            }
        }
    }
}

// ============================================================================
// Helpers
// ============================================================================

bool Cortina::isMouseOver(
    double mx,
    double my,
    float rx,
    float ry,
    float rwidth,
    float rheight)
{
    float ry_topdown =
        gViewport[3] - ry - rheight;

    return
        mx >= rx &&
        mx < (rx + rwidth) &&
        my >= ry_topdown &&
        my < (ry_topdown + rheight);
}

// ============================================================================
// Rect Rendering
// ============================================================================

void Cortina::drawRect(
    float rx,
    float ry,
    float rwidth,
    float rheight,
    const glm::vec3& color)
{
    glm::mat4 ortho_projection =
        glm::ortho(
            0.0f,
            (float)gViewport[2],
            (float)gViewport[3],
            0.0f
        );

    Renderer2D::use();
    Renderer2D::setMVP(ortho_projection);
    Renderer2D::setColor(color);

    GLfloat vertices[] =
    {
        // Triangle 1
        rx,          ry + rheight,
        rx,          ry,
        rx + rwidth, ry,

        // Triangle 2
        rx,          ry + rheight,
        rx + rwidth, ry,
        rx + rwidth, ry + rheight
    };

    GLuint vao = 0;
    GLuint vbo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_DYNAMIC_DRAW
    );

    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(GLfloat),
        (void*)0
    );

    glEnableVertexAttribArray(0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
}

// ============================================================================
// Render
// ============================================================================

void Cortina::render(TextRenderer* renderer)
{
    if (!renderer)
        return;

    float ascender  = renderer->getAscenderPx();
    float descender = renderer->getDescenderPx();

    float centerline_offset =
        0.5f * (ascender - descender) * text_scale;

    const float VISUAL_TWEAK_PX =
        1.0f * text_scale;

    // ------------------------------------------------------------------------
    // Main box
    // ------------------------------------------------------------------------

    drawRect(
        x - border_thickness,
        y - border_thickness,
        width  + 2 * border_thickness,
        height + 2 * border_thickness,
        border_color
    );

    drawRect(
        x,
        y,
        width,
        height,
        background_color
    );

    // ------------------------------------------------------------------------
    // Main text
    // ------------------------------------------------------------------------

    float main_rect_center_y_bu =
        y + height * 0.5f;

    float main_text_baseline_y_bu =
        main_rect_center_y_bu -
        centerline_offset +
        VISUAL_TWEAK_PX;

    float main_text_y_td =
        (float)gViewport[3] -
        main_text_baseline_y_bu;

    renderer->RenderText(
        options[selectedIndex],
        x + padding_x,
        main_text_y_td - padding_y,
        text_scale,
        text_color
    );

    // ------------------------------------------------------------------------
    // Arrow
    // ------------------------------------------------------------------------

    float arrow_width  = 10.0f;
    float arrow_height = 8.0f;

    float arrow_x =
        x + width - arrow_width - 5.0f;

    float arrow_y =
        y + height * 0.5f - arrow_height * 0.5f;

    if (isOpen)
    {
        // UP

        drawRect(
            arrow_x + arrow_width * 0.5f - 1.0f,
            arrow_y + arrow_height * 0.25f,
            2.0f,
            arrow_height * 0.5f,
            text_color
        );

        drawRect(
            arrow_x,
            arrow_y + arrow_height * 0.75f,
            arrow_width,
            2.0f,
            text_color
        );
    }
    else
    {
        // DOWN

        drawRect(
            arrow_x + arrow_width * 0.5f - 1.0f,
            arrow_y + arrow_height * 0.25f,
            2.0f,
            arrow_height * 0.5f,
            text_color
        );

        drawRect(
            arrow_x,
            arrow_y + arrow_height * 0.25f,
            arrow_width,
            2.0f,
            text_color
        );
    }

    // ------------------------------------------------------------------------
    // Dropdown list
    // ------------------------------------------------------------------------

    if (isOpen)
    {
        float item_height = height;

        int start_index = scrollOffset;

        int end_index =
            std::min(
                (int)options.size(),
                scrollOffset + maxVisibleItems
            );

        for (int i = start_index; i < end_index; ++i)
        {
            int pos = i - start_index;

            float item_y =
                y + height + pos * item_height;

            glm::vec3 item_bg_color =
                (i == selectedIndex)
                    ? hover_color
                    : background_color;

            if (isMouseOver(
                    mouseX,
                    mouseY,
                    x,
                    item_y,
                    width,
                    item_height))
            {
                item_bg_color = hover_color;
            }

            drawRect(
                x - border_thickness,
                item_y - border_thickness,
                width + 2 * border_thickness,
                item_height + 2 * border_thickness,
                border_color
            );

            drawRect(
                x,
                item_y,
                width,
                item_height,
                item_bg_color
            );

            float item_rect_center_y_bu =
                item_y + item_height * 0.5f;

            float item_text_baseline_y_bu =
                item_rect_center_y_bu -
                centerline_offset +
                VISUAL_TWEAK_PX;

            float item_text_y_td =
                (float)gViewport[3] -
                item_text_baseline_y_bu;

            renderer->RenderText(
                options[i],
                x + padding_x,
                item_text_y_td - padding_y,
                text_scale,
                text_color
            );
        }
    }
}

// ============================================================================
// Mouse Click
// ============================================================================

bool Cortina::handleMouseClick(
    double xpos,
    double ypos)
{
    bool handled = false;

    // Main box

    if (isMouseOver(
            xpos,
            ypos,
            x,
            y,
            width,
            height))
    {
        isOpen = !isOpen;
        handled = true;
    }
    else if (isOpen)
    {
        float item_height = height;

        int start_index = scrollOffset;

        int end_index =
            std::min(
                (int)options.size(),
                scrollOffset + maxVisibleItems
            );

        for (int i = start_index; i < end_index; ++i)
        {
            int pos = i - start_index;

            float item_y =
                y + height + pos * item_height;

            if (isMouseOver(
                    xpos,
                    ypos,
                    x,
                    item_y,
                    width,
                    item_height))
            {
                selectedIndex = i;
                isOpen = false;
                handled = true;

                if (selectionCallback)
                {
                    selectionCallback(selectedIndex);
                }

                break;
            }
        }

        if (!handled)
        {
            isOpen = false;
        }
    }

    return handled;
}

// ============================================================================
// Mouse Scroll
// ============================================================================

void Cortina::handleMouseScroll(
    double xpos,
    double ypos,
    double yoffset)
{
    if (!isOpen)
        return;

    int visible_count =
        std::min(
            (int)options.size(),
            maxVisibleItems
        );

    float list_top =
        y + height;

    float list_height =
        height * visible_count;

    if (isMouseOver(
            xpos,
            ypos,
            x,
            list_top,
            width,
            list_height))
    {
        if (yoffset > 0)
        {
            if (scrollOffset > 0)
            {
                scrollOffset--;
            }
        }
        else if (yoffset < 0)
        {
            int max_scroll =
                (int)options.size() -
                maxVisibleItems;

            if (max_scroll > 0 &&
                scrollOffset < max_scroll)
            {
                scrollOffset++;
            }
        }
    }
}

// ============================================================================
// Scroll
// ============================================================================

void Cortina::scroll(int direction)
{
    if (!isOpen)
        return;

    if (direction > 0)
    {
        if (scrollOffset > 0)
        {
            scrollOffset--;
        }
    }
    else if (direction < 0)
    {
        int max_scroll =
            (int)options.size() -
            maxVisibleItems;

        if (max_scroll > 0 &&
            scrollOffset < max_scroll)
        {
            scrollOffset++;
        }
    }
}

// ============================================================================
// Hover
// ============================================================================

void Cortina::updateHover(int mx, int my)
{
    mouseX = mx;
    mouseY = my;
}

// ============================================================================
// Selected Text
// ============================================================================

std::string Cortina::getSelectedText() const
{
    if (selectedIndex >= 0 &&
        selectedIndex < (int)options.size())
    {
        return options[selectedIndex];
    }

    return "";
}