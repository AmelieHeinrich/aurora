#include "platform_layer.h"
#include "rhi.h"
#include "camera.h"
#include "mesh.h"

#include <stdio.h>

global fps_camera camera;
global rhi_image depth_buffer;
global f64 last_frame;

typedef struct push_constants push_constants;
struct push_constants
{
	hmm_mat4 model_matrix;
	hmm_mat4 camera_matrix;
	hmm_vec3 camera_pos;
	f32 pad;
};	

void game_resize(u32 width, u32 height)
{
	rhi_wait_idle();
	rhi_resize();
	fps_camera_resize(&camera, width, height);
	rhi_resize_image(&depth_buffer, width, height);
}

int main()
{
	aurora_platform_layer_init();
	platform.width = 1280;
	platform.height = 720;
	platform.resize_event = game_resize;
	aurora_platform_open_window("Aurora Window");

	rhi_descriptor_heap image_heap;
	rhi_descriptor_heap sampler_heap;
	rhi_pipeline mesh_pipeline;
	rhi_sampler nearest_sampler;
	mesh sponza;
	push_constants matrices;

	rhi_init();
	rhi_init_descriptor_heap(&image_heap, DESCRIPTOR_HEAP_IMAGE, 1024);
	rhi_init_descriptor_heap(&sampler_heap, DESCRIPTOR_HEAP_SAMPLER, 16);
	mesh_loader_set_texture_heap(&image_heap);
	mesh_loader_init(2);
	fps_camera_init(&camera);

	{
		rhi_allocate_image(&depth_buffer, platform.width, platform.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

		nearest_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		nearest_sampler.filter = VK_FILTER_NEAREST;
		rhi_init_sampler(&nearest_sampler);
		rhi_push_descriptor_heap_sampler(&sampler_heap, &nearest_sampler, 0);

		matrices.model_matrix = HMM_Mat4d(1.0f);
		matrices.model_matrix = HMM_Scale(HMM_Vec3(0.00800000037997961, 0.00800000037997961, 0.00800000037997961));
		matrices.model_matrix = HMM_MultiplyMat4(matrices.model_matrix, HMM_Rotate(180.0f, HMM_Vec3(1.0f, 0.0f, 0.0f)));

		rhi_shader_module vertex_shader;
		rhi_shader_module pixel_shader;
		rhi_pipeline_descriptor descriptor;

		rhi_load_shader(&vertex_shader, "shaders/triangle_vert.vert.spv");
		rhi_load_shader(&pixel_shader, "shaders/triangle_frag.frag.spv");

		memset(&descriptor, 0, sizeof(descriptor));
		descriptor.color_attachment_count = 1;
		descriptor.color_attachments_formats[0] = VK_FORMAT_B8G8R8A8_UNORM;
		descriptor.depth_attachment_format = VK_FORMAT_D32_SFLOAT;
		descriptor.cull_mode = VK_CULL_MODE_BACK_BIT;
		descriptor.depth_op = VK_COMPARE_OP_LESS;
		descriptor.front_face = VK_FRONT_FACE_CLOCKWISE;
		descriptor.polygon_mode = VK_POLYGON_MODE_FILL;
		descriptor.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		descriptor.use_mesh_shaders = 0;
		descriptor.shaders.vs = &vertex_shader;
		descriptor.shaders.ps = &pixel_shader;
		descriptor.set_layout_count = 3;
		descriptor.set_layouts[0] = rhi_get_image_heap_set_layout();
		descriptor.set_layouts[1] = rhi_get_sampler_heap_set_layout();
		descriptor.set_layouts[2] = mesh_loader_get_descriptor_set_layout();
		descriptor.push_constant_size = sizeof(push_constants);

		rhi_init_graphics_pipeline(&mesh_pipeline, &descriptor);
		mesh_load(&sponza, "assets/Sponza.gltf");

		rhi_free_shader(&pixel_shader);
		rhi_free_shader(&vertex_shader);
	}

	while (!platform.quit)
	{
		f32 time = aurora_platform_get_time();
		f32 dt = time - last_frame;
		last_frame = time;

		rhi_begin();

		rhi_image* swap_chain_image = rhi_get_swapchain_image();
		rhi_command_buf* cmd_buf = rhi_get_swapchain_cmd_buf();

		rhi_render_begin begin = {0};
		begin.r = 0.1f;
		begin.g = 0.1f;
		begin.b = 0.1f;
		begin.a = 1.0f;
		begin.has_depth = 1;
		begin.width = platform.width;
		begin.height = platform.height;
		begin.images[0] = swap_chain_image;
		begin.images[1] = &depth_buffer;
		begin.image_count = 2;

		rhi_cmd_img_transition_layout(cmd_buf, swap_chain_image, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
		rhi_cmd_img_transition_layout(cmd_buf, &depth_buffer, 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, 0);

		rhi_cmd_start_render(cmd_buf, begin);
		rhi_cmd_set_viewport(cmd_buf, platform.width, platform.height);
		rhi_cmd_set_pipeline(cmd_buf, &mesh_pipeline);
		rhi_cmd_set_descriptor_heap(cmd_buf, &mesh_pipeline, &image_heap, 0);
		rhi_cmd_set_descriptor_heap(cmd_buf, &mesh_pipeline, &sampler_heap, 1);
		for (i32 i = 0; i < sponza.primitive_count; i++)
		{
			matrices.camera_matrix = camera.view_projection;
			matrices.camera_pos = camera.position;
			rhi_cmd_set_push_constants(cmd_buf, &mesh_pipeline, &matrices, sizeof(matrices));
			rhi_cmd_set_descriptor_set(cmd_buf, &mesh_pipeline, &sponza.materials[sponza.primitives[i].material_index].material_set, 2);
			rhi_cmd_set_vertex_buffer(cmd_buf, &sponza.primitives[i].vertex_buffer);
			rhi_cmd_set_index_buffer(cmd_buf, &sponza.primitives[i].index_buffer);
			rhi_cmd_draw_indexed(cmd_buf, sponza.primitives[i].index_count);
		}
		
		rhi_cmd_img_transition_layout(cmd_buf, swap_chain_image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
		rhi_cmd_end_render(cmd_buf);

		rhi_end();
		rhi_present();

		fps_camera_input(&camera, dt);
		fps_camera_update(&camera, dt);

		aurora_platform_update_window();
	}
	
	rhi_wait_idle();

	mesh_loader_free();
	rhi_free_pipeline(&mesh_pipeline);
	mesh_free(&sponza);
	rhi_free_sampler(&nearest_sampler);
	rhi_free_image(&depth_buffer);
	rhi_free_descriptor_heap(&image_heap);
	rhi_free_descriptor_heap(&sampler_heap);
	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
