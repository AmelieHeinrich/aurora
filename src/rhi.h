#ifndef RHI_H_INCLUDED
#define RHI_H_INCLUDED

/*
Vulkan TODOs:
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
#define PIPELINE_GRAPHICS 3
#define PIPELINE_COMPUTE 4
#define BUFFER_VERTEX VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
#define BUFFER_INDEX VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
#define BUFFER_UNIFORM VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
#define IMAGE_RTV VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
#define IMAGE_DSV VK_IMAGE_USAGE_DEPTH_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
#define IMAGE_STORAGE VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_SAMPLED_BIT

// It's not like I'm going to implement another graphics API for this project, so we'll let the vulkan stuff public for now kekw
#include "vulkan/vulkan.h"
#include "vma.h"
#include "common.h"

typedef struct rhi_image rhi_image;
struct rhi_image
{
    VkImage image;
    VkImageView image_view;
    VkFormat format;
    VkExtent2D extent;
    VkImageLayout image_layout;

    VmaAllocation allocation;

    i32 width, height;
    u32 usage;
};

typedef struct rhi_command_buf rhi_command_buf;
struct rhi_command_buf
{
    VkCommandBuffer buf;
    u32 command_buffer_type;
};

typedef struct rhi_shader_module rhi_shader_module;
struct rhi_shader_module
{
    VkShaderModule shader_module;
    u32* byte_code;
    u32 byte_code_size;
};

typedef struct rhi_pipeline_descriptor rhi_pipeline_descriptor;
struct rhi_pipeline_descriptor
{
    // Don't have mesh shaders atm but I have it here for the future
    b32 use_mesh_shaders;
    struct {
        rhi_shader_module* vs;
        rhi_shader_module* ps;
        rhi_shader_module* ms;
        rhi_shader_module* ts;
    } shaders;

    VkPrimitiveTopology primitive_topology;
    VkPolygonMode polygon_mode;
    VkCullModeFlagBits cull_mode;
    VkFrontFace front_face;
    b32 ccw;
    VkCompareOp depth_op;

    i32 color_attachment_count;
    VkFormat depth_attachment_format;
    VkFormat color_attachments_formats[32];
};

typedef struct rhi_pipeline rhi_pipeline;
struct rhi_pipeline
{
    u32 pipeline_type;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
};  

typedef struct rhi_buffer rhi_buffer;
struct rhi_buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VkBufferUsageFlagBits usage;
    VmaMemoryUsage memory_usage;
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

// Pipeline/Shaders
void rhi_load_shader(rhi_shader_module* shader, const char* path);
void rhi_free_shader(rhi_shader_module* shader);
void rhi_init_graphics_pipeline(rhi_pipeline* pipeline, rhi_pipeline_descriptor* descriptor);
void rhi_free_pipeline(rhi_pipeline* pipeline);

// Buffer
void rhi_allocate_buffer(rhi_buffer* buffer, u32 size, u32 buffer_usage);
void rhi_free_buffer(rhi_buffer* buffer);
void rhi_upload_buffer(rhi_buffer* buffer, void* data, u32 size);

// Image
void rhi_allocate_image(rhi_image* image, i32 width, i32 height, VkFormat format, u32 usage);
void rhi_load_image(rhi_image* image, const char* path);
void rhi_free_image(rhi_image* image);
void rhi_resize_image(rhi_image* image, i32 width, i32 height);

// Cmd Buf
void rhi_init_cmd_buf(rhi_command_buf* buf, u32 command_buffer_type);
void rhi_init_upload_cmd_buf(rhi_command_buf* buf);
void rhi_submit_cmd_buf(rhi_command_buf* buf);
void rhi_submit_upload_cmd_buf(rhi_command_buf* buf);
void rhi_begin_cmd_buf(rhi_command_buf* buf);
void rhi_end_cmd_buf(rhi_command_buf* buf);
void rhi_cmd_set_viewport(rhi_command_buf* buf, u32 width, u32 height);
void rhi_cmd_set_graphics_pipeline(rhi_command_buf* buf, rhi_pipeline* pipeline);
void rhi_cmd_set_vertex_buffer(rhi_command_buf* buf, rhi_buffer* buffer);
void rhi_cmd_set_index_buffer(rhi_command_buf* buf, rhi_buffer* buffer);
void rhi_cmd_draw(rhi_command_buf* buf, u32 count);
void rhi_cmd_draw_indexed(rhi_command_buf* buf, u32 count);
void rhi_cmd_start_render(rhi_command_buf* buf, rhi_render_begin info);
void rhi_cmd_end_render(rhi_command_buf* buf);
void rhi_cmd_img_transition_layout(rhi_command_buf* buf, rhi_image* img, u32 src_access, u32 dst_access, u32 src_layout, u32 dst_layout, u32 src_p_stage, u32 dst_p_stage, u32 layer);

#endif