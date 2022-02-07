#include "rhi.h"
#include "platform_layer.h"
#include "vk_utils.h"
#include "vma.h"

#include <assert.h>
#include <volk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define vk_check(result) assert(result == VK_SUCCESS)
#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

typedef struct vk_state vk_state;
struct vk_state
{
    VkInstance instance;
    char* layers[64];
    char* extensions[64];
    i32 layer_count;
    i32 extension_count;

    VkSurfaceKHR surface;

    VkPhysicalDevice physical_device;
    u32 graphics_family;
    u32 compute_family;
    VkPhysicalDeviceProperties2 physical_device_properties_2;
    VkPhysicalDeviceFeatures2 physical_device_features;
    VkPhysicalDeviceMeshShaderPropertiesNV mesh_shader_properties;

    VkDevice device;
    VkQueue graphics_queue;
    VkQueue compute_queue;
    char* device_extensions[64];
    i32 device_extension_count;
    VkCommandPool graphics_pool;
    VkCommandPool compute_pool;
    VkFence compute_fence;
    VkCommandPool upload_pool;
    VkFence upload_fence;

    VkSwapchainKHR swap_chain;
    VkExtent2D swap_chain_extent;
    VkFormat swap_chain_format;
    VkImage* swap_chain_images;
    VkImageView swap_chain_image_views[FRAMES_IN_FLIGHT];
    VkFence swap_chain_fences[FRAMES_IN_FLIGHT];
    VkSemaphore image_available_semaphore;
    VkSemaphore image_rendered_semaphore;
    rhi_image rhi_swap_chain[FRAMES_IN_FLIGHT];
    i32 image_index;

    rhi_command_buf swap_chain_cmd_bufs[FRAMES_IN_FLIGHT];

    VmaAllocator allocator;
};

vk_state state;

b32 check_layers(u32 check_count, char **check_names, u32 layer_count, VkLayerProperties *layers) {
    for (u32 i = 0; i < check_count; i++) {
         b32 found = 0;
        for (u32 j = 0; j < layer_count; j++) {
            printf("Instance Layer: %s\n", layers[j].layerName);
            if (strcmp(check_names[i], layers[j].layerName) == 0) {
                found = 1;
                break;
            }
        }
    }
    return 1;
}

void rhi_make_instance()
{
    u32 instance_extension_count = 0;
    u32 instance_layer_count = 0;
    u32 validation_layer_count = 0;
    char** instance_validation_layers = NULL;

    char* instance_validation_layers_alt1[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    b32 validation_found;
    VkResult result = vkEnumerateInstanceLayerProperties(&instance_layer_count, NULL);
    vk_check(result);

    instance_validation_layers = instance_validation_layers_alt1;

    if (instance_layer_count > 0) {
        VkLayerProperties* instance_layers = malloc(sizeof (VkLayerProperties) * instance_layer_count);
        result = vkEnumerateInstanceLayerProperties(&instance_layer_count, instance_layers);
        vk_check(result);

        validation_found = check_layers(ARRAY_SIZE(instance_validation_layers_alt1), instance_validation_layers, instance_layer_count, instance_layers);

        if (validation_found) {
            state.layer_count = 1;
            state.layers[0] = "VK_LAYER_KHRONOS_validation";
            validation_layer_count = 1;
        }

        free(instance_layers);
    }

    result = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, NULL);
    vk_check(result);

    if (instance_extension_count > 0) {
        VkExtensionProperties *instance_extensions = malloc(sizeof(VkExtensionProperties) * instance_extension_count);
        if (instance_extensions)
        {
            result = vkEnumerateInstanceExtensionProperties(NULL, &instance_extension_count, instance_extensions);
            vk_check(result);

            for (u32 i = 0; i < instance_extension_count; i++) {
                printf("Instance Extension: %s\n", instance_extensions[i].extensionName);

                if (!strcmp(VK_KHR_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                    state.extensions[state.extension_count++] = VK_KHR_SURFACE_EXTENSION_NAME;
                }

                if (!strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                    state.extensions[state.extension_count++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                }

                if (!strcmp(VK_KHR_WIN32_SURFACE_EXTENSION_NAME, instance_extensions[i].extensionName)) {
                    state.extensions[state.extension_count++] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
                }

                assert(state.extension_count < 64);
            }

            free(instance_extensions);
        }
    }

    VkApplicationInfo app_info = {0};
    app_info.pNext = NULL;
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "AuroraApplication";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Aurora";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info = {0};
    create_info.pNext = NULL;
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = state.extension_count;
    create_info.ppEnabledExtensionNames = (const char *const *)state.extensions;
    create_info.enabledLayerCount = state.layer_count;
    create_info.ppEnabledLayerNames = (const char* const*)state.layers;

    result = vkCreateInstance(&create_info, NULL, &state.instance);
    vk_check(result);

    volkLoadInstance(state.instance);
}

void rhi_make_physical_device()
{
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(state.instance, &device_count, NULL);
    assert(device_count != 0);

    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    if (devices)
    {
        vkEnumeratePhysicalDevices(state.instance, &device_count, devices);
        state.physical_device = devices[0];
        free(devices);
    }

    state.physical_device_properties_2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    state.mesh_shader_properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV;

    state.physical_device_properties_2.pNext = &state.mesh_shader_properties;
    vkGetPhysicalDeviceProperties2(state.physical_device, &state.physical_device_properties_2);

    state.physical_device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    vkGetPhysicalDeviceFeatures2(state.physical_device, &state.physical_device_features);

    // Find queue families
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(state.physical_device, &queue_family_count, NULL);

    VkQueueFamilyProperties* queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    if (queue_families)
    {
        vkGetPhysicalDeviceQueueFamilyProperties(state.physical_device, &queue_family_count, queue_families);

        for (u32 i = 0; i < queue_family_count; i++)
        {
            if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                state.graphics_family = i;
            }

            if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
            {
                state.compute_family = i;
            }
        }

        free(queue_families);
    }
}

void rhi_make_device()
{
    f32 queuePriority = 1.0f;

    VkDeviceQueueCreateInfo graphics_queue_create_info = { 0 };
    graphics_queue_create_info.flags = 0;
    graphics_queue_create_info.pNext = NULL;
    graphics_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    graphics_queue_create_info.queueFamilyIndex = state.graphics_family;
    graphics_queue_create_info.queueCount = 1;
    graphics_queue_create_info.pQueuePriorities = &queuePriority;

    VkDeviceQueueCreateInfo compute_queue_create_info = { 0 };
    compute_queue_create_info.flags = 0;
    compute_queue_create_info.pNext = NULL;
    compute_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    compute_queue_create_info.queueFamilyIndex = state.compute_family;
    compute_queue_create_info.queueCount = 1;
    compute_queue_create_info.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures features = {0};
    features.samplerAnisotropy = 1;
    features.fillModeNonSolid = 1;
    features.geometryShader = 1;
    features.pipelineStatisticsQuery = 1;

    state.physical_device_features.features = features;

    VkPhysicalDevice16BitStorageFeatures features16 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES };
    features16.storageBuffer16BitAccess = true;

    VkPhysicalDevice8BitStorageFeaturesKHR features8 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES_KHR };
    features8.storageBuffer8BitAccess = true;
    features8.uniformAndStorageBuffer8BitAccess = true;
    features8.pNext = &features16;

    VkPhysicalDeviceMeshShaderFeaturesNV mesh_shader_features = { 0 };
    mesh_shader_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
    mesh_shader_features.taskShader = VK_TRUE;
    mesh_shader_features.meshShader = VK_TRUE;
    mesh_shader_features.pNext = &features8;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_features = { 0 };
    dynamic_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_features.dynamicRendering = 1;
    dynamic_features.pNext = &mesh_shader_features;

    state.physical_device_features.pNext = &dynamic_features;

    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(state.physical_device, NULL, &extension_count, NULL);
    VkExtensionProperties* properties = malloc(sizeof(VkExtensionProperties) * extension_count);
    if (properties)
    {
        vkEnumerateDeviceExtensionProperties(state.physical_device, NULL, &extension_count, properties);

        for (u32 i = 0; i < extension_count; i++)
        {
            printf("Device Extension: %s\n", properties[i].extensionName);

            if (!strcmp(VK_KHR_SWAPCHAIN_EXTENSION_NAME, properties[i].extensionName)) {
                state.device_extensions[state.device_extension_count++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
            }

            if (!strcmp(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, properties[i].extensionName)) {
                state.device_extensions[state.device_extension_count++] = VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
            }

            if (!strcmp(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME, properties[i].extensionName)) {
                state.device_extensions[state.device_extension_count++] = VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME;
            }

            if (!strcmp(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME, properties[i].extensionName)) {
                state.device_extensions[state.device_extension_count++] = VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
            }

            if (!strcmp(VK_NV_MESH_SHADER_EXTENSION_NAME, properties[i].extensionName)) {
                state.device_extensions[state.device_extension_count++] = VK_NV_MESH_SHADER_EXTENSION_NAME;
            }
        }

        free(properties);
    }

    VkDeviceQueueCreateInfo queue_create_infos[2] = {graphics_queue_create_info, compute_queue_create_info};
    i32 queue_create_info_count = 2;

    VkDeviceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = 2;
    create_info.pQueueCreateInfos = queue_create_infos;
    create_info.enabledExtensionCount = state.device_extension_count;
    create_info.ppEnabledExtensionNames = (const char* const*)state.device_extensions;
    create_info.pNext = &state.physical_device_features;
    create_info.enabledLayerCount = state.layer_count;
    create_info.ppEnabledLayerNames = (const char* const*)state.layers;

    VkResult result = vkCreateDevice(state.physical_device, &create_info, NULL, &state.device);
    vk_check(result);

    volkLoadDevice(state.device);
    vkGetDeviceQueue(state.device, state.graphics_family, 0, &state.graphics_queue);
    vkGetDeviceQueue(state.device, state.compute_family, 0, &state.compute_queue);
}

void rhi_make_swapchain()
{
    state.swap_chain_extent.width = platform.width;
    state.swap_chain_extent.height = platform.height;
    u32 queue_family_indices[] = { state.graphics_family };

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.physical_device, state.surface, &capabilities);

    u32 format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.physical_device, state.surface, &format_count, NULL);
    VkSurfaceFormatKHR* formats = malloc(sizeof(VkSurfaceFormatKHR) * format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.physical_device, state.surface, &format_count, formats);

    VkSwapchainCreateInfoKHR create_info = { 0 };
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = state.surface;
    create_info.minImageCount = FRAMES_IN_FLIGHT;
    create_info.imageExtent = state.swap_chain_extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 1;
    create_info.pQueueFamilyIndices = queue_family_indices;
    create_info.preTransform = capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // TODO(milo): setting to enable vsync?
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;
    create_info.imageFormat = formats[0].format;
    create_info.imageColorSpace = formats[0].colorSpace;
    free(formats);

    state.swap_chain_format = create_info.imageFormat;

    VkResult result = vkCreateSwapchainKHR(state.device, &create_info, NULL, &state.swap_chain);
    vk_check(result);

    u32 image_count = 0;
    vkGetSwapchainImagesKHR(state.device, state.swap_chain, &image_count, NULL);
    state.swap_chain_images = malloc(sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(state.device, state.swap_chain, &image_count, state.swap_chain_images);

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        VkImageViewCreateInfo iv_create_info = { 0 };
        iv_create_info.flags = 0;
        iv_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iv_create_info.image = state.swap_chain_images[i];
        iv_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iv_create_info.format = state.swap_chain_format;
        iv_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        iv_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iv_create_info.subresourceRange.baseMipLevel = 0;
        iv_create_info.subresourceRange.levelCount = 1;
        iv_create_info.subresourceRange.baseArrayLayer = 0;
        iv_create_info.subresourceRange.layerCount = 1;

        result = vkCreateImageView(state.device, &iv_create_info, NULL, &state.swap_chain_image_views[i]);
        vk_check(result);

        state.rhi_swap_chain[i].extent = state.swap_chain_extent;
        state.rhi_swap_chain[i].format = state.swap_chain_format;
        state.rhi_swap_chain[i].image = state.swap_chain_images[i];
        state.rhi_swap_chain[i].image_view = state.swap_chain_image_views[i];
        state.rhi_swap_chain[i].image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
}

void rhi_make_sync()
{
    VkResult result;

    VkFenceCreateInfo fence_info = { 0 };
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    result = vkCreateFence(state.device, &fence_info, NULL, &state.upload_fence);
    vk_check(result);

    result = vkCreateFence(state.device, &fence_info, NULL, &state.compute_fence);
    vk_check(result);

    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++) {
        result = vkCreateFence(state.device, &fence_info, NULL, &state.swap_chain_fences[i]);
        vk_check(result);
    }

    VkSemaphoreCreateInfo semaphore_info = { 0 };
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    result = vkCreateSemaphore(state.device, &semaphore_info, NULL, &state.image_available_semaphore);
    vk_check(result);
    result = vkCreateSemaphore(state.device, &semaphore_info, NULL, &state.image_rendered_semaphore);
    vk_check(result);
}

void rhi_make_cmd()
{
    VkCommandPoolCreateInfo command_pool_create_info = {0};
    command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_pool_create_info.queueFamilyIndex = state.graphics_family;
    command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VkResult result = vkCreateCommandPool(state.device, &command_pool_create_info, NULL, &state.graphics_pool);
    vk_check(result);
    result = vkCreateCommandPool(state.device, &command_pool_create_info, NULL, &state.upload_pool);
    vk_check(result);

    command_pool_create_info.queueFamilyIndex = state.compute_family;
    result = vkCreateCommandPool(state.device, &command_pool_create_info, NULL, &state.compute_pool);
    vk_check(result);

    for (i32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        rhi_init_cmd_buf(&state.swap_chain_cmd_bufs[i], COMMAND_BUFFER_GRAPHICS);
}

void rhi_make_allocator()
{
    VmaAllocatorCreateInfo allocator_info = { 0 };
    allocator_info.device = state.device;
    allocator_info.instance = state.instance;
    allocator_info.physicalDevice = state.physical_device;
    allocator_info.vulkanApiVersion = VK_API_VERSION_1_2;
    VmaVulkanFunctions vulkanFunctions = {
            vkGetPhysicalDeviceProperties,
            vkGetPhysicalDeviceMemoryProperties,
            vkAllocateMemory,
            vkFreeMemory,
            vkMapMemory,
            vkUnmapMemory,
            vkFlushMappedMemoryRanges,
            vkInvalidateMappedMemoryRanges,
            vkBindBufferMemory,
            vkBindImageMemory,
            vkGetBufferMemoryRequirements,
            vkGetImageMemoryRequirements,
            vkCreateBuffer,
            vkDestroyBuffer,
            vkCreateImage,
            vkDestroyImage,
            vkCmdCopyBuffer,
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
                vkGetBufferMemoryRequirements2,
                vkGetImageMemoryRequirements2,
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
                vkBindBufferMemory2,
                vkBindImageMemory2,
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
                vkGetPhysicalDeviceMemoryProperties2,
#endif
    };

    allocator_info.pVulkanFunctions = &vulkanFunctions;

    VkResult result = vmaCreateAllocator(&allocator_info, &state.allocator);
    assert(result == VK_SUCCESS);
}

void rhi_init()
{
    memset(&state, 0, sizeof(vk_state));
    vk_check(volkInitialize());
    
    rhi_make_instance();
    aurora_platform_create_vk_surface(state.instance, &state.surface);
    rhi_make_physical_device();
    rhi_make_device();
    rhi_make_swapchain();
    rhi_make_sync();
    rhi_make_cmd();
    rhi_make_allocator();
}

void rhi_begin()
{
    vkAcquireNextImageKHR(state.device, state.swap_chain, UINT32_MAX, state.image_available_semaphore, VK_NULL_HANDLE, (u32*)&state.image_index);

    rhi_command_buf* cmd_buf = &state.swap_chain_cmd_bufs[state.image_index];

    vkWaitForFences(state.device, 1, &state.swap_chain_fences[state.image_index], VK_TRUE, UINT32_MAX);
    vkResetFences(state.device, 1, &state.swap_chain_fences[state.image_index]);
    vkResetCommandBuffer(cmd_buf->buf, 0);

    rhi_begin_cmd_buf(cmd_buf);
}

void rhi_end()
{
    rhi_command_buf cmd_buf = state.swap_chain_cmd_bufs[state.image_index];
    rhi_end_cmd_buf(&cmd_buf);

    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore wait_semaphores[] = { state.image_available_semaphore };
    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf.buf;

    VkSemaphore signal_semaphores[] = { state.image_rendered_semaphore };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    vkResetFences(state.device, 1, &state.swap_chain_fences[state.image_index]);

    VkResult result = vkQueueSubmit(state.graphics_queue, 1, &submit_info, state.swap_chain_fences[state.image_index]);
    vk_check(result);
}

void rhi_present()
{
    VkSemaphore signal_semaphores[] = { state.image_rendered_semaphore };
    VkPresentInfoKHR present_info = { 0 };
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    VkSwapchainKHR swap_chains[] = { state.swap_chain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = (u32*)&state.image_index;

    vkQueuePresentKHR(state.graphics_queue, &present_info);
}

void rhi_shutdown()
{
    vkDeviceWaitIdle(state.device);

    vmaDestroyAllocator(state.allocator);

    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
    {
        vkDestroyFence(state.device, state.swap_chain_fences[i], NULL);
        vkDestroyImageView(state.device, state.swap_chain_image_views[i], NULL);
    }

    free(state.swap_chain_images);
    vkDestroySemaphore(state.device, state.image_available_semaphore, NULL);
    vkDestroySemaphore(state.device, state.image_rendered_semaphore, NULL);
    vkDestroySwapchainKHR(state.device, state.swap_chain, NULL);
    vkDestroyCommandPool(state.device, state.compute_pool, NULL);
    vkDestroyFence(state.device, state.compute_fence, NULL);
    vkDestroyCommandPool(state.device, state.upload_pool, NULL);
    vkDestroyFence(state.device, state.upload_fence, NULL);
    vkDestroyCommandPool(state.device, state.graphics_pool, NULL);
    vkDestroyDevice(state.device, NULL);
    vkDestroySurfaceKHR(state.instance, state.surface, NULL);
    vkDestroyInstance(state.instance, NULL);
}

void rhi_resize()
{
    if (state.swap_chain != VK_NULL_HANDLE)
    {
        for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
            vkDestroyImageView(state.device, state.swap_chain_image_views[i], NULL);
    
        free(state.swap_chain_images);
        vkDestroySwapchainKHR(state.device, state.swap_chain, NULL);
    
        rhi_make_swapchain();
    }
}

void rhi_wait_idle()
{
    if (state.device)
        vkDeviceWaitIdle(state.device);
}

rhi_image* rhi_get_swapchain_image()
{
    return &state.rhi_swap_chain[state.image_index];
}

rhi_command_buf* rhi_get_swapchain_cmd_buf()
{
    return &state.swap_chain_cmd_bufs[state.image_index];
}

void rhi_load_shader(rhi_shader_module* shader, const char* path)
{
    shader->byte_code = (u32*)aurora_platform_read_file(path, &shader->byte_code_size);
    
    VkShaderModuleCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = shader->byte_code_size;
    create_info.pCode = shader->byte_code;
    
    VkResult res = vkCreateShaderModule(state.device, &create_info, NULL, &shader->shader_module);
    vk_check(res);
}

void rhi_free_shader(rhi_shader_module* shader)
{
    vkDestroyShaderModule(state.device, shader->shader_module, NULL);
    free(shader->byte_code);
}

void rhi_init_cmd_buf(rhi_command_buf* buf, u32 command_buffer_type)
{
    buf->command_buffer_type = command_buffer_type;

    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_buffer_type == COMMAND_BUFFER_GRAPHICS ? state.graphics_pool : state.compute_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkResult result = vkAllocateCommandBuffers(state.device, &alloc_info, &buf->buf);
    vk_check(result);
}

void rhi_init_upload_cmd_buf(rhi_command_buf* buf)
{   
    buf->command_buffer_type = COMMAND_BUFFER_UPLOAD;

    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = state.upload_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkResult result = vkAllocateCommandBuffers(state.device, &alloc_info, &buf->buf);
    vk_check(result);
}

void rhi_submit_cmd_buf(rhi_command_buf* buf)
{   
    rhi_end_cmd_buf(buf);

    VkQueue submit_queue = buf->command_buffer_type == COMMAND_BUFFER_GRAPHICS ? state.graphics_queue : state.compute_queue;

    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buf->buf;

    if (buf->command_buffer_type == COMMAND_BUFFER_COMPUTE)
    {
        vkQueueSubmit(state.compute_queue, 1, &submit_info, state.compute_fence);
        vkWaitForFences(state.device, 1, &state.compute_fence, VK_TRUE, UINT32_MAX);
        vkResetFences(state.device, 1, &state.compute_fence);
        vkResetCommandPool(state.device, state.compute_pool, 0);
    }
    else
    {
        vkQueueSubmit(submit_queue, 1, &submit_info, VK_NULL_HANDLE);
        vkQueueWaitIdle(submit_queue);
    }
}

void rhi_submit_upload_cmd_buf(rhi_command_buf* buf)
{
    VkSubmitInfo submit_info = { 0 };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &buf->buf;

    vkQueueSubmit(state.graphics_queue, 1, &submit_info, state.upload_fence);
    vkWaitForFences(state.device, 1, &state.upload_fence, VK_TRUE, INT_MAX);
    vkResetFences(state.device, 1, &state.upload_fence);
    vkResetCommandPool(state.device, state.upload_pool, 0);
}

void rhi_begin_cmd_buf(rhi_command_buf* buf)
{
    VkCommandBufferBeginInfo begin_info = { 0 };
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(buf->buf, &begin_info);
    vk_check(result);
}

void rhi_end_cmd_buf(rhi_command_buf* buf)
{
    vk_check(vkEndCommandBuffer(buf->buf));
}

void rhi_cmd_set_viewport(rhi_command_buf* buf, u32 width, u32 height)
{
    VkViewport viewport = { 0 };
    viewport.width = (f32)width;
    viewport.height = (f32)height;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = { 0 };
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = (u32)width;
    scissor.extent.height = (u32)height;

    vkCmdSetViewport(buf->buf, 0, 1, &viewport);
    vkCmdSetScissor(buf->buf, 0, 1, &scissor);
}

void rhi_cmd_start_render(rhi_command_buf* buf, rhi_render_begin info)
{
    u32 color_iterator = info.has_depth ? info.image_count - 1 : info.image_count;

    VkRect2D render_area = { 0 };
    render_area.extent.width = (u32)info.width;
    render_area.extent.height = (u32)info.height;
    render_area.offset.x = 0;
    render_area.offset.y = 0;

    VkRenderingInfoKHR rendering_info = { 0 };
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    rendering_info.renderArea = render_area;
    rendering_info.colorAttachmentCount = color_iterator;
    rendering_info.layerCount = 1;

    // Max attachment count is 64
    VkRenderingAttachmentInfoKHR color_attachments[64] = { 0 };

    for (u32 i = 0; i < color_iterator; i++)
    {
        rhi_image* image = info.images[i];

        VkClearValue clear_value = { 0 };
        clear_value.color.float32[0] = info.r;
        clear_value.color.float32[1] = info.g;
        clear_value.color.float32[2] = info.b;
        clear_value.color.float32[3] = info.a;

        VkRenderingAttachmentInfoKHR color_attachment_info = { 0 };
        color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        color_attachment_info.imageView = image->image_view;
        color_attachment_info.imageLayout = image->image_layout;
        color_attachment_info.resolveMode = VK_RESOLVE_MODE_NONE;
        color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info.clearValue = clear_value;

        color_attachments[i] = color_attachment_info;
    }

    if (info.has_depth)
    {
        rhi_image* image = info.images[color_iterator];

        VkClearValue depth_clear_value = { 0 };
        depth_clear_value.depthStencil.depth = 1.0f;
        depth_clear_value.depthStencil.stencil = 0.0f;

        VkRenderingAttachmentInfoKHR depth_attachment = { 0 };
        depth_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        depth_attachment.imageView = image->image_view;
        depth_attachment.imageLayout = image->image_layout;
        depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.clearValue = depth_clear_value;

        rendering_info.pStencilAttachment = &depth_attachment;
        rendering_info.pDepthAttachment = &depth_attachment;
    }

    rendering_info.pColorAttachments = color_attachments;

    vkCmdBeginRenderingKHR(buf->buf, &rendering_info);
}

void rhi_cmd_end_render(rhi_command_buf* buf)
{
    vkCmdEndRenderingKHR(buf->buf);
}

void rhi_cmd_img_transition_layout(rhi_command_buf* buf, rhi_image* img, u32 src_access, u32 dst_access, u32 src_layout, u32 dst_layout, u32 src_p_stage, u32 dst_p_stage, u32 layer)
{
    VkImageSubresourceRange range = { 0 };
    range.baseMipLevel = 0;
    range.levelCount = VK_REMAINING_MIP_LEVELS;
    range.baseArrayLayer = layer;
    range.layerCount = VK_REMAINING_ARRAY_LAYERS;
    range.aspectMask = (VkImageAspectFlagBits)vk_get_image_aspect(img->format);

    VkImageMemoryBarrier barrier = { 0 };
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcAccessMask = src_access;
    barrier.dstAccessMask = dst_access;
    barrier.oldLayout = src_layout;
    barrier.newLayout = dst_layout;
    barrier.image = img->image;
    barrier.subresourceRange = range;

    vkCmdPipelineBarrier(buf->buf, src_p_stage, dst_p_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}