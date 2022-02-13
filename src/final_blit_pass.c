#include "final_blit_pass.h"

void final_blit_pass_init(render_graph_node* node, render_graph_execute* execute)
{

}

void final_blit_pass_free(render_graph_node* node, render_graph_execute* execute)
{

}

void final_blit_pass_resize(render_graph_node* node, render_graph_execute* execute)
{

}

void final_blit_pass_update(render_graph_node* node, render_graph_execute* execute)
{
    rhi_command_buf* cmd_buf = rhi_get_swapchain_cmd_buf();

    rhi_cmd_img_transition_layout(cmd_buf, rhi_get_swapchain_image(), 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0);
    rhi_cmd_img_blit(cmd_buf, get_render_graph_node_input_image(&node->inputs[0]), rhi_get_swapchain_image(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    rhi_cmd_img_transition_layout(cmd_buf, rhi_get_swapchain_image(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0);
}

render_graph_node* create_final_blit_pass()
{
    render_graph_node* node = malloc(sizeof(render_graph_node));

    node->init = final_blit_pass_init;
    node->free = final_blit_pass_free;
    node->resize = final_blit_pass_resize;
    node->update = final_blit_pass_update;
    node->input_count = 0;
	memset(node->inputs, 0, sizeof(node->inputs));

    return node;
}