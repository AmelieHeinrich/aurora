#ifndef VK_UTILS_H_INCLUDED
#define VK_UTILS_H_INCLUDED

#include <vulkan/vulkan.h>
#include <core/common.h>

u32 vk_get_image_aspect(u32 format);
u32 vk_get_format_size(VkFormat format);
u32 vk_get_memory_usage(VkBufferUsageFlagBits flags);

#endif