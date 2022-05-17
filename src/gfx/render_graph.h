#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include <core/common.h>
#include <gfx/rhi.h>
#include <resource/mesh.h>

#define DECLARE_NODE_OUTPUT(index) ((~(1u << 31u)) & index)
#define DECLARE_NODE_INPUT(index) ((1u << 31u) | index)
#define IS_NODE_INPUT(id) (((1u << 31u) & id) > 0)
#define GET_NODE_PORT_INDEX(id) (((1u << 31u) - 1u) & id)
#define RENDER_GRAPH_MAX_MODELS 512
#define RENDER_GRAPH_MAX_LIGHTS 512

typedef struct RenderGraphExecute RenderGraphExecute;
typedef struct RenderGraphNode RenderGraphNode;
typedef struct RenderGraphNode_input RenderGraphNode_input;
typedef struct RenderGraph RenderGraph;
typedef struct RenderGraphPointLight RenderGraphPointLight;

struct RenderGraphPointLight
{
    hmm_vec3 position;
    f32 pad;
    hmm_vec3 color;
    f32 pad2;
};

struct RenderGraphExecute
{
    Mesh models[RENDER_GRAPH_MAX_MODELS];
    i32 model_count;

    u32 width;
    u32 height;

    RHI_DescriptorHeap image_heap;
    RHI_DescriptorHeap sampler_heap;

    RHI_Buffer camera_buffer;
    RHI_DescriptorSet camera_descriptor_set;
    RHI_DescriptorSetLayout camera_descriptor_set_layout;

    RHI_Buffer light_buffer;
    RHI_DescriptorSet light_descriptor_set;
    RHI_DescriptorSetLayout light_descriptor_set_layout;
    
    struct {
        RenderGraphPointLight lights[RENDER_GRAPH_MAX_LIGHTS];
        i32 light_count;
        hmm_vec3 pad;
    } light_info;

    struct {
        hmm_mat4 projection;
        hmm_mat4 view;

        hmm_vec3 pos;
        f32 pad;
        
        hmm_vec4 frustrum_planes[6];
    } camera;

    b32 freeze_frustrum;
};

struct RenderGraphNode_input
{
    RenderGraphNode* owner;
    u32 index;
};  

struct RenderGraphNode
{
    void* private_data;
    char* name;

    void (*init)(RenderGraphNode* node, RenderGraphExecute* execute);
    void (*free)(RenderGraphNode* node, RenderGraphExecute* execute);
    void (*update)(RenderGraphNode* node, RenderGraphExecute* execute);
    void (*resize)(RenderGraphNode* node, RenderGraphExecute* execute);

    RHI_Image outputs[32];
    u32 output_count;

    RenderGraphNode_input inputs[32];
    u32 input_count;
};

struct RenderGraph
{
    RenderGraphNode* nodes[32];
    u32 node_count;
};

void init_render_graph(RenderGraph* graph, RenderGraphExecute* execute);
void connect_render_graph_nodes(RenderGraph* graph, u32 src_id, u32 dst_id, RenderGraphNode* src_node, RenderGraphNode* dst_node);
RHI_Image* get_render_graph_node_input_image(RenderGraphNode_input* input);
void bake_render_graph(RenderGraph* graph, RenderGraphExecute* execute, RenderGraphNode* last_node);
void free_render_graph(RenderGraph* graph, RenderGraphExecute* execute);
void resize_render_graph(RenderGraph* graph, RenderGraphExecute* execute);
void update_render_graph(RenderGraph* graph, RenderGraphExecute* execute);

#endif