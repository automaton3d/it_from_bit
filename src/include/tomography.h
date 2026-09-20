#pragma once

#include <vector>
#include <mutex>
#include <glm/glm.hpp>

namespace tomography
{
    struct VoxelSnapshot
    {
        glm::vec3 position;
        uint32_t color;
    };

    extern std::mutex gVoxelBufferMutex;

    void update();
    void requestUpdate();

    bool isVoxelVisible(unsigned x,
                        unsigned y,
                        unsigned z);

    void renderTomoPlane();
    void renderControls();

    float getSlicePosition();

    void updateSnapshot();

    const std::vector<VoxelSnapshot>&
    getSnapshot();
}