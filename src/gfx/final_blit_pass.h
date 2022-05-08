#ifndef FINAL_BLIT_PASS_H_INCLUDED
#define FINAL_BLIT_PASS_H_INCLUDED

#include <gfx/render_graph.h>

enum FinalBlitPassInput
{
    FinalBlitPassInputImage = DECLARE_NODE_INPUT(0) 
};

RenderGraphNode* create_final_blit_pass();

#endif