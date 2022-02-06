#include "vk_utils.h"

#include <vulkan/vulkan.h>

u32 vk_get_image_aspect(u32 format)
{
    if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT)
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    
    return VK_IMAGE_ASPECT_COLOR_BIT;
}  