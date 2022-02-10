#include "render_graph.h"

#include <assert.h>

void recursively_add_nodes(render_graph_node* node, render_graph* graph)
{
    if (node)
    {
        graph->nodes[graph->node_count] = node;
        graph->node_count++;

        for (u32 i = 0; i < node->input_count; i++)
        {
            render_graph_node* owner = node->inputs[i].owner;

            if (!owner) break;

            recursively_add_nodes(owner, graph);
        }
    }
}

void init_render_graph(render_graph* graph)
{
    memset(graph, 0, sizeof(render_graph));
    graph->node_count = 0;
}

void connect_render_graph_nodes(render_graph* graph, u32 src_id, u32 dst_id, render_graph_node* src_node, render_graph_node* dst_node)
{
    assert(src_node);
    assert(dst_node);
    assert(!IS_NODE_INPUT(src_id));
    assert(IS_NODE_INPUT(dst_id));

    dst_node->inputs[GET_NODE_PORT_INDEX(dst_id)].owner = src_node;
    dst_node->inputs[GET_NODE_PORT_INDEX(dst_id)].index = GET_NODE_PORT_INDEX(src_id);
    dst_node->input_count++;
}

rhi_image* get_render_graph_node_input_image(render_graph_node_input* input)
{
    assert(input->owner);
    return &input->owner->outputs[input->index];
}

void bake_render_graph(render_graph* graph, render_graph_execute* execute, render_graph_node* last_node)
{
    render_graph temp;
    temp.node_count = 0;
    recursively_add_nodes(last_node, &temp);

    for (i32 i = temp.node_count - 1; i >= 0; --i)
    {
        render_graph_node* node = temp.nodes[i];
        b32 already_in_graph = 0;

        for (u32 j = 0; j < graph->node_count; i++)
        {
            if (graph->nodes[i] == node)
            {
                already_in_graph = 1;
                break;
            }
        }

        if (!already_in_graph)
        {
            graph->nodes[graph->node_count] = node;
            graph->node_count++;
        }
    }

    for (u32 i = 0; i < graph->node_count; i++)
    {
        render_graph_node* node = graph->nodes[i];
        node->init(node, execute);
    }
}

void free_render_graph(render_graph* graph, render_graph_execute* execute)
{
    for (u32 i = 0; i < graph->node_count; i++)
        graph->nodes[i]->free(graph->nodes[i], execute);
}

void resize_render_graph(render_graph* graph, render_graph_execute* execute)
{
    for (u32 i = 0; i < graph->node_count; i++)
        graph->nodes[i]->resize(graph->nodes[i], execute);
}

void update_render_graph(render_graph* graph, render_graph_execute* execute)
{
    for (u32 i = 0; i < graph->node_count; i++)
        graph->nodes[i]->update(graph->nodes[i], execute);
}
