#ifndef FXAA_PASS_H_INCLUDED
#define FXAA_PASS_H_INCLUDED

#include <gfx/render_graph.h>

enum FXAAPassInput
{
    FXAAPassInputColor = DECLARE_NODE_INPUT(0)
};

enum FXAAPassOutput
{
    FXAAPassOutputAntiAliased = DECLARE_NODE_OUTPUT(0)
};

RenderGraphNode* create_fxaa_pass();

#endif