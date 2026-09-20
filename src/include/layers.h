/*
 * layers.h
 */

#ifndef LAYERS_H_
#define LAYERS_H_

#include <vector>
#include <array>
#include <iostream>
#include <glm/glm.hpp>
#include "vslider.h"
#include "radio.h"
#include "model/simulation.h"
#include "text_renderer.h"
#include "projection_manager.h"
#include "globals.h"

namespace automaton
{
    extern unsigned CENTER;
    extern unsigned W_USED;
    extern std::vector<Cell> lattice_curr;
    extern std::vector<std::array<unsigned, 3>> lcenters;
}

namespace framework
{
    extern VSlider vslider;  // defined in your main .cpp

    class LayerList
    {
    public:
        LayerList(unsigned wDim, int visibleLayers = 25)
            : wDim_(wDim), visibleLayers_(visibleLayers), lastFirstIndex_(-1)
        {
            lastCellCoordinates_.resize(wDim_);
            for (unsigned w = 0; w < wDim_; ++w)
                lastCellCoordinates_[w] = {automaton::CENTER, automaton::CENTER, automaton::CENTER};

            char buf[32];
            for (unsigned w = 0; w < wDim_; ++w)
            {
                snprintf(buf, sizeof(buf), "Layer %2u", w);
                layers_.emplace_back(1680, 145 + 25 * static_cast<int>(w), buf);
                layers_.back().setFontScale(0.58f);
            }
            if (!layers_.empty())
                layers_[0].setSelected(true);
        }

        int getSelected() const
        {
            for (size_t i = 0; i < layers_.size(); ++i)
                if (layers_[i].isSelected())
                    return static_cast<int>(i);
            return -1;
        }

        void update(TextRenderer& renderer)
        {
            const int first = vslider.getFirstVisibleIndex(wDim_, visibleLayers_);

            for (int i = 0; i < visibleLayers_ && (first + i) < static_cast<int>(wDim_); ++i)
            {
                unsigned w = first + i;

                const auto& center = automaton::lcenters[w];
                const automaton::Cell& cell = getCell(automaton::lattice_curr,
                                           center[0], center[1], center[2], w);

                bool changed = (cell.x[0] != lastCellCoordinates_[w][0] ||
                                cell.x[1] != lastCellCoordinates_[w][1] ||
                                cell.x[2] != lastCellCoordinates_[w][2]);

                glm::vec3 color = changed ? glm::vec3(1.0f, 0.0f, 1.0f)   // magenta = changed
                                          : glm::vec3(1.0f, 1.0f, 0.0f);  // yellow = same

                char buf[64];
                snprintf(buf, sizeof(buf), "(%u,%u,%u)", cell.x[0], cell.x[1], cell.x[2]);

                // Radio buttons are at Y (from top): 145 + 25 * i
                // TextRenderer uses Y from bottom, so convert:
                // Y_bottom = viewport_height - Y_top
                float radioY_top = 145.0f + 25.0f * i;
                float textY = gViewport[3] - radioY_top;

                float textX = 1780.0f;

                // Render coordinates text to the right of the radio button
                renderer.RenderText(buf, textX, textY, 0.58f, color,
                                   gViewport[2], gViewport[3]);

                lastCellCoordinates_[w] = {cell.x[0], cell.x[1], cell.x[2]};
            }
        }

        void render(TextRenderer& renderer)
        {
            const int first = vslider.getFirstVisibleIndex(wDim_, visibleLayers_);

            for (int i = 0; i < visibleLayers_ && (first + i) < static_cast<int>(wDim_); ++i)
            {
                unsigned w = first + i;

                // Radio buttons use their stored position from constructor
                layers_[w].draw(renderer);
            }
        }

        bool poll(int mouseX, int mouseY)
        {
            const int first = vslider.getFirstVisibleIndex(wDim_, visibleLayers_);
            int currentSel = getSelected();

            Radio* clicked = nullptr;

            for (int i = 0; i < visibleLayers_ && (first + i) < static_cast<int>(wDim_); ++i)
            {
                unsigned w = first + i;

                if (layers_[w].contains(mouseX, mouseY))
                {
                    clicked = &layers_[w];
                    break;
                }
            }

            if (clicked)
            {
                if (!clicked->isSelected())
                {
                    if (currentSel != -1)
                        layers_[currentSel].setSelected(false);
                    clicked->setSelected(true);
                }
                lastFirstIndex_ = first;
                return true;
            }

            if (first != lastFirstIndex_ && first < static_cast<int>(wDim_))
            {
                if (currentSel != -1)
                    layers_[currentSel].setSelected(false);
                layers_[first].setSelected(true);
                lastFirstIndex_ = first;
            }

            return false;
        }

    private:
        unsigned wDim_;
        int visibleLayers_;
        std::vector<Radio> layers_;
        std::vector<std::array<unsigned, 3>> lastCellCoordinates_;
        int lastFirstIndex_;
    };

} // namespace framework

#endif // LAYERS_H_
