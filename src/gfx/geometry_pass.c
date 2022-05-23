#include "geometry_pass.h"

#include <core/platform_layer.h>
#include <stdio.h>

typedef struct geometry_pass geometry_pass;
struct geometry_pass
{
    struct {
        b32 show_meshlets;
        b32 shade_meshlets;
        hmm_vec2 pad;
    } parameters;

    RHI_Sampler nearest_sampler;
    RHI_Sampler linear_sampler;
    RHI_Sampler cubemap_sampler;

    RHI_Pipeline cubemap_pipeline;
    RHI_Pipeline skybox_pipeline;
    RHI_Pipeline irradiance_pipeline;
    RHI_Pipeline prefilter_pipeline;
    RHI_Pipeline brdf_pipeline;
    RHI_Pipeline gbuffer_pipeline;
    RHI_Pipeline deferred_pipeline;

    RHI_Image hdr_cubemap;
    RHI_Image cubemap;
    RHI_Image irradiance;
    RHI_Image prefilter;
    RHI_Image brdf;

    RHI_Image gPosition;
    RHI_Image gNormal;
    RHI_Image gAlbedo;
    RHI_Image gMetallicRoughness;

    RHI_Buffer screen_vertex_buffer;
    RHI_Buffer render_params_buffer;

    RHI_DescriptorSetLayout cubemap_set_layout;
    RHI_DescriptorSet cubemap_set;

    RHI_DescriptorSetLayout irradiance_set_layout;
    RHI_DescriptorSet irradiance_set;

    RHI_DescriptorSetLayout prefilter_set_layout;
    RHI_DescriptorSet prefilter_set;

    RHI_DescriptorSetLayout params_set_layout;
    RHI_DescriptorSet params_set;

    RHI_DescriptorSetLayout deferred_set_layout;
    RHI_DescriptorSet deferred_set;

    RHI_DescriptorSetLayout skybox_set_layout;
    RHI_DescriptorSet skybox_set;

    RHI_DescriptorSetLayout brdf_set_layout;
    RHI_DescriptorSet brdf_set;
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

    RHI_RawImage raw_hdr;
    rhi_load_raw_hdr_image(&raw_hdr, "assets/env_map.hdr");
    
    rhi_upload_image(&data->hdr_cubemap, &raw_hdr, 0);
    rhi_free_raw_image(&raw_hdr);
    rhi_allocate_cubemap(&data->cubemap, 512, 512, VK_FORMAT_R16G16B16A16_UNORM, IMAGE_STORAGE, VK_IMAGE_LAYOUT_GENERAL);
    rhi_allocate_cubemap(&data->irradiance, 128, 128, VK_FORMAT_R16G16B16A16_UNORM, IMAGE_STORAGE, VK_IMAGE_LAYOUT_GENERAL);
    rhi_allocate_cubemap(&data->prefilter, 512, 512, VK_FORMAT_R16G16B16A16_UNORM, IMAGE_STORAGE, VK_IMAGE_LAYOUT_GENERAL);
    rhi_allocate_image(&data->brdf, 512, 512, VK_FORMAT_R16G16_SFLOAT, IMAGE_STORAGE, VK_IMAGE_LAYOUT_GENERAL);

    rhi_allocate_image(&data->gPosition, execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_GBUFFER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rhi_allocate_image(&data->gNormal, execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_GBUFFER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rhi_allocate_image(&data->gAlbedo, execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_GBUFFER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rhi_allocate_image(&data->gMetallicRoughness, execute->width, execute->height, VK_FORMAT_R8G8B8A8_UNORM, IMAGE_GBUFFER, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rhi_allocate_image(&node->outputs[0], execute->width, execute->height, VK_FORMAT_R16G16B16A16_SFLOAT, IMAGE_RTV, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rhi_allocate_image(&node->outputs[1], execute->width, execute->height, VK_FORMAT_D24_UNORM_S8_UINT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    node->output_count = 2;

    RHI_CommandBuffer cmd_buf;
    rhi_init_cmd_buf(&cmd_buf, COMMAND_BUFFER_COMPUTE);

    {
        {
            data->cubemap_set_layout.descriptors[0] = DESCRIPTOR_STORAGE_IMAGE;
            data->cubemap_set_layout.descriptors[1] = DESCRIPTOR_STORAGE_IMAGE;
            data->cubemap_set_layout.descriptor_count = 2;
            rhi_init_descriptor_set_layout(&data->cubemap_set_layout);

            rhi_init_descriptor_set(&data->cubemap_set, &data->cubemap_set_layout);

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

        {
            data->irradiance_set_layout.descriptors[0] = DESCRIPTOR_SAMPLED_IMAGE;
            data->irradiance_set_layout.descriptors[1] = DESCRIPTOR_STORAGE_IMAGE;
            data->irradiance_set_layout.descriptor_count = 2;
            rhi_init_descriptor_set_layout(&data->irradiance_set_layout);

            rhi_init_descriptor_set(&data->irradiance_set, &data->irradiance_set_layout);

            RHI_ShaderModule cs;

            rhi_load_shader(&cs, "shaders/irradiance.comp.spv");

            RHI_PipelineDescriptor descriptor;
            descriptor.push_constant_size = 0;
            descriptor.set_layouts[0] = &data->irradiance_set_layout;
            descriptor.set_layout_count = 1;
            descriptor.shaders.cs = &cs;

            rhi_init_compute_pipeline(&data->irradiance_pipeline, &descriptor);

            rhi_free_shader(&cs);
        }

        {
            data->prefilter_set_layout.descriptors[0] = DESCRIPTOR_SAMPLED_IMAGE;
            data->prefilter_set_layout.descriptors[1] = DESCRIPTOR_STORAGE_IMAGE;
            data->prefilter_set_layout.descriptor_count = 2;
            rhi_init_descriptor_set_layout(&data->prefilter_set_layout);

            rhi_init_descriptor_set(&data->prefilter_set, &data->prefilter_set_layout);

            RHI_ShaderModule cs;

            rhi_load_shader(&cs, "shaders/prefilter.comp.spv");

            RHI_PipelineDescriptor descriptor;
            descriptor.push_constant_size = sizeof(hmm_vec4);
            descriptor.set_layouts[0] = &data->prefilter_set_layout;
            descriptor.set_layout_count = 1;
            descriptor.shaders.cs = &cs;

            rhi_init_compute_pipeline(&data->prefilter_pipeline, &descriptor);

            rhi_free_shader(&cs);
        }

        {
            data->brdf_set_layout.descriptors[0] = DESCRIPTOR_STORAGE_IMAGE;
            data->brdf_set_layout.descriptor_count = 1;
            rhi_init_descriptor_set_layout(&data->brdf_set_layout);

            rhi_init_descriptor_set(&data->brdf_set, &data->brdf_set_layout);

            RHI_ShaderModule cs;

            rhi_load_shader(&cs, "shaders/brdf.comp.spv");

            RHI_PipelineDescriptor descriptor;
            descriptor.push_constant_size = 0;
            descriptor.set_layouts[0] = &data->brdf_set_layout;
            descriptor.set_layout_count = 1;
            descriptor.shaders.cs = &cs;

            rhi_init_compute_pipeline(&data->brdf_pipeline, &descriptor);

            rhi_free_shader(&cs);
        }

        // equi to cubemap compute
        {
            rhi_begin_cmd_buf(&cmd_buf);

            rhi_cmd_img_transition_layout(&cmd_buf, &data->hdr_cubemap, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
            rhi_cmd_img_transition_layout(&cmd_buf, &data->cubemap, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

            rhi_descriptor_set_write_storage_image(&data->cubemap_set, &data->hdr_cubemap, &data->nearest_sampler, 0);
            rhi_descriptor_set_write_storage_image(&data->cubemap_set, &data->cubemap, &data->cubemap_sampler, 1);

            rhi_cmd_set_pipeline(&cmd_buf, &data->cubemap_pipeline);
            rhi_cmd_set_descriptor_set(&cmd_buf, &data->cubemap_pipeline, &data->cubemap_set, 0);
            rhi_cmd_dispatch(&cmd_buf, 1024 / 32, 1024 / 32, 6);
        }

        // irradiance
        {
            rhi_cmd_img_transition_layout(&cmd_buf, &data->cubemap, 0, 0, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0);
            rhi_cmd_img_transition_layout(&cmd_buf, &data->irradiance, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

            rhi_descriptor_set_write_image_sampler(&data->irradiance_set, &data->cubemap, &data->cubemap_sampler, 0);
            rhi_descriptor_set_write_storage_image(&data->irradiance_set, &data->irradiance, &data->cubemap_sampler, 1);

            rhi_cmd_set_pipeline(&cmd_buf, &data->irradiance_pipeline);
            rhi_cmd_set_descriptor_set(&cmd_buf, &data->irradiance_pipeline, &data->irradiance_set, 0);
            rhi_cmd_dispatch(&cmd_buf, 128 / 32, 128 / 32, 6);
        }

        // prefilter
        {
            rhi_cmd_img_transition_layout(&cmd_buf, &data->prefilter, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

            rhi_descriptor_set_write_image_sampler(&data->prefilter_set, &data->cubemap, &data->cubemap_sampler, 0);
            rhi_descriptor_set_write_storage_image(&data->prefilter_set, &data->prefilter, &data->cubemap_sampler, 1);

            rhi_cmd_set_pipeline(&cmd_buf, &data->prefilter_pipeline);
            rhi_cmd_set_descriptor_set(&cmd_buf, &data->prefilter_pipeline, &data->prefilter_set, 0);

            for (u32 i = 0; i < 5; i++)
	        {
	        	u32 mip_width = (u32)(512.0f * pow(0.5f, i));
	        	u32 mip_height = (u32)(512.0f * pow(0.5f, i));
	        	f32 roughness = (f32)i / (f32)(5 - 1);

	        	hmm_vec4 vec;
                vec.X = roughness;

	        	rhi_cmd_set_push_constants(&cmd_buf, &data->prefilter_pipeline, &vec, sizeof(hmm_vec4));
	        	rhi_cmd_dispatch(&cmd_buf, mip_width / 32, mip_height / 32, 6);
	        }
        }

        // brdf
        {
            rhi_cmd_img_transition_layout(&cmd_buf, &data->brdf, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);

            rhi_descriptor_set_write_storage_image(&data->brdf_set, &data->brdf, &data->nearest_sampler, 0);

            rhi_cmd_set_pipeline(&cmd_buf, &data->brdf_pipeline);
            rhi_cmd_set_descriptor_set(&cmd_buf, &data->brdf_pipeline, &data->brdf_set, 0);
            rhi_cmd_dispatch(&cmd_buf, 512 / 32, 512 / 32, 6);
        }

        rhi_submit_cmd_buf(&cmd_buf);
        rhi_free_cmd_buf(&cmd_buf);

        rhi_free_image(&data->hdr_cubemap);
    }

    {
        data->skybox_set_layout.descriptors[0] = DESCRIPTOR_SAMPLER;
        data->skybox_set_layout.descriptors[1] = DESCRIPTOR_IMAGE;
        data->skybox_set_layout.descriptor_count = 2;
        rhi_init_descriptor_set_layout(&data->skybox_set_layout);

        rhi_init_descriptor_set(&data->skybox_set, &data->skybox_set_layout);
        rhi_descriptor_set_write_sampler(&data->skybox_set, &data->cubemap_sampler, 0);
        rhi_descriptor_set_write_image(&data->skybox_set, &data->cubemap, 1);

        RHI_ShaderModule vs;
        RHI_ShaderModule fs;

        rhi_load_shader(&vs, "shaders/skybox.vert.spv");
        rhi_load_shader(&fs, "shaders/skybox.frag.spv");

        RHI_PipelineDescriptor descriptor;
        descriptor.use_mesh_shaders = 0;
        descriptor.reflect_input_layout = 0;
        descriptor.depth_bounds_enable = 1;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.color_attachment_count = 1;
        descriptor.depth_attachment_format = VK_FORMAT_D24_UNORM_S8_UINT;
        descriptor.cull_mode = VK_CULL_MODE_NONE;
        descriptor.depth_op = VK_COMPARE_OP_LESS_OR_EQUAL;
        descriptor.polygon_mode = VK_POLYGON_MODE_FILL;
        descriptor.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        descriptor.push_constant_size = sizeof(hmm_mat4);
        descriptor.set_layouts[0] = &data->skybox_set_layout;
        descriptor.set_layout_count = 1;
        descriptor.shaders.vs = &vs;
        descriptor.shaders.ps = &fs;
        descriptor.depth_biased_enable = 0;

        rhi_init_graphics_pipeline(&data->skybox_pipeline, &descriptor);
        
        rhi_free_shader(&vs);
        rhi_free_shader(&fs);
    }

    {
        data->deferred_set_layout.descriptors[0] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[1] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[2] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[3] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[4] = DESCRIPTOR_SAMPLER;
        data->deferred_set_layout.descriptors[5] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[6] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[7] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptors[8] = DESCRIPTOR_IMAGE;
        data->deferred_set_layout.descriptor_count = 9;
        rhi_init_descriptor_set_layout(&data->deferred_set_layout);

        rhi_init_descriptor_set(&data->deferred_set, &data->deferred_set_layout);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gPosition, 0);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gNormal, 1);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gAlbedo, 2);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->gMetallicRoughness, 3);
        rhi_descriptor_set_write_sampler(&data->deferred_set, &data->cubemap_sampler, 4);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->cubemap, 5);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->irradiance, 6);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->prefilter, 7);
        rhi_descriptor_set_write_image(&data->deferred_set, &data->brdf, 8);

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
        descriptor.reflect_input_layout = 0;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.color_attachments_formats[1] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.color_attachments_formats[2] = VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.color_attachments_formats[3] = VK_FORMAT_R8G8B8A8_UNORM;
        descriptor.color_attachment_count = 4;
        descriptor.depth_attachment_format = VK_FORMAT_D24_UNORM_S8_UINT;
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
        descriptor.depth_bounds_enable = 1;

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
        descriptor.reflect_input_layout = 1;
        descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
        descriptor.color_attachment_count = 1;
        descriptor.color_attachments_formats[0] = VK_FORMAT_R16G16B16A16_SFLOAT;
        descriptor.depth_attachment_format = VK_FORMAT_D24_UNORM_S8_UINT;
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
}

void geometry_pass_execute_gbuffer(RHI_CommandBuffer* cmd_buf, RenderGraphNode* node, RenderGraphExecute* execute, geometry_pass* data)
{
    f64 start = aurora_platform_get_time();

    RHI_RenderBegin begin;
    memset(&begin, 0, sizeof(RHI_RenderBegin));
    begin.r = 0.0f;
	begin.g = 0.0f;
	begin.b = 0.0f;
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

    rhi_cmd_start_render(cmd_buf, begin);

    rhi_cmd_img_transition_layout(cmd_buf, &data->gPosition, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &data->gNormal, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &data->gAlbedo, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &data->gMetallicRoughness, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[1], 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0);

    rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);
    rhi_cmd_set_pipeline(cmd_buf, &data->gbuffer_pipeline);
    rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &execute->camera_descriptor_set, 0);
    rhi_cmd_set_descriptor_heap(cmd_buf, &data->gbuffer_pipeline, &execute->image_heap, 1);
    rhi_cmd_set_descriptor_heap(cmd_buf, &data->gbuffer_pipeline, &execute->sampler_heap, 2);
    rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &data->params_set, 5);
    rhi_cmd_set_depth_bounds(cmd_buf, 0.0f, 0.999f);

    for (i32 i = 0; i < execute->model_count; i++)
    {
        Mesh* model = &execute->models[i];
        for (u32 i = 0; i < model->primitive_count; i++)
	    {
	    	rhi_cmd_set_push_constants(cmd_buf, &data->gbuffer_pipeline, &model->primitives[i].transform, sizeof(hmm_mat4));
            rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &model->materials[model->primitives[i].material_index].material_set, 3);
            rhi_cmd_set_descriptor_set(cmd_buf, &data->gbuffer_pipeline, &model->primitives[i].geometry_descriptor_set, 4);
	    	rhi_cmd_draw_meshlets(cmd_buf, model->primitives[i].meshlet_count);
	    }
    }

    rhi_cmd_img_transition_layout(cmd_buf, &data->gPosition, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &data->gNormal, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &data->gAlbedo, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    rhi_cmd_img_transition_layout(cmd_buf, &data->gMetallicRoughness, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);

    rhi_cmd_end_render(cmd_buf);

    f64 end = aurora_platform_get_time();

    //printf("Geometry Pass: GBuffer execution took %f ms\n", (end - start) * 1000);
}

void geometry_pass_execute_deferred(RHI_CommandBuffer* cmd_buf, RenderGraphNode* node, RenderGraphExecute* execute, geometry_pass* data)
{
    f64 start = aurora_platform_get_time();

    RHI_RenderBegin begin;
    memset(&begin, 0, sizeof(RHI_RenderBegin));
	begin.r = 0.0f;
	begin.g = 0.0f;
	begin.b = 0.0f;
	begin.a = 1.0f;
	begin.has_depth = 0;
	begin.width = execute->width;
	begin.height = execute->height;
	begin.images[0] = &node->outputs[0];
	begin.image_count = 1;

    rhi_cmd_img_transition_layout(cmd_buf, &node->outputs[0], VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
    hmm_vec4 temp = HMM_Vec4(execute->camera.pos.X, execute->camera.pos.Y, execute->camera.pos.Z, 1.0);

    for (u32 i = 0; i < 6; i++)
    {
        rhi_cmd_img_transition_layout(cmd_buf, &data->cubemap, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, i);
        rhi_cmd_img_transition_layout(cmd_buf, &data->irradiance, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, i);
        rhi_cmd_img_transition_layout(cmd_buf, &data->prefilter, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, i);
    }
    rhi_cmd_img_transition_layout(cmd_buf, &data->brdf, 0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);

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

    f64 end = aurora_platform_get_time();

    //printf("Geometry Pass: Deferred execution took %f ms\n", (end - start) * 1000);
}

void geometry_pass_execute_skybox(RHI_CommandBuffer* cmd_buf, RenderGraphNode* node, RenderGraphExecute* execute, geometry_pass* data)
{
    f64 start = aurora_platform_get_time();

    RHI_RenderBegin begin;
    memset(&begin, 0, sizeof(RHI_RenderBegin));
    begin.r = 0.0f;
	begin.g = 0.0f;
	begin.b = 0.0f;
	begin.a = 1.0f;
	begin.has_depth = 1;
	begin.width = execute->width;
	begin.height = execute->height;
	begin.images[0] = &node->outputs[0];
    begin.images[1] = &node->outputs[1];
	begin.image_count = 2;
    begin.read_depth = 1;
    begin.read_color = 1;

    rhi_cmd_start_render(cmd_buf, begin);
    rhi_cmd_set_viewport(cmd_buf, execute->width, execute->height);

    hmm_mat4 final_matrix = HMM_Mat4d(1.0f);

    hmm_mat4 view = execute->camera.view;
    view.Elements[3][0] = 0.0f;
    view.Elements[3][1] = 0.0f;
    view.Elements[3][2] = 0.0f;
    view.Elements[0][3] = 0.0f;
    view.Elements[1][3] = 0.0f;
    view.Elements[2][3] = 0.0f;
    view.Elements[3][3] = 1.0f;

    final_matrix = HMM_MultiplyMat4(final_matrix, execute->camera.projection);
    final_matrix = HMM_MultiplyMat4(final_matrix, view);

    rhi_cmd_set_pipeline(cmd_buf, &data->skybox_pipeline);
    rhi_cmd_set_push_constants(cmd_buf, &data->skybox_pipeline, &final_matrix, sizeof(hmm_mat4));
    rhi_cmd_set_descriptor_set(cmd_buf, &data->skybox_pipeline, &data->skybox_set, 0);
    rhi_cmd_set_depth_bounds(cmd_buf, 0.999f, 1.0f);
    rhi_cmd_draw(cmd_buf, 36);

    rhi_cmd_end_render(cmd_buf);

    f64 end = aurora_platform_get_time();

    //printf("Geometry Pass: Skybox execution took %f ms\n", (end - start) * 1000);
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

    geometry_pass_execute_gbuffer(cmd_buf, node, execute, data);
    geometry_pass_execute_deferred(cmd_buf, node, execute, data);
    geometry_pass_execute_skybox(cmd_buf, node, execute, data);
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
}

void geometry_pass_free(RenderGraphNode* node, RenderGraphExecute* execute)
{
    geometry_pass* data = node->private_data;

    rhi_free_descriptor_set(&data->brdf_set);
    rhi_free_pipeline(&data->brdf_pipeline);
    rhi_free_descriptor_set_layout(&data->brdf_set_layout);

    rhi_free_descriptor_set(&data->prefilter_set);
    rhi_free_pipeline(&data->prefilter_pipeline);
    rhi_free_descriptor_set_layout(&data->prefilter_set_layout);

    rhi_free_descriptor_set(&data->irradiance_set);
    rhi_free_pipeline(&data->irradiance_pipeline);
    rhi_free_descriptor_set_layout(&data->irradiance_set_layout);

    rhi_free_descriptor_set(&data->skybox_set);
    rhi_free_pipeline(&data->skybox_pipeline);
    rhi_free_descriptor_set_layout(&data->skybox_set_layout);

    rhi_free_descriptor_set(&data->cubemap_set);
    rhi_free_descriptor_set_layout(&data->cubemap_set_layout);
    rhi_free_pipeline(&data->cubemap_pipeline);

    rhi_free_image(&data->brdf);
    rhi_free_image(&data->prefilter);
    rhi_free_image(&data->irradiance);
    rhi_free_image(&data->cubemap);

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