#include "platform_layer.h"
#include "rhi.h"

#include <stdio.h>

void game_resize(u32 width, u32 height)
{
	rhi_wait_idle();
	rhi_resize();
}

int main()
{
	aurora_platform_layer_init();
	platform.width = 1280;
	platform.height = 720;
	platform.resize_event = game_resize;
	aurora_platform_open_window("Aurora Window");

	rhi_init();

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
		rhi_cmd_img_transition_layout(cmd_buf, swap_chain_image, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0);
		rhi_cmd_end_render(cmd_buf);

		rhi_end();
		rhi_present();

		aurora_platform_update_window();
	}

	rhi_shutdown();
	
	aurora_platform_free_window();
	aurora_platform_layer_free();
	return 0;
}
