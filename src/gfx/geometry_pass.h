#ifndef GEOMETRY_PASS_H_INCLUDED
#define GEOMETRY_PASS_H_INCLUDED

#include <gfx/render_graph.h>

enum GeometryPassOutput
{
    GeometryPassOutputLit = DECLARE_NODE_OUTPUT(0)
};

RenderGraphNode* create_geometry_pass();

#endif