#include "platform_layer.h"
#include "rhi.h"

#include <stdio.h>

void game_resize(u32 width, u32 height)
{
	rhi_wait_idle();
	rhi_resize();
}

global f32 vertices[] = {
	-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
	 0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
	 0.5f,  0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f,
	-0.5f,  0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f
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

	rhi_pipeline triangle_pipeline;
	rhi_buffer triangle_vertex_buffer;
	rhi_buffer triangle_index_buffer;

	{
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
	
		rhi_init_graphics_pipeline(&triangle_pipeline, &descriptor);
		rhi_allocate_buffer(&triangle_vertex_buffer, sizeof(vertices), BUFFER_VERTEX);
		rhi_upload_buffer(&triangle_vertex_buffer, vertices, sizeof(vertices));
		rhi_allocate_buffer(&triangle_index_buffer, sizeof(indices), BUFFER_INDEX);
		rhi_upload_buffer(&triangle_index_buffer, indices, sizeof(indices));

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
		rhi_cmd_set_graphics_pipeline(cmd_buf, &triangle_pipeline);
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
	rhi_free_buffer(&triangle_index_buffer);
	rhi_free_buffer(&triangle_vertex_buffer);

	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
