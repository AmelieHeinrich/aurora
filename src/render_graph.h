#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include "common.h"
#include "rhi.h"

#define DECLARE_NODE_INPUT(index) ((~(1u << 31u)) & index)
#define DECLARE_NODE_OUTPUT(index) ((1u << 31u) | index)
#define IS_NODE_INPUT(id) (((1u << 31u) & id) > 0)
#define GET_NODE_PORT_INDEX(id) (((1u << 31u) - 1u) & id)

typedef struct render_graph_execute render_graph_execute;
typedef struct render_graph_node render_graph_node;
typedef struct render_graph_node_input render_graph_node_input;
typedef struct render_graph render_graph;

struct render_graph_execute
{
    u32 width;
    u32 height;
};

struct render_graph_node_input
{
    render_graph_node* owner;
    u32 index;
};  

struct render_graph_node
{
    void* private_data;
    char* name;

    void (*init)(render_graph_node* node, render_graph_execute* execute);
    void (*free)(render_graph_node* node, render_graph_execute* execute);
    void (*update)(render_graph_node* node, render_graph_execute* execute);
    void (*resize)(render_graph_node* node, render_graph_execute* execute);

    rhi_image outputs[32];
    u32 output_count;

    render_graph_node_input inputs[32];
    u32 input_count;
};

struct render_graph
{
    render_graph_node* nodes[32];
    u32 node_count;
};

void init_render_graph(render_graph* graph);
void connect_render_graph_nodes(render_graph* graph, u32 src_id, u32 dst_id, render_graph_node* src_node, render_graph_node* dst_node);
rhi_image* get_render_graph_node_input_image(render_graph_node_input* input);
void bake_render_graph(render_graph* graph, render_graph_execute* execute, render_graph_node* last_node);
void free_render_graph(render_graph* graph, render_graph_execute* execute);
void resize_render_graph(render_graph* graph, render_graph_execute* execute);
void update_render_graph(render_graph* graph, render_graph_execute* execute);

#endif