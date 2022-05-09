#include "geometry_pass.h"

#include <core/platform_layer.h>

typedef struct geometry_pass geometry_pass;
struct geometry_pass
{
    struct {
        b32 show_meshlets;
        b32 shade_meshlets;
        hmm_vec2 pad;
    } parameters;

    b32 first_render;

    RHI_Sampler nearest_sampler;
    RHI_Sampler linear_sampler;
    RHI_Sampler cubemap_sampler;

    RHI_Pipeline cubemap_pipeline;
    RHI_Pipeline gbuffer_pipeline;
    RHI_Pipeline deferred_pipeline;

    RHI_Image hdr_cubemap;
    RHI_Image cubemap;

    RHI_Image gPosition;
    RHI_Image gNormal;
    RHI_Image gAlbedo;
    RHI_Image gMetallicRoughness;

    RHI_Buffer screen_vertex_buffer;
    RHI_Buffer render_params_buffer;

    RHI_DescriptorSetLayout cubemap_set_layout;
    RHI_DescriptorSet cubemap_set;

    RHI_DescriptorSetLayout params_set_layout;
    RHI_DescriptorSet params_set;

    RHI_DescriptorSetLayout deferred_set_layout;
    RHI_DescriptorSet deferred_set;
};

void geometry_pass_init(RenderGraphNode* node, RenderGraphExecute* execute)
{
    geometry_pass* data = node->private_data;
    data->parameters.show_meshlets = 0;
    data->parameters.shade_meshlets = 0;
    
    f32 quad_vertices[] = {
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
	};

    rhi_allocate_buffer(&data->screen_vertex_buffer, sizeof(quad_vertices), BUFFER_VERTEX);
    rhi_upload_buffer(&data->screen_vertex_buffer, quad_vertices, sizeof(quad_vertices));

    rhi_allocate_buffer(&data->render_params_buffer, sizeof(data->parameters), BUFFER_UNIFORM);

    data->nearest_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	data->nearest_sampler.filter = VK_FILTER_NEAREST;
	rhi_init_sampler(&data->nearest_sampler, 1);
	rhi_push_descriptor_heap_sampler(&execute->sampler_heap, &data->nearest_sampler, 0);

    data->linear_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	data->linear_sampler.filter = VK_FILTER_LINEAR;
	rhi_init_sampler(&data->linear_sampler, 1);
	rhi_push_descriptor_heap_sampler(&execute->sampler_heap, &data->linear_sampler, 1);

    data->cubemap_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    data->cubemap_sampler.filter = VK_FILTER_LINEAR;
    rhi_init_sampler(&data->cubemap_sampler, 1);

    rhi_load_hdr_image(&data->hdr_cubemap, "assets/env_map.hdr");
    rhi_allocate_cubemap(&data->cubemap, 512, 512, VK_FORMAT_R16G16B16A16_UNORM, IMAGE_STORAGE);

    rhi_allocate_image(&data->gPosition, execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_GBUFFER);
    rhi_allocate_image(&data->gNormal, execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_GBUFFER);
    rhi_allocate_image(&data->gAlbedo, execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_GBUFFER);
    rhi_allocate_image(&data->gMetallicRoughness, execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_GBUFFER);
    rhi_allocate_image(&node->outputs[0], execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_RTV);
    rhi_allocate_image(&node->outputs[1], execute->width, execute->height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    node->output_count = 2;

    RHI_CommandBuffer cmd_buf;
    rhi_init_cmd_buf(&cmd_buf, COMMAND_BUFFER_COMPUTE);
    rhi_begin_cmd_buf(&cmd_buf);

    {
        data->cubemap_set_layout.binding = 0;
        data->cubemap_set_layout.descriptors[0] = DESCRIPTOR_STORAGE_IMAGE;
        data->cubemap_set_layout.descriptors[1] = DESCRIPTOR_STORAGE_IMAGE;
        data->cubemap_set_layout.descriptor_count = 2;
        rhi_init_descriptor_set_layout(&data->cubemap_set_layout);

        rhi_init_descriptor_set(&data->cubemap_set, &data->cubemap_set_layout);

        {
            RHI_ShaderModule cs;

            rhi_load_shader(&cs, "shaders/equirectangular_cubemap.comp.spv");

            RHI_PipelineDescriptor descriptor;
            descriptor.use_mesh_shaders = 0;
            descriptor.push_constant_size = 0;
            descriptor.set_layouts[0] = &data->cubemap_set_layout;
            descriptor.set_layout_count = 1;
            descriptor.shaders.cs = &cs;
            descriptor.depth_biased_enable = 0;

            rhi_init_compute_pipeline(&data->cubemap_pipeline, &descriptor);

            rhi_free_shader(&cs);
        }

        // equi to cubemap compute

        rhi_cmd_img_transition_layout(&cmd_buf, &data->hdr_cubemap, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
        rhi_cmd_img_transition_layout(&cmd_buf, &data->cubemap, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

        rhi_descriptor_set_write_storage_image(&data->cubemap_set, &data->hdr_cubemap, &data->nearest_sampler, 0);
        rhi_descriptor_set_write_storage_image(&data->cubemap_set, &data->cubemap, &data->cubemap_sampler, 1);

        rhi_cmd_set_pipeline(&cmd_buf, &data->cubemap_pipeline);
        rhi_cmd_set_descriptor_set(&cmd_buf, &data->cubemap_pipeline, &data->cubemap_set, 0);
        rhi_cmd_dispatch(&cmd_buf, 1024 / 32, 1024 / 32, 6);

        rhi_submit_cmd_buf(&cmd_buf);
        rhi_free_cmd_buf(&cmd_buf);
    }

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

        data->params_set_layout.binding = 0;
        data->params_set_layout.descriptors[0] = DESCRIPTOR_BUFFER;
        data->params_set_layout.descriptor_count = 1;
        rhi_init_descriptor_set_layout(&data->params_set_layout);

        rhi_init_descriptor_set(&data->params_set, &data->params_set_layout);
        rhi_descriptor_set_write_buffer(&data->params_set, &data->render_params_buffer, sizeof(data->parameters), 0);
    }

    {
        RHI_ShaderModule ts;
        RHI_ShaderModule ms;
        RHI_ShaderModule fs;

        rhi_load_shader(&ts, "shaders/gbuffer.task.spv");
        rhi_load_shader(&ms, "shaders/gbuffer.mesh.spv");
        rhi_load_shader(&fs, "shaders/gbuffer.frag.spv");

        RHI_PipelineDescriptor descriptor;
        descriptor.use_mesh_shaders = 1;
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
        descriptor.set_layouts[4] = mesh_loader_get_geometry_descriptor_set_layout();
        descriptor.set_layouts[5] = &data->params_set_layout;
        descriptor.set_layout_count = 6;
        descriptor.shaders.ts = &ts;
        descriptor.shaders.ms = &ms;
        descriptor.shaders.ps = &fs;
        descriptor.depth_biased_enable = 0;

        rhi_init_graphics_pipeline(&data->gbuffer_pipeline, &descriptor);

        rhi_free_shader(&ts);
        rhi_free_shader(&ms);
        rhi_free_shader(&fs);
    }

    {
        RHI_ShaderModule vs;
        RHI_ShaderModule fs;

        rhi_load_shader(&vs, "shaders/deferred.vert.spv");
		rhi_load_shader(&fs, "shaders/deferred.frag.spv");

        RHI_PipelineDescriptor descriptor;
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
        descriptor.set_layouts[3] = &data->params_set_layout;
        descriptor.set_layout_count = 4;
        descriptor.shaders.vs = &vs;
        descriptor.shaders.ps = &fs;
        descriptor.depth_biased_enable = 0;

        rhi_init_graphics_pipeline(&data->deferred_pipeline, &descriptor);

        rhi_free_shader(&vs);
        rhi_free_shader(&fs);
    }

    data->first_render = 1;
}

void geometry_pass_update(RenderGraphNode* node, RenderGraphExecute* execute)
{
    geometry_pass* data = node->private_data;

    if (aurora_platform_key_pressed(KEY_Y))
        data->parameters.show_meshlets = 1;
    if (aurora_platform_key_pressed(KEY_N))
        data->parameters.show_meshlets = 0;
    if (aurora_platform_key_pressed(KEY_O))
        data->parameters.shade_meshlets = 1;
    if (aurora_platform_key_pressed(KEY_P))
        data->parameters.shade_meshlets = 0;

    RHI_CommandBuffer* cmd_buf = rhi_get_swapchain_cmd_buf();
    rhi_upload_buffer(&data->render_params_buffer, &data->parameters, sizeof(data->parameters));

    {
        RHI_RenderBegin begin;
        memset(&begin, 0, sizeof(RHI_RenderBegin));
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
        rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[1], 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0);

        rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);
        rhi_cmd_set_pipeline(cmd_buf, &data->gbuffer_pipeline);
        rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &execute->camera_descriptor_set, 0);
        rhi_cmd_set_descriptor_heap(cmd_buf, &data->gbuffer_pipeline, &execute->image_heap, 1);
        rhi_cmd_set_descriptor_heap(cmd_buf, &data->gbuffer_pipeline, &execute->sampler_heap, 2);
        rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &data->params_set, 5);

        for (i32 i = 0; i < execute->drawable_count; i++)
        {
            RenderGraphDrawable* drawable = &execute->drawables[i];
            for (u32 i = 0; i < drawable->m.primitive_count; i++)
	        {
	        	rhi_cmd_set_push_constants(cmd_buf, &data->gbuffer_pipeline, &drawable->model_matrix, sizeof(hmm_mat4));
                rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &drawable->m.materials[drawable->m.primitives[i].material_index].material_set, 3);
                rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &drawable->m.primitives[i].geometry_descriptor_set, 4);
	        	rhi_cmd_draw_meshlets(cmd_buf, drawable->m.primitives[i].meshlet_count);
	        }
        }

        rhi_cmd_img_transition_layout(cmd_buf, &data->gPosition, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gNormal, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gAlbedo, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
        rhi_cmd_img_transition_layout(cmd_buf, &data->gMetallicRoughness, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);

        rhi_cmd_end_render(cmd_buf);
    }

    {
        RHI_RenderBegin begin;
        memset(&begin, 0, sizeof(RHI_RenderBegin));
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
        rhi_cmd_set_descriptor_set(cmd_buf, &data->deferred_pipeline, &data->params_set, 3);
        rhi_cmd_set_push_constants(cmd_buf, &data->deferred_pipeline, &temp, sizeof(hmm_vec4));
        rhi_cmd_set_vertex_buffer(cmd_buf, &data->screen_vertex_buffer);
        rhi_cmd_draw(cmd_buf, 4);
        rhi_cmd_end_render(cmd_buf);

        rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    }
}

void geometry_pass_resize(RenderGraphNode* node, RenderGraphExecute* execute)
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

void geometry_pass_free(RenderGraphNode* node, RenderGraphExecute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_free_descriptor_set(&data->cubemap_set);
    rhi_free_descriptor_set_layout(&data->cubemap_set_layout);
    rhi_free_pipeline(&data->cubemap_pipeline);
    rhi_free_image(&data->cubemap);
    rhi_free_image(&data->hdr_cubemap);

    rhi_free_image(&data->gPosition);
    rhi_free_image(&data->gNormal);
    rhi_free_image(&data->gAlbedo);
    rhi_free_image(&data->gMetallicRoughness);
    rhi_free_image(&node->outputs[1]);
    rhi_free_image(&node->outputs[0]);
    rhi_free_pipeline(&data->deferred_pipeline);
    rhi_free_pipeline(&data->gbuffer_pipeline);
    rhi_free_sampler(&data->cubemap_sampler);
    rhi_free_sampler(&data->linear_sampler);
    rhi_free_sampler(&data->nearest_sampler);
    rhi_free_descriptor_set(&data->params_set);
    rhi_free_descriptor_set_layout(&data->params_set_layout);
    rhi_free_descriptor_set(&data->deferred_set);
    rhi_free_descriptor_set_layout(&data->deferred_set_layout);
    rhi_free_buffer(&data->screen_vertex_buffer);
    rhi_free_buffer(&data->render_params_buffer);

    free(data);
}

RenderGraphNode* create_geometry_pass()
{
    RenderGraphNode* node = malloc(sizeof(RenderGraphNode));

    node->init = geometry_pass_init;
    node->free = geometry_pass_free;
    node->resize = geometry_pass_resize;
    node->update = geometry_pass_update;
    node->private_data = malloc(sizeof(geometry_pass));
    node->input_count = 0;
    memset(node->inputs, 0, sizeof(node->inputs));

    return node;
}