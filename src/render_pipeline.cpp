#include "render_pipeline.h"

RenderPipelineState gPipelineState = RenderPipelineState::FULL_VOLUME;

void setPipelineState(RenderPipelineState state)
{
    gPipelineState = state;
}