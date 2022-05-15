#ifndef RHI_H_INCLUDED
#define RHI_H_INCLUDED

/*
Renderer TODOs:
TODO: SSAO
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
#define IMAGE_STORAGE VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
#define DESCRIPTOR_HEAP_IMAGE 0
#define DESCRIPTOR_HEAP_SAMPLER 1
#define DESCRIPTOR_IMAGE VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
#define DESCRIPTOR_SAMPLED_IMAGE VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
#define DESCRIPTOR_SAMPLER VK_DESCRIPTOR_TYPE_SAMPLER
#define DESCRIPTOR_BUFFER VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
#define DESCRIPTOR_STORAGE_IMAGE VK_DESCRIPTOR_TYPE_STORAGE_IMAGE

// It's not like I'm going to implement another graphics API for this project, so we'll let the vulkan stuff public for now kekw
#include <vulkan/vulkan.h>
#include <vma.h>

#include <core/common.h>

typedef struct RHI_Image RHI_Image;
struct RHI_Image
{
    VkImage image;
    VkImageView image_view;
    VkFormat format;
    VkExtent2D extent;
    VkImageLayout image_layout;

    VmaAllocation allocation;

    i32 width, height;
    u32 usage;
    u32 mip_levels;
};

typedef struct RHI_Sampler RHI_Sampler;
struct RHI_Sampler
{
    VkSamplerAddressMode address_mode;
    VkFilter filter;
    VkSampler sampler;
};

typedef struct RHI_CommandBuffer RHI_CommandBuffer;
struct RHI_CommandBuffer
{
    VkCommandBuffer buf;
    u32 command_buffer_type;
};

typedef struct RHI_ShaderModule RHI_ShaderModule;
struct RHI_ShaderModule
{
    VkShaderModule shader_module;
    u32* byte_code;
    u32 byte_code_size;
};

typedef struct RHI_DescriptorSetLayout RHI_DescriptorSetLayout;
struct RHI_DescriptorSetLayout
{
    u32 descriptors[32];
    u32 descriptor_count;

    VkDescriptorSetLayout layout;
};

typedef struct RHI_DescriptorSet RHI_DescriptorSet;
struct RHI_DescriptorSet
{
    i32 binding;
    VkDescriptorSet set;
};

typedef struct RHI_PipelineDescriptor RHI_PipelineDescriptor;
struct RHI_PipelineDescriptor
{
    // Don't have mesh shaders atm but I have it here for the future
    b32 use_mesh_shaders;
    struct {
        RHI_ShaderModule* vs;
        RHI_ShaderModule* ps;
        RHI_ShaderModule* cs;
        RHI_ShaderModule* ms;
        RHI_ShaderModule* ts;
    } shaders;

    VkPrimitiveTopology primitive_topology;
    VkPolygonMode polygon_mode;
    VkCullModeFlagBits cull_mode;
    VkFrontFace front_face;
    b32 ccw;
    VkCompareOp depth_op;
    b32 depth_biased_enable;
    b32 depth_bounds_enable;

    b32 reflect_input_layout;

    i32 color_attachment_count;
    VkFormat depth_attachment_format;
    VkFormat color_attachments_formats[32];

    RHI_DescriptorSetLayout* set_layouts[16];
    u32 set_layout_count;

    u32 push_constant_size;
};

typedef struct RHI_Pipeline RHI_Pipeline;
struct RHI_Pipeline
{
    u32 pipeline_type;
    VkPipelineBindPoint bind_point;
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
};  

typedef struct RHI_Buffer RHI_Buffer;
struct RHI_Buffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VkBufferUsageFlagBits usage;
    VmaMemoryUsage memory_usage;
};  

typedef struct RHI_DescriptorHeap RHI_DescriptorHeap;
struct RHI_DescriptorHeap
{
    u32 type;
    u32 size;
    u32 used;
    VkDescriptorSet set;
    b32* heap_handle;
};

typedef struct RHI_RenderBegin RHI_RenderBegin;
struct RHI_RenderBegin
{
    RHI_Image* images[32];
    i32 image_count;

    i32 width;
    i32 height;
    b32 has_depth;
    b32 read_depth;
    b32 read_color;

    f32 r, g, b, a;
};

void rhi_init();
void rhi_begin();
void rhi_end();
void rhi_present();
void rhi_shutdown();
void rhi_wait_idle();
void rhi_resize();

RHI_Image* rhi_get_swapchain_image();
RHI_CommandBuffer* rhi_get_swapchain_cmd_buf();
RHI_DescriptorSetLayout* rhi_get_image_heap_set_layout();
RHI_DescriptorSetLayout* rhi_get_sampler_heap_set_layout();

// Descriptor set layout
void rhi_init_descriptor_set_layout(RHI_DescriptorSetLayout* layout);
void rhi_free_descriptor_set_layout(RHI_DescriptorSetLayout* layout);

// Descriptor set
void rhi_init_descriptor_set(RHI_DescriptorSet* set, RHI_DescriptorSetLayout* layout);
void rhi_free_descriptor_set(RHI_DescriptorSet* set);
void rhi_descriptor_set_write_sampler(RHI_DescriptorSet* set, RHI_Sampler* sampler, i32 binding);
void rhi_descriptor_set_write_image(RHI_DescriptorSet* set, RHI_Image* image, i32 binding);
void rhi_descriptor_set_write_image_sampler(RHI_DescriptorSet* set, RHI_Image* image, RHI_Sampler* sampler, i32 binding);
void rhi_descriptor_set_write_buffer(RHI_DescriptorSet* set, RHI_Buffer* buffer, i32 size, i32 binding);
void rhi_descriptor_set_write_storage_image(RHI_DescriptorSet* set, RHI_Image* image, RHI_Sampler* sampler, i32 binding);
void rhi_descriptor_set_write_storage_buffer(RHI_DescriptorSet* set, RHI_Buffer* buffer, i32 size, i32 binding);

// Samplers
void rhi_init_sampler(RHI_Sampler* sampler, u32 mips);
void rhi_free_sampler(RHI_Sampler* sampler);

// Pipeline/Shaders
void rhi_load_shader(RHI_ShaderModule* shader, const char* path);
void rhi_free_shader(RHI_ShaderModule* shader);
void rhi_init_graphics_pipeline(RHI_Pipeline* pipeline, RHI_PipelineDescriptor* descriptor);
void rhi_init_compute_pipeline(RHI_Pipeline* pipeline, RHI_PipelineDescriptor* descriptor);
void rhi_free_pipeline(RHI_Pipeline* pipeline);

// Buffer
void rhi_allocate_buffer(RHI_Buffer* buffer, u64 size, u32 buffer_usage);
void rhi_free_buffer(RHI_Buffer* buffer);
void rhi_upload_buffer(RHI_Buffer* buffer, void* data, u64 size);

// Image
void rhi_allocate_image(RHI_Image* image, i32 width, i32 height, VkFormat format, u32 usage);
void rhi_allocate_cubemap(RHI_Image* image, i32 width, i32 height, VkFormat format, u32 usage);
void rhi_load_image(RHI_Image* image, const char* path);
void rhi_load_hdr_image(RHI_Image* image, const char* path);
void rhi_free_image(RHI_Image* image);
void rhi_resize_image(RHI_Image* image, i32 width, i32 height);

// Descriptor heap
void rhi_init_descriptor_heap(RHI_DescriptorHeap* heap, u32 type, u32 size);
i32 rhi_find_available_descriptor(RHI_DescriptorHeap* heap);
void rhi_push_descriptor_heap_image(RHI_DescriptorHeap* heap, RHI_Image* image, i32 binding);
void rhi_push_descriptor_heap_sampler(RHI_DescriptorHeap* heap, RHI_Sampler* sampler, i32 binding);
void rhi_free_descriptor(RHI_DescriptorHeap* heap, u32 descriptor);
void rhi_free_descriptor_heap(RHI_DescriptorHeap* heap);

// Cmd Buf
void rhi_init_cmd_buf(RHI_CommandBuffer* buf, u32 command_buffer_type);
void rhi_free_cmd_buf(RHI_CommandBuffer* buf);
void rhi_init_upload_cmd_buf(RHI_CommandBuffer* buf);
void rhi_submit_cmd_buf(RHI_CommandBuffer* buf);
void rhi_submit_upload_cmd_buf(RHI_CommandBuffer* buf);
void rhi_begin_cmd_buf(RHI_CommandBuffer* buf);
void rhi_end_cmd_buf(RHI_CommandBuffer* buf);
void rhi_cmd_set_viewport(RHI_CommandBuffer* buf, u32 width, u32 height);
void rhi_cmd_set_pipeline(RHI_CommandBuffer* buf, RHI_Pipeline* pipeline);
void rhi_cmd_set_vertex_buffer(RHI_CommandBuffer* buf, RHI_Buffer* buffer);
void rhi_cmd_set_index_buffer(RHI_CommandBuffer* buf, RHI_Buffer* buffer);
void rhi_cmd_set_descriptor_heap(RHI_CommandBuffer* buf, RHI_Pipeline* pipeline, RHI_DescriptorHeap* heap, i32 binding);
void rhi_cmd_set_descriptor_set(RHI_CommandBuffer* buf, RHI_Pipeline* pipeline, RHI_DescriptorSet* set, i32 binding);
void rhi_cmd_set_push_constants(RHI_CommandBuffer* buf, RHI_Pipeline* pipeline, void* data, u32 size);
void rhi_cmd_set_depth_bounds(RHI_CommandBuffer* buf, f32 min, f32 max);
void rhi_cmd_draw(RHI_CommandBuffer* buf, u32 count);
void rhi_cmd_draw_indexed(RHI_CommandBuffer* buf, u32 count);
void rhi_cmd_draw_meshlets(RHI_CommandBuffer* buf, u32 count);
void rhi_cmd_dispatch(RHI_CommandBuffer* buf, u32 x, u32 y, u32 z);
void rhi_cmd_start_render(RHI_CommandBuffer* buf, RHI_RenderBegin info);
void rhi_cmd_end_render(RHI_CommandBuffer* buf);
void rhi_cmd_img_transition_layout(RHI_CommandBuffer* buf, RHI_Image* img, u32 src_access, u32 dst_access, u32 src_layout, u32 dst_layout, u32 src_p_stage, u32 dst_p_stage, u32 layer);
void rhi_cmd_img_blit(RHI_CommandBuffer* buf, RHI_Image* src, RHI_Image* dst, u32 srcl, u32 dstl);

#endif