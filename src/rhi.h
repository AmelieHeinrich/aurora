#ifndef RHI_H_INCLUDED
#define RHI_H_INCLUDED

/*
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
#define IMAGE_RTV VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
#define IMAGE_GBUFFER VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
#define IMAGE_DSV VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT
#define IMAGE_STORAGE VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_SAMPLED_BIT
#define DESCRIPTOR_HEAP_IMAGE 0
#define DESCRIPTOR_HEAP_SAMPLER 1
#define DESCRIPTOR_IMAGE VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
#define DESCRIPTOR_SAMPLER VK_DESCRIPTOR_TYPE_SAMPLER
#define DESCRIPTOR_BUFFER VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER

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

typedef struct rhi_sampler rhi_sampler;
struct rhi_sampler
{
    VkSamplerAddressMode address_mode;
    VkFilter filter;
    VkSampler sampler;
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

typedef struct rhi_descriptor_set_layout rhi_descriptor_set_layout;
struct rhi_descriptor_set_layout
{
    u32 descriptors[32];
    u32 descriptor_count;
    u32 binding;

    VkDescriptorSetLayout layout;
};

typedef struct rhi_descriptor_set rhi_descriptor_set;
struct rhi_descriptor_set
{
    i32 binding;
    VkDescriptorSet set;
};

typedef struct rhi_pipeline_descriptor rhi_pipeline_descriptor;
struct rhi_pipeline_descriptor
{
    // Don't have mesh shaders atm but I have it here for the future
    b32 use_mesh_shaders;
    struct {
        rhi_shader_module* vs;
        rhi_shader_module* ps;
        rhi_shader_module* cs;
        rhi_shader_module* ms;
        rhi_shader_module* ts;
    } shaders;

    VkPrimitiveTopology primitive_topology;
    VkPolygonMode polygon_mode;
    VkCullModeFlagBits cull_mode;
    VkFrontFace front_face;
    b32 ccw;
    VkCompareOp depth_op;
    b32 depth_biased_enable;

    i32 color_attachment_count;
    VkFormat depth_attachment_format;
    VkFormat color_attachments_formats[32];

    rhi_descriptor_set_layout* set_layouts[16];
    u32 set_layout_count;

    u32 push_constant_size;
};

typedef struct rhi_pipeline rhi_pipeline;
struct rhi_pipeline
{
    u32 pipeline_type;
    VkPipelineBindPoint bind_point;
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

typedef struct rhi_descriptor_heap rhi_descriptor_heap;
struct rhi_descriptor_heap
{
    u32 type;
    u32 size;
    u32 used;
    VkDescriptorSet set;
    b32* heap_handle;
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
rhi_descriptor_set_layout* rhi_get_image_heap_set_layout();
rhi_descriptor_set_layout* rhi_get_sampler_heap_set_layout();

// Descriptor set layout
void rhi_init_descriptor_set_layout(rhi_descriptor_set_layout* layout);
void rhi_free_descriptor_set_layout(rhi_descriptor_set_layout* layout);

// Descriptor set
void rhi_init_descriptor_set(rhi_descriptor_set* set, rhi_descriptor_set_layout* layout);
void rhi_free_descriptor_set(rhi_descriptor_set* set);
void rhi_descriptor_set_write_image(rhi_descriptor_set* set, rhi_image* image, i32 binding);
void rhi_descriptor_set_write_buffer(rhi_descriptor_set* set, rhi_buffer* buffer, i32 size, i32 binding);

// Samplers
void rhi_init_sampler(rhi_sampler* sampler);
void rhi_free_sampler(rhi_sampler* sampler);

// Pipeline/Shaders
void rhi_load_shader(rhi_shader_module* shader, const char* path);
void rhi_free_shader(rhi_shader_module* shader);
void rhi_init_graphics_pipeline(rhi_pipeline* pipeline, rhi_pipeline_descriptor* descriptor);
void rhi_init_compute_pipeline(rhi_pipeline* pipeline, rhi_pipeline_descriptor* descriptor);
void rhi_free_pipeline(rhi_pipeline* pipeline);

// Buffer
void rhi_allocate_buffer(rhi_buffer* buffer, u64 size, u32 buffer_usage);
void rhi_free_buffer(rhi_buffer* buffer);
void rhi_upload_buffer(rhi_buffer* buffer, void* data, u64 size);

// Image
void rhi_allocate_image(rhi_image* image, i32 width, i32 height, VkFormat format, u32 usage);
void rhi_allocate_cubemap(rhi_image* image, i32 width, i32 height, VkFormat format, u32 usage);
void rhi_load_image(rhi_image* image, const char* path);
void rhi_load_hdr_image(rhi_image* image, const char* path);
void rhi_free_image(rhi_image* image);
void rhi_resize_image(rhi_image* image, i32 width, i32 height);

// Descriptor heap
void rhi_init_descriptor_heap(rhi_descriptor_heap* heap, u32 type, u32 size);
i32 rhi_find_available_descriptor(rhi_descriptor_heap* heap);
void rhi_push_descriptor_heap_image(rhi_descriptor_heap* heap, rhi_image* image, i32 binding);
void rhi_push_descriptor_heap_sampler(rhi_descriptor_heap* heap, rhi_sampler* sampler, i32 binding);
void rhi_free_descriptor(rhi_descriptor_heap* heap, u32 descriptor);
void rhi_free_descriptor_heap(rhi_descriptor_heap* heap);

// Cmd Buf
void rhi_init_cmd_buf(rhi_command_buf* buf, u32 command_buffer_type);
void rhi_init_upload_cmd_buf(rhi_command_buf* buf);
void rhi_submit_cmd_buf(rhi_command_buf* buf);
void rhi_submit_upload_cmd_buf(rhi_command_buf* buf);
void rhi_begin_cmd_buf(rhi_command_buf* buf);
void rhi_end_cmd_buf(rhi_command_buf* buf);
void rhi_cmd_set_viewport(rhi_command_buf* buf, u32 width, u32 height);
void rhi_cmd_set_pipeline(rhi_command_buf* buf, rhi_pipeline* pipeline);
void rhi_cmd_set_vertex_buffer(rhi_command_buf* buf, rhi_buffer* buffer);
void rhi_cmd_set_index_buffer(rhi_command_buf* buf, rhi_buffer* buffer);
void rhi_cmd_set_descriptor_heap(rhi_command_buf* buf, rhi_pipeline* pipeline, rhi_descriptor_heap* heap, i32 binding);
void rhi_cmd_set_descriptor_set(rhi_command_buf* buf, rhi_pipeline* pipeline, rhi_descriptor_set* set, i32 binding);
void rhi_cmd_set_push_constants(rhi_command_buf* buf, rhi_pipeline* pipeline, void* data, u32 size);
void rhi_cmd_draw(rhi_command_buf* buf, u32 count);
void rhi_cmd_draw_indexed(rhi_command_buf* buf, u32 count);
void rhi_cmd_draw_meshlets(rhi_command_buf* buf, u32 count);
void rhi_cmd_dispatch(rhi_command_buf* buf, u32 x, u32 y, u32 z);
void rhi_cmd_start_render(rhi_command_buf* buf, rhi_render_begin info);
void rhi_cmd_end_render(rhi_command_buf* buf);
void rhi_cmd_img_transition_layout(rhi_command_buf* buf, rhi_image* img, u32 src_access, u32 dst_access, u32 src_layout, u32 dst_layout, u32 src_p_stage, u32 dst_p_stage, u32 layer);
void rhi_cmd_img_blit(rhi_command_buf* buf, rhi_image* src, rhi_image* dst, u32 srcl, u32 dstl);

#endif