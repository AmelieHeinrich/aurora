#include "geometry_pass.h"

typedef struct geometry_pass geometry_pass;
struct geometry_pass
{
    b32 first_render;

    rhi_sampler nearest_sampler;
    rhi_sampler linear_sampler;

    rhi_pipeline gbuffer_pipeline;
    rhi_pipeline deferred_pipeline;

    rhi_image gPosition;
    rhi_image gNormal;
    rhi_image gAlbedo;
    rhi_image gMetallicRoughness;

    rhi_buffer screen_vertex_buffer;

    rhi_descriptor_set_layout deferred_set_layout;
    rhi_descriptor_set deferred_set;
};

void geometry_pass_init(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;
    
    f32 quad_vertices[] = {
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};

    rhi_allocate_buffer(&data->screen_vertex_buffer, sizeof(quad_vertices), BUFFER_VERTEX);
    rhi_upload_buffer(&data->screen_vertex_buffer, quad_vertices, sizeof(quad_vertices));

    data->nearest_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	data->nearest_sampler.filter = VK_FILTER_NEAREST;
	rhi_init_sampler(&data->nearest_sampler);
	rhi_push_descriptor_heap_sampler(&execute->sampler_heap, &data->nearest_sampler, 0);

    data->linear_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	data->linear_sampler.filter = VK_FILTER_LINEAR;
	rhi_init_sampler(&data->linear_sampler);
	rhi_push_descriptor_heap_sampler(&execute->sampler_heap, &data->linear_sampler, 1);

    rhi_allocate_image(&data->gPosition, execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_GBUFFER);
    rhi_allocate_image(&data->gNormal, execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_GBUFFER);
    rhi_allocate_image(&data->gAlbedo, execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_GBUFFER);
    rhi_allocate_image(&data->gMetallicRoughness, execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_GBUFFER);
    rhi_allocate_image(&node->outputs[0], execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_RTV);
    rhi_allocate_image(&node->outputs[1], execute->width, execute->height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    node->output_count = 2;

    {
        data->deferred_set_layout.binding = 0;
        data->deferred_set_layout.descriptors[0] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[1] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[2] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[3] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptor_count = 4;
        rhi_init_descriptor_set_layout(&data->deferred_set_layout);

        rhi_init_descriptor_set(&data->deferred_set, &data->deferred_set_layout);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gPosition, 0);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gNormal, 1);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gAlbedo, 2);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gMetallicRoughness, 3);
    }

    {
        rhi_shader_module vs;
        rhi_shader_module fs;

        rhi_load_shader(&vs, "shaders/gbuffer.vert.spv");
        rhi_load_shader(&fs, "shaders/gbuffer.frag.spv");

        rhi_pipeline_descriptor descriptor;
        descriptor.use_mesh_shaders = 0;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.color_attachments_formats[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.color_attachments_formats[2] = VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.color_attachments_formats[3] = VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.color_attachment_count = 4;
        descriptor.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
        descriptor.cull_mode = VK_CULL_MODE_BACK_BIT;
        descriptor.depth_op = VK_COMPARE_OP_LESS;
        descriptor.polygon_mode = VK_POLYGON_MODE_FILL;
        descriptor.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        descriptor.push_constant_size = sizeof(hmm_mat4);
        descriptor.set_layouts[0] = &execute->camera_descriptor_set_layout;
        descriptor.set_layouts[1] = rhi_get_image_heap_set_layout();
        descriptor.set_layouts[2] = rhi_get_sampler_heap_set_layout();
        descriptor.set_layouts[3] = mesh_loader_get_descriptor_set_layout();
        descriptor.set_layout_count = 4;
        descriptor.shaders.vs = &vs;
        descriptor.shaders.ps = &fs;

        rhi_init_graphics_pipeline(&data->gbuffer_pipeline, &descriptor);

        rhi_free_shader(&vs);
        rhi_free_shader(&fs);
    }

    {
        rhi_shader_module vs;
        rhi_shader_module fs;

        rhi_load_shader(&vs, "shaders/deferred.vert.spv");
		rhi_load_shader(&fs, "shaders/deferred.frag.spv");

        rhi_pipeline_descriptor descriptor;
        descriptor.use_mesh_shaders = 0;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.color_attachment_count = 1;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
        descriptor.cull_mode = VK_CULL_MODE_BACK_BIT;
        descriptor.depth_op = VK_COMPARE_OP_LESS;
        descriptor.polygon_mode = VK_POLYGON_MODE_FILL;
        descriptor.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        descriptor.push_constant_size = sizeof(hmm_vec4);
        descriptor.set_layouts[0] = &data->deferred_set_layout;
        descriptor.set_layouts[1] = rhi_get_sampler_heap_set_layout();
        descriptor.set_layouts[2] = &execute->light_descriptor_set_layout;
        descriptor.set_layout_count = 3;
        descriptor.shaders.vs = &vs;
        descriptor.shaders.ps = &fs;

        rhi_init_graphics_pipeline(&data->deferred_pipeline, &descriptor);

        rhi_free_shader(&vs);
        rhi_free_shader(&fs);
    }

    data->first_render = 1;
}

void geometry_pass_update(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_command_buf* cmd_buf = rhi_get_swapchain_cmd_buf();

    {
        rhi_render_begin begin;
        memset(&begin, 0, sizeof(rhi_render_begin));
        begin.r = 0.1f;
	    begin.g = 0.1f;
	    begin.b = 0.1f;
	    begin.a = 1.0f;
	    begin.has_depth = 1;
	    begin.width = execute->width;
	    begin.height = execute->height;
	    begin.images[0] = &data->gPosition;
	    begin.images[1] = &data->gNormal;
        begin.images[2] = &data->gAlbedo;
	    begin.images[3] = &data->gMetallicRoughness;
        begin.images[4] = &node->outputs[1];
	    begin.image_count = 5;

        u32 src_layout = data->first_render == 1 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
        rhi_cmd_start_render(cmd_buf, begin);

        rhi_cmd_img_transition_layout(cmd_buf, &data->gPosition, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gNormal, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gAlbedo, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gMetallicRoughness, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
        rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);
        rhi_cmd_set_pipeline(cmd_buf, &data->gbuffer_pipeline);
        rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &execute->camera_descriptor_set, 0);
        rhi_cmd_set_descriptor_heap(cmd_buf, &data->gbuffer_pipeline, &execute->image_heap, 1);
        rhi_cmd_set_descriptor_heap(cmd_buf, &data->gbuffer_pipeline, &execute->sampler_heap, 2);

        for (i32 i = 0; i < execute->drawable_count; i++)
        {
            render_graph_drawable* drawable = &execute->drawables[i];
            for (u32 i = 0; i < drawable->m.primitive_count; i++)
	        {
	        	rhi_cmd_set_push_constants(cmd_buf, &data->gbuffer_pipeline, &drawable->model_matrix, sizeof(hmm_mat4));
	        	rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &drawable->m.materials[drawable->m.primitives[i].material_index].material_set, 3);
	        	rhi_cmd_set_vertex_buffer(cmd_buf, &drawable->m.primitives[i].vertex_buffer);
	        	rhi_cmd_set_index_buffer(cmd_buf, &drawable->m.primitives[i].index_buffer);
	        	rhi_cmd_draw_indexed(cmd_buf, drawable->m.primitives[i].index_count);
	        }
        }

        rhi_cmd_img_transition_layout(cmd_buf, &data->gPosition, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gNormal, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gAlbedo, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gMetallicRoughness, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);

        rhi_cmd_end_render(cmd_buf);
    }

    {
        rhi_render_begin begin;
        memset(&begin, 0, sizeof(rhi_render_begin));
	    begin.r = 0.1f;
	    begin.g = 0.1f;
	    begin.b = 0.1f;
	    begin.a = 1.0f;
	    begin.has_depth = 0;
	    begin.width = execute->width;
	    begin.height = execute->height;
	    begin.images[0] = &node->outputs[0];
	    begin.image_count = 1;

        u32 src_layout = data->first_render == 1 ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        data->first_render = 0;

        rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, src_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);

        hmm_vec4 temp = HMM_Vec4(execute->camera.pos.X, execute->camera.pos.Y, execute->camera.pos.Z, 1.0);

        rhi_cmd_start_render(cmd_buf, begin);
        rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);
        rhi_cmd_set_pipeline(cmd_buf, &data->deferred_pipeline);
        rhi_cmd_set_descriptor_set(cmd_buf, &data->deferred_pipeline, &data->deferred_set, 0);
        rhi_cmd_set_descriptor_heap(cmd_buf, &data->deferred_pipeline, &execute->sampler_heap, 1);
        rhi_cmd_set_descriptor_set(cmd_buf, &data->deferred_pipeline, &execute->light_descriptor_set, 2);
        rhi_cmd_set_push_constants(cmd_buf, &data->deferred_pipeline, &temp, sizeof(hmm_vec4));
        rhi_cmd_set_vertex_buffer(cmd_buf, &data->screen_vertex_buffer);
        rhi_cmd_draw(cmd_buf, 4);
        rhi_cmd_end_render(cmd_buf);

        rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    }
}

void geometry_pass_resize(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_resize_image(&data->gPosition, execute->width, execute->height);
    rhi_resize_image(&data->gNormal, execute->width, execute->height);
    rhi_resize_image(&data->gAlbedo, execute->width, execute->height);
    rhi_resize_image(&data->gMetallicRoughness, execute->width, execute->height);
    rhi_resize_image(&node->outputs[0], execute->width, execute->height);
    rhi_resize_image(&node->outputs[1], execute->width, execute->height);
    rhi_descriptor_set_write_image(&data->deferred_set, &data->gPosition, 0);
    rhi_descriptor_set_write_image(&data->deferred_set, &data->gNormal, 1);
    rhi_descriptor_set_write_image(&data->deferred_set, &data->gAlbedo, 2);
    rhi_descriptor_set_write_image(&data->deferred_set, &data->gMetallicRoughness, 3);
    data->first_render = 1;
}

void geometry_pass_free(render_graph_node* node, render_graph_execute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_free_image(&data->gPosition);
    rhi_free_image(&data->gNormal);
    rhi_free_image(&data->gAlbedo);
    rhi_free_image(&data->gMetallicRoughness);
    rhi_free_image(&node->outputs[1]);
    rhi_free_image(&node->outputs[0]);
    rhi_free_pipeline(&data->deferred_pipeline);
    rhi_free_pipeline(&data->gbuffer_pipeline);
    rhi_free_sampler(&data->linear_sampler);
    rhi_free_sampler(&data->nearest_sampler);
    rhi_free_descriptor_set(&data->deferred_set);
    rhi_free_descriptor_set_layout(&data->deferred_set_layout);
    rhi_free_buffer(&data->screen_vertex_buffer);

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