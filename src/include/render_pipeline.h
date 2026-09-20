#pragma once

#include <glad/glad.h>

enum class RenderPipelineState
{
    FULL_VOLUME,
    TOMOGRAPHY_XY,
    TOMOGRAPHY_YZ,
    TOMOGRAPHY_ZX
};

extern RenderPipelineState gPipelineState;

void setPipelineState(RenderPipelineState state);