#include "render_graph.h"

#include <assert.h>

void recursively_add_nodes(RenderGraphNode* node, RenderGraph* graph)
{
    if (node)
    {
        graph->nodes[graph->node_count] = node;
        graph->node_count++;

        for (u32 i = 0; i < node->input_count; i++)
        {
            RenderGraphNode* owner = node->inputs[i].owner;

            if (!owner) break;

            recursively_add_nodes(owner, graph);
        }
    }
}

void init_render_graph(RenderGraph* graph, RenderGraphExecute* execute)
{
    memset(graph, 0, sizeof(RenderGraph));
    memset(&execute->light_info, 0, sizeof(execute->light_info));
    graph->node_count = 0;

    rhi_init_descriptor_heap(&execute->image_heap, DESCRIPTOR_HEAP_IMAGE, 512);
	rhi_init_descriptor_heap(&execute->sampler_heap, DESCRIPTOR_HEAP_SAMPLER, 512);
	mesh_loader_set_texture_heap(&execute->image_heap);
    mesh_loader_set_sampler_heap(&execute->sampler_heap);
	mesh_loader_init(4);

    execute->camera_descriptor_set_layout.binding = 1;
    execute->camera_descriptor_set_layout.descriptor_count = 1;
    execute->camera_descriptor_set_layout.descriptors[0] = DESCRIPTOR_BUFFER;
    rhi_init_descriptor_set_layout(&execute->camera_descriptor_set_layout);

    execute->light_descriptor_set_layout.binding = 2;
    execute->light_descriptor_set_layout.descriptor_count = 1;
    execute->light_descriptor_set_layout.descriptors[0] = DESCRIPTOR_BUFFER;
    rhi_init_descriptor_set_layout(&execute->light_descriptor_set_layout);

    rhi_allocate_buffer(&execute->camera_buffer, sizeof(execute->camera), BUFFER_UNIFORM);
    rhi_allocate_buffer(&execute->light_buffer, sizeof(execute->light_info), BUFFER_UNIFORM);
    
    rhi_init_descriptor_set(&execute->camera_descriptor_set, &execute->camera_descriptor_set_layout);
    rhi_descriptor_set_write_buffer(&execute->camera_descriptor_set, &execute->camera_buffer, sizeof(execute->camera), 0);

    rhi_init_descriptor_set(&execute->light_descriptor_set, &execute->light_descriptor_set_layout);
    rhi_descriptor_set_write_buffer(&execute->light_descriptor_set, &execute->light_buffer, sizeof(execute->light_info), 0);
}

void connect_render_graph_nodes(RenderGraph* graph, u32 src_id, u32 dst_id, RenderGraphNode* src_node, RenderGraphNode* dst_node)
{
    assert(src_node);
    assert(dst_node);
    assert(!IS_NODE_INPUT(src_id));
    assert(IS_NODE_INPUT(dst_id));

    dst_node->inputs[GET_NODE_PORT_INDEX(dst_id)].owner = src_node;
    dst_node->inputs[GET_NODE_PORT_INDEX(dst_id)].index = GET_NODE_PORT_INDEX(src_id);
    dst_node->input_count++;
}

RHI_Image* get_render_graph_node_input_image(RenderGraphNode_input* input)
{
    assert(input->owner);
    return &input->owner->outputs[input->index];
}

void bake_render_graph(RenderGraph* graph, RenderGraphExecute* execute, RenderGraphNode* last_node)
{
    RenderGraph temp;
    temp.node_count = 0;
    recursively_add_nodes(last_node, &temp);

    for (i32 i = temp.node_count - 1; i >= 0; --i)
    {
        RenderGraphNode* node = temp.nodes[i];
        b32 already_in_graph = 0;

        for (u32 j = 0; j < graph->node_count; j++)
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
        RenderGraphNode* node = graph->nodes[i];
        node->init(node, execute);
    }
}

void free_render_graph(RenderGraph* graph, RenderGraphExecute* execute)
{
    for (u32 i = 0; i < graph->node_count; i++)
        graph->nodes[i]->free(graph->nodes[i], execute);

    rhi_free_buffer(&execute->light_buffer);
    rhi_free_descriptor_set(&execute->light_descriptor_set);
    rhi_free_descriptor_set_layout(&execute->light_descriptor_set_layout);

    rhi_free_buffer(&execute->camera_buffer);
    rhi_free_descriptor_set(&execute->camera_descriptor_set);
    rhi_free_descriptor_set_layout(&execute->camera_descriptor_set_layout);
}

void resize_render_graph(RenderGraph* graph, RenderGraphExecute* execute)
{
    for (u32 i = 0; i < graph->node_count; i++)
        graph->nodes[i]->resize(graph->nodes[i], execute);
}

void update_render_graph(RenderGraph* graph, RenderGraphExecute* execute)
{
    rhi_upload_buffer(&execute->camera_buffer, &execute->camera, sizeof(execute->camera));
    rhi_upload_buffer(&execute->light_buffer, &execute->light_info, sizeof(execute->light_info));

    for (u32 i = 0; i < graph->node_count; i++)
        graph->nodes[i]->update(graph->nodes[i], execute);
}
