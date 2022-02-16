#ifndef RENDER_GRAPH_H
#define RENDER_GRAPH_H

#include "common.h"
#include "rhi.h"
#include "mesh.h"

#define DECLARE_NODE_OUTPUT(index) ((~(1u << 31u)) & index)
#define DECLARE_NODE_INPUT(index) ((1u << 31u) | index)
#define IS_NODE_INPUT(id) (((1u << 31u) & id) > 0)
#define GET_NODE_PORT_INDEX(id) (((1u << 31u) - 1u) & id)
#define RENDER_GRAPH_MAX_DRAWABLES 512
#define RENDER_GRAPH_MAX_LIGHTS 512

typedef struct render_graph_execute render_graph_execute;
typedef struct render_graph_node render_graph_node;
typedef struct render_graph_node_input render_graph_node_input;
typedef struct render_graph render_graph;
typedef struct render_graph_drawable render_graph_drawable;
typedef struct render_graph_point_light render_graph_point_light;

struct render_graph_drawable
{
    mesh m;
    hmm_mat4 model_matrix;
};

struct render_graph_point_light
{
    hmm_vec3 position;
    f32 pad;
    hmm_vec3 color;
    f32 pad2;
};

struct render_graph_execute
{
    render_graph_drawable drawables[RENDER_GRAPH_MAX_DRAWABLES];
    i32 drawable_count;

    u32 width;
    u32 height;

    rhi_descriptor_heap image_heap;
    rhi_descriptor_heap sampler_heap;

    rhi_buffer camera_buffer;
    rhi_descriptor_set camera_descriptor_set;
    rhi_descriptor_set_layout camera_descriptor_set_layout;

    rhi_buffer light_buffer;
    rhi_descriptor_set light_descriptor_set;
    rhi_descriptor_set_layout light_descriptor_set_layout;
    
    struct {
        render_graph_point_light lights[RENDER_GRAPH_MAX_LIGHTS];
        i32 light_count;
        hmm_vec3 pad;
    } light_info;

    struct {
        hmm_mat4 projection;
        hmm_mat4 view;
        hmm_vec3 pos;
        f32 pad;
    } camera;
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

void init_render_graph(render_graph* graph, render_graph_execute* execute);
void connect_render_graph_nodes(render_graph* graph, u32 src_id, u32 dst_id, render_graph_node* src_node, render_graph_node* dst_node);
rhi_image* get_render_graph_node_input_image(render_graph_node_input* input);
void bake_render_graph(render_graph* graph, render_graph_execute* execute, render_graph_node* last_node);
void free_render_graph(render_graph* graph, render_graph_execute* execute);
void resize_render_graph(render_graph* graph, render_graph_execute* execute);
void update_render_graph(render_graph* graph, render_graph_execute* execute);

#endif