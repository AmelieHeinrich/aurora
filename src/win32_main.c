#include "platform_layer.h"
#include "rhi.h"

#include <stdio.h>

typedef struct bindless_material bindless_material;
struct bindless_material
{
	u32 texture_index;
	u32 sampler_index;
};	

void game_resize(u32 width, u32 height)
{
	rhi_wait_idle();
	rhi_resize();
}

global f32 vertices[] = {
	-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
	 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
	 0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
	-0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f
};

global u32 indices[] = {
	0, 1, 2,
	2, 3, 0
};

int main()
{
	aurora_platform_layer_init();
	platform.width = 1280;
	platform.height = 720;
	platform.resize_event = game_resize;
	aurora_platform_open_window("Aurora Window");

	rhi_init();

	rhi_descriptor_heap image_heap;
	rhi_descriptor_heap sampler_heap;
	rhi_init_descriptor_heap(&image_heap, DESCRIPTOR_HEAP_IMAGE, 64);
	rhi_init_descriptor_heap(&sampler_heap, DESCRIPTOR_HEAP_SAMPLER, 8);

	rhi_pipeline triangle_pipeline;
	rhi_descriptor_set_layout material_set_layout;
	rhi_descriptor_set material_set;
	rhi_image triangle_texture;
	rhi_sampler triangle_sampler;
	rhi_buffer triangle_vertex_buffer;
	rhi_buffer triangle_index_buffer;
	rhi_buffer triangle_material;
	bindless_material triangle_bindless;

	{
		material_set_layout.descriptors[0] = DESCRIPTOR_BUFFER;
		material_set_layout.descriptor_count = 1;
		material_set_layout.binding = 2;
		rhi_init_descriptor_set_layout(&material_set_layout);

		triangle_sampler.address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		triangle_sampler.filter = VK_FILTER_NEAREST;
		rhi_init_sampler(&triangle_sampler);
		rhi_load_image(&triangle_texture, "assets/texture.jpg");
		triangle_bindless.texture_index = rhi_find_available_descriptor(&image_heap);
		triangle_bindless.sampler_index = rhi_find_available_descriptor(&sampler_heap);

		rhi_push_descriptor_heap_image(&image_heap, &triangle_texture, triangle_bindless.texture_index);
		rhi_push_descriptor_heap_sampler(&sampler_heap, &triangle_sampler, triangle_bindless.sampler_index);

		rhi_shader_module vertex_shader;
		rhi_shader_module pixel_shader;
		rhi_load_shader(&vertex_shader, "shaders/triangle_vert.vert.spv");
		rhi_load_shader(&pixel_shader, "shaders/triangle_frag.frag.spv");

		rhi_pipeline_descriptor descriptor;
		memset(&descriptor, 0, sizeof(descriptor));
		descriptor.color_attachment_count = 1;
		descriptor.color_attachments_formats[0] = VK_FORMAT_B8G8R8A8_UNORM;
		descriptor.cull_mode = VK_CULL_MODE_NONE;
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
		descriptor.set_layouts[2] = &material_set_layout;
	
		rhi_init_graphics_pipeline(&triangle_pipeline, &descriptor);
		rhi_allocate_buffer(&triangle_vertex_buffer, sizeof(vertices), BUFFER_VERTEX);
		rhi_upload_buffer(&triangle_vertex_buffer, vertices, sizeof(vertices));
		rhi_allocate_buffer(&triangle_index_buffer, sizeof(indices), BUFFER_INDEX);
		rhi_upload_buffer(&triangle_index_buffer, indices, sizeof(indices));
		rhi_allocate_buffer(&triangle_material, sizeof(bindless_material), BUFFER_UNIFORM);
		rhi_upload_buffer(&triangle_material, &triangle_bindless, sizeof(bindless_material));

		rhi_init_descriptor_set(&material_set, &material_set_layout);
		rhi_descriptor_set_write_buffer(&material_set, &triangle_material, sizeof(bindless_material), 0);

		rhi_free_shader(&pixel_shader);
		rhi_free_shader(&vertex_shader);
	}

	while (!platform.quit)
	{
		rhi_begin();

		rhi_image* swap_chain_image = rhi_get_swapchain_image();
		rhi_command_buf* cmd_buf = rhi_get_swapchain_cmd_buf();

		rhi_render_begin begin = {0};
		begin.r = 0.0f;
		begin.g = 0.0f;
		begin.b = 0.0f;
		begin.a = 1.0f;
		begin.has_depth = 0;
		begin.width = platform.width;
		begin.height = platform.height;
		begin.images[0] = swap_chain_image;
		begin.image_count = 1;

		rhi_cmd_start_render(cmd_buf, begin);
		rhi_cmd_img_transition_layout(cmd_buf, swap_chain_image, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0);
		
		rhi_cmd_set_viewport(cmd_buf, platform.width, platform.height);
		rhi_cmd_set_pipeline(cmd_buf, &triangle_pipeline);
		rhi_cmd_set_descriptor_heap(cmd_buf, &triangle_pipeline, &image_heap, 0);
		rhi_cmd_set_descriptor_heap(cmd_buf, &triangle_pipeline, &sampler_heap, 1);
		rhi_cmd_set_descriptor_set(cmd_buf, &triangle_pipeline, &material_set, 2);
		rhi_cmd_set_vertex_buffer(cmd_buf, &triangle_vertex_buffer);
		rhi_cmd_set_index_buffer(cmd_buf, &triangle_index_buffer);
		rhi_cmd_draw_indexed(cmd_buf, 6);
		
		rhi_cmd_img_transition_layout(cmd_buf, swap_chain_image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
		rhi_cmd_end_render(cmd_buf);

		rhi_end();
		rhi_present();

		aurora_platform_update_window();
	}
	
	rhi_wait_idle();

	rhi_free_pipeline(&triangle_pipeline);
	rhi_free_descriptor_set(&material_set);
	rhi_free_buffer(&triangle_material);
	rhi_free_buffer(&triangle_index_buffer);
	rhi_free_buffer(&triangle_vertex_buffer);
	rhi_free_image(&triangle_texture);
	rhi_free_sampler(&triangle_sampler);
	rhi_free_descriptor_set_layout(&material_set_layout);

	rhi_free_descriptor_heap(&image_heap);
	rhi_free_descriptor_heap(&sampler_heap);

	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
