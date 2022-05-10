#include "fxaa_pass.h"

typedef struct fxaa_pass_data fxaa_pass_data;
struct fxaa_pass_data
{
    RHI_Pipeline fxaa_pipeline;
    RHI_Buffer screen_vertex_buffer;
    b32 first_render;

    struct {
        hmm_vec2 screen_size;
        hmm_vec2 pad;
    } push_constants;

    RHI_DescriptorSetLayout fxaa_set_layout;
    RHI_DescriptorSet fxaa_set;
};

void fxaa_pass_init(RenderGraphNode* node, RenderGraphExecute* execute)
{
    fxaa_pass_data* data = node->private_data;
    data->first_render = 1;

    rhi_allocate_image(&node->outputs[0], execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_RTV);
    node->output_count = 1;

    {
        data->fxaa_set_layout.binding = 0;
        data->fxaa_set_layout.descriptor_count = 1;
        data->fxaa_set_layout.descriptors[0] = DESCRIPTOR_IMAGE;
        rhi_init_descriptor_set_layout(&data->fxaa_set_layout);

        rhi_init_descriptor_set(&data->fxaa_set, &data->fxaa_set_layout);
        rhi_descriptor_set_write_image(&data->fxaa_set, get_render_graph_node_input_image(&node->inputs[0]), 0);
    }

    {
        RHI_ShaderModule vs;
        RHI_ShaderModule ps;

        rhi_load_shader(&vs, "shaders/fxaa.vert.spv");
        rhi_load_shader(&ps, "shaders/fxaa.frag.spv");

        RHI_PipelineDescriptor descriptor;
        descriptor.reflect_input_layout = 1;
        descriptor.shaders.vs = &vs;
        descriptor.shaders.ps = &ps;
        descriptor.push_constant_size = sizeof(hmm_vec4);
        descriptor.color_attachment_count = 1;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.depth_op = VK_COMPARE_OP_LESS;
        descriptor.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
        descriptor.polygon_mode = VK_POLYGON_MODE_FILL;
        descriptor.cull_mode = VK_CULL_MODE_NONE;
        descriptor.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        descriptor.use_mesh_shaders = 0;
        descriptor.set_layouts[0] = &data->fxaa_set_layout;
        descriptor.set_layouts[1] = rhi_get_sampler_heap_set_layout();
        descriptor.set_layout_count = 2;
        descriptor.depth_biased_enable = 0;

        rhi_init_graphics_pipeline(&data->fxaa_pipeline, &descriptor);

        rhi_free_shader(&vs);
        rhi_free_shader(&ps);
    }

    f32 quad_vertices[] = {
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};

    rhi_allocate_buffer(&data->screen_vertex_buffer, sizeof(quad_vertices), BUFFER_VERTEX);
    rhi_upload_buffer(&data->screen_vertex_buffer, quad_vertices, sizeof(quad_vertices));
}

void fxaa_pass_free(RenderGraphNode* node, RenderGraphExecute* execute)
{
    fxaa_pass_data* data = node->private_data;

    rhi_free_pipeline(&data->fxaa_pipeline);
    rhi_free_buffer(&data->screen_vertex_buffer);
    rhi_free_descriptor_set(&data->fxaa_set);
    rhi_free_descriptor_set_layout(&data->fxaa_set_layout);
    rhi_free_image(&node->outputs[0]);

    free(data);
}

void fxaa_pass_update(RenderGraphNode* node, RenderGraphExecute* execute)
{
    fxaa_pass_data* data = node->private_data;

    RHI_CommandBuffer* cmd_buf = rhi_get_swapchain_cmd_buf();

    data->push_constants.screen_size.X = execute->width;
    data->push_constants.screen_size.Y = execute->height;
    data->push_constants.pad.X = execute->width;
    data->push_constants.pad.Y = execute->height;

    u32 src_layout = data->first_render ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    data->first_render = 0;

    RHI_RenderBegin begin = {0};
	begin.r = 0.0f;
	begin.g = 0.0f;
	begin.b = 0.0f;
	begin.a = 1.0f;
	begin.has_depth = 0;
	begin.width = execute->width;
	begin.height = execute->height;
	begin.images[0] = &node->outputs[0];
	begin.image_count = 1;
    
    rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
    
    rhi_cmd_start_render(cmd_buf, begin);
    rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);
    rhi_cmd_set_pipeline(cmd_buf, &data->fxaa_pipeline);
    rhi_cmd_set_push_constants(cmd_buf, &data->fxaa_pipeline, &data->push_constants, sizeof(data->push_constants));
    rhi_cmd_set_descriptor_set(cmd_buf, &data->fxaa_pipeline, &data->fxaa_set, 0);
    rhi_cmd_set_descriptor_heap(cmd_buf, &data->fxaa_pipeline, &execute->sampler_heap, 1);
    rhi_cmd_set_vertex_buffer(cmd_buf, &data->screen_vertex_buffer);
    rhi_cmd_draw(cmd_buf, 4);
    rhi_cmd_end_render(cmd_buf);

    rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
}

void fxaa_pass_resize(RenderGraphNode* node, RenderGraphExecute* execute)
{
    fxaa_pass_data* data = node->private_data;

    rhi_descriptor_set_write_image(&data->fxaa_set, get_render_graph_node_input_image(&node->inputs[0]), 0);
    rhi_resize_image(&node->outputs[0], execute->width, execute->height);
    data->first_render = 1;
}

RenderGraphNode* create_fxaa_pass()
{
    RenderGraphNode* node = malloc(sizeof(RenderGraphNode));

    node->init = fxaa_pass_init;
    node->free = fxaa_pass_free;
    node->update = fxaa_pass_update;
    node->resize = fxaa_pass_resize;
    node->private_data = malloc(sizeof(fxaa_pass_data));
    node->input_count = 0;
    memset(node->inputs, 0, sizeof(node->inputs));

    return node;
}