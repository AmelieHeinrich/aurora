#include "geometry_pass.h"

typedef struct geometry_pass geometry_pass;
struct geometry_pass
{
    b32 first_render;

    rhi_pipeline geometry_pipeline;
    rhi_sampler linear_sampler;
};

void geometry_pass_init(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    data->linear_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	data->linear_sampler.filter = VK_FILTER_LINEAR;
	rhi_init_sampler(&data->linear_sampler);
	rhi_push_descriptor_heap_sampler(&execute->sampler_heap, &data->linear_sampler, 0);

    rhi_allocate_image(&node->outputs[0], execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_RTV);
    rhi_allocate_image(&node->outputs[1], execute->width, execute->height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    node->output_count = 2;

    data->first_render = 1;

    {
        rhi_shader_module vs;
        rhi_shader_module fs;

        rhi_load_shader(&vs, "shaders/geometry.vert.spv");
		rhi_load_shader(&fs, "shaders/geometry.frag.spv");

        rhi_pipeline_descriptor descriptor;
        descriptor.use_mesh_shaders = 0;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.color_attachment_count = 1;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
        descriptor.cull_mode = VK_CULL_MODE_BACK_BIT;
        descriptor.depth_op = VK_COMPARE_OP_LESS;
        descriptor.polygon_mode = VK_POLYGON_MODE_FILL;
        descriptor.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        descriptor.push_constant_size = sizeof(hmm_mat4) + sizeof(hmm_vec3);
        descriptor.set_layouts[0] = &execute->camera_descriptor_set_layout;
        descriptor.set_layouts[1] = rhi_get_image_heap_set_layout();
        descriptor.set_layouts[2] = rhi_get_sampler_heap_set_layout();
        descriptor.set_layouts[3] = mesh_loader_get_descriptor_set_layout();
        descriptor.set_layout_count = 4;
        descriptor.shaders.vs = &vs;
        descriptor.shaders.ps = &fs;

        rhi_init_graphics_pipeline(&data->geometry_pipeline, &descriptor);

        rhi_free_shader(&vs);
        rhi_free_shader(&fs);
    }
}

void geometry_pass_update(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_render_begin begin = {0};
	begin.r = 0.1f;
	begin.g = 0.1f;
	begin.b = 0.1f;
	begin.a = 1.0f;
	begin.has_depth = 1;
	begin.width = execute->width;
	begin.height = execute->height;
	begin.images[0] = &node->outputs[0];
	begin.images[1] = &node->outputs[1];
	begin.image_count = 2;

    rhi_command_buf* cmd_buf = rhi_get_swapchain_cmd_buf();

    u32 src_layout = data->first_render == 1 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    data->first_render = 0;

    rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
	rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[1], 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0);

    rhi_cmd_start_render(cmd_buf, begin);
    rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);
    rhi_cmd_set_pipeline(cmd_buf, &data->geometry_pipeline);
    rhi_cmd_set_descriptor_set(cmd_buf, &data->geometry_pipeline, &execute->camera_descriptor_set, 0);
    rhi_cmd_set_descriptor_heap(cmd_buf, &data->geometry_pipeline, &execute->image_heap, 1);
    rhi_cmd_set_descriptor_heap(cmd_buf, &data->geometry_pipeline, &execute->sampler_heap, 2);

    for (i32 i = 0; i < execute->drawable_count; i++)
    {
        render_graph_drawable* drawable = &execute->drawables[i];
        for (u32 i = 0; i < drawable->m.primitive_count; i++)
	    {
	    	rhi_cmd_set_push_constants(cmd_buf, &data->geometry_pipeline, &drawable->model_matrix, sizeof(hmm_mat4));
	    	rhi_cmd_set_descriptor_set(cmd_buf, &data->geometry_pipeline, &drawable->m.materials[drawable->m.primitives[i].material_index].material_set, 3);
	    	rhi_cmd_set_vertex_buffer(cmd_buf, &drawable->m.primitives[i].vertex_buffer);
	    	rhi_cmd_set_index_buffer(cmd_buf, &drawable->m.primitives[i].index_buffer);
	    	rhi_cmd_draw_indexed(cmd_buf, drawable->m.primitives[i].index_count);
	    }
    }

    rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    rhi_cmd_end_render(cmd_buf);
}

void geometry_pass_resize(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_resize_image(&node->outputs[0], execute->width, execute->height);
    rhi_resize_image(&node->outputs[1], execute->width, execute->height);
    data->first_render = 1;
}

void geometry_pass_free(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_free_image(&node->outputs[1]);
    rhi_free_image(&node->outputs[0]);
    rhi_free_pipeline(&data->geometry_pipeline);
    rhi_free_sampler(&data->linear_sampler);

    free(data);
}

render_graph_node* create_geometry_pass()
{
    render_graph_node* node = malloc(sizeof(render_graph_node));

    node->init = geometry_pass_init;
    node->free = geometry_pass_free;
    node->resize = geometry_pass_resize;
    node->update = geometry_pass_update;
    node->private_data = malloc(sizeof(geometry_pass));
    node->input_count = 0;
    memset(node->inputs, 0, sizeof(node->inputs));

    return node;
}