#ifndef RHI_H_INCLUDED
#define RHI_H_INCLUDED

/*
Vulkan TODOs:
TODO: Pipelines
TODO: Buffers
TODO: Textures
TODO: Profiler
TODO: Descriptors

Renderer TODOs:
TODO: GLTF loading
TODO: Render Graph
TODO: (possibly) microui or cimgui?
TODO: PBR environment maps
TODO: Composite Pass
TODO: Forward+ Rendering
TODO: Mesh Shaders
TODO: Shadows
TODO: Global Illumination
*/

#define FRAMES_IN_FLIGHT 2
#define COMMAND_BUFFER_GRAPHICS 0
#define COMMAND_BUFFER_COMPUTE 1
#define COMMAND_BUFFER_UPLOAD 2

// It's not like I'm going to implement another graphics API for this project, so we'll let the vulkan stuff public for now kekw
#include "vulkan/vulkan.h"
#include "common.h"

typedef struct rhi_image rhi_image;
struct rhi_image
{
    VkImage image;
    VkImageView image_view;
    VkFormat format;
    VkExtent2D extent;
    VkImageLayout image_layout;
};

typedef struct rhi_command_buf rhi_command_buf;
struct rhi_command_buf
{
    VkCommandBuffer buf;
    u32 command_buffer_type;
};

typedef struct rhi_render_begin rhi_render_begin;
struct rhi_render_begin
{
    rhi_image* images[32];
    i32 image_count;

    i32 width;
    i32 height;
    b32 has_depth;

    f32 r, g, b, a;
};

void rhi_init();
void rhi_begin();
void rhi_end();
void rhi_present();
void rhi_shutdown();
void rhi_wait_idle();
void rhi_resize();

rhi_image* rhi_get_swapchain_image();
rhi_command_buf* rhi_get_swapchain_cmd_buf();

void rhi_init_cmd_buf(rhi_command_buf* buf, u32 command_buffer_type);
void rhi_init_upload_cmd_buf(rhi_command_buf* buf);
void rhi_submit_cmd_buf(rhi_command_buf* buf);
void rhi_submit_upload_cmd_buf(rhi_command_buf* buf);
void rhi_begin_cmd_buf(rhi_command_buf* buf);
void rhi_end_cmd_buf(rhi_command_buf* buf);
void rhi_cmd_set_viewport(rhi_command_buf* buf, u32 width, u32 height);
void rhi_cmd_start_render(rhi_command_buf* buf, rhi_render_begin info);
void rhi_cmd_end_render(rhi_command_buf* buf);
void rhi_cmd_img_transition_layout(rhi_command_buf* buf, rhi_image* img, u32 src_access, u32 dst_access, u32 src_layout, u32 dst_layout, u32 src_p_stage, u32 dst_p_stage, u32 layer);

#endif