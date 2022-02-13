#ifndef GEOMETRY_PASS_H_INCLUDED
#define GEOMETRY_PASS_H_INCLUDED

#include "render_graph.h"

enum geometry_pass_output
{
    geometry_pass_output_lit = DECLARE_NODE_OUTPUT(0)
};

render_graph_node* create_geometry_pass();

#endif