#ifndef FINAL_BLIT_PASS_H_INCLUDED
#define FINAL_BLIT_PASS_H_INCLUDED

#include "render_graph.h"

enum final_blit_pass_input
{
    final_blit_pass_input_image = DECLARE_NODE_INPUT(0) 
};

render_graph_node* create_final_blit_pass();

#endif