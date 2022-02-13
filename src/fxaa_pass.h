#ifndef FXAA_PASS_H_INCLUDED
#define FXAA_PASS_H_INCLUDED

#include "render_graph.h"

enum fxaa_pass_input
{
    fxaa_pass_input_color = DECLARE_NODE_INPUT(0)
};

enum fxaa_pass_output
{
    fxaa_pass_output_anti_aliased = DECLARE_NODE_OUTPUT(0)
};

render_graph_node* create_fxaa_pass();

#endif