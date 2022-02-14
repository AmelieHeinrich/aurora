#include "rhi.h"
#include "platform_layer.h"
#include "vk_utils.h"
#include "spirv_reflect.h"
#include "stb_image.h"

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
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout image_heap_layout;
    VkDescriptorSetLayout sampler_heap_layout;

    rhi_descriptor_set_layout rhi_image_heap;
    rhi_descriptor_set_layout rhi_sampler_heap;
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

    VkPhysicalDeviceDescriptorIndexingFeatures indexing_features = {0};
    indexing_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
    indexing_features.descriptorBindingPartiallyBound = 1;
    indexing_features.pNext = &mesh_shader_features;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_features = { 0 };
    dynamic_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
    dynamic_features.dynamicRendering = 1;
    dynamic_features.pNext = &indexing_features; 

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

            if (!strcmp(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, properties[i].extensionName)) {
                state.device_extensions[state.device_extension_count++] = VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME;
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
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
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

void rhi_make_descriptors()
{
    VkDescriptorPoolSize sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 4096 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4096 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4096 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 4096 }
    };

    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.pPoolSizes = sizes;
    pool_info.poolSizeCount = 4;
    pool_info.maxSets = 2048;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    VkResult res = vkCreateDescriptorPool(state.device, &pool_info, NULL, &state.descriptor_pool);
    vk_check(res);

    VkDescriptorSetLayoutBinding binding = {0};
    binding.binding = 0;
    binding.descriptorCount = 2048;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binding.stageFlags = VK_SHADER_STAGE_ALL;

    VkDescriptorBindingFlags flag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags = {0};
    binding_flags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    binding_flags.bindingCount = 1;
    binding_flags.pBindingFlags = &flag;

    VkDescriptorSetLayoutCreateInfo set_layout_info = {0};
    set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.bindingCount = 1;
    set_layout_info.pBindings = &binding;
    set_layout_info.pNext = &binding_flags;
    
    res = vkCreateDescriptorSetLayout(state.device, &set_layout_info, NULL, &state.image_heap_layout);
    vk_check(res);

    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;

    res = vkCreateDescriptorSetLayout(state.device, &set_layout_info, NULL, &state.sampler_heap_layout);
    vk_check(res);

    state.rhi_image_heap.binding = 0;
    state.rhi_image_heap.descriptors[0] = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    state.rhi_image_heap.descriptor_count = 2048;
    state.rhi_image_heap.layout = state.image_heap_layout;

    state.rhi_sampler_heap.binding = 1;
    state.rhi_sampler_heap.descriptors[0] = VK_DESCRIPTOR_TYPE_SAMPLER;
    state.rhi_sampler_heap.descriptor_count = 2048;
    state.rhi_sampler_heap.layout = state.sampler_heap_layout;
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
    rhi_make_descriptors();
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

    vkDestroyDescriptorSetLayout(state.device, state.sampler_heap_layout, NULL);
    vkDestroyDescriptorSetLayout(state.device, state.image_heap_layout, NULL);
    vkDestroyDescriptorPool(state.device, state.descriptor_pool, NULL);
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

rhi_descriptor_set_layout* rhi_get_image_heap_set_layout()
{
    return &state.rhi_image_heap;
}

rhi_descriptor_set_layout* rhi_get_sampler_heap_set_layout()
{
    return &state.rhi_sampler_heap;
}

void rhi_init_descriptor_set_layout(rhi_descriptor_set_layout* layout)
{
    VkDescriptorSetLayoutBinding bindings[32] = {0};
    for (i32 i = 0; i < layout->descriptor_count; i++)
    {
        bindings[i].binding = i;
        bindings[i].descriptorCount = 1;
        bindings[i].descriptorType = (VkDescriptorType)layout->descriptors[i];
        bindings[i].stageFlags = VK_SHADER_STAGE_ALL;
    }

    VkDescriptorSetLayoutCreateInfo set_layout_info = {0};
    set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set_layout_info.pBindings = bindings;
    set_layout_info.bindingCount = layout->descriptor_count;
    
    VkResult res = vkCreateDescriptorSetLayout(state.device, &set_layout_info, NULL, &layout->layout);
    vk_check(res);
}

void rhi_free_descriptor_set_layout(rhi_descriptor_set_layout* layout)
{
    vkDestroyDescriptorSetLayout(state.device, layout->layout, NULL);
}

void rhi_init_descriptor_set(rhi_descriptor_set* set, rhi_descriptor_set_layout* layout)
{
    VkDescriptorSetAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = state.descriptor_pool;
    alloc_info.pSetLayouts = &layout->layout;
    alloc_info.descriptorSetCount = 1;

    VkResult res = vkAllocateDescriptorSets(state.device, &alloc_info, &set->set);
    vk_check(res);
}

void rhi_free_descriptor_set(rhi_descriptor_set* set)
{
    vkFreeDescriptorSets(state.device, state.descriptor_pool, 1, &set->set);
}

void rhi_descriptor_set_write_buffer(rhi_descriptor_set* set, rhi_buffer* buffer, i32 size, i32 binding)
{
    VkDescriptorBufferInfo buffer_info = {0};
    buffer_info.buffer = buffer->buffer;
    buffer_info.offset = 0;
    buffer_info.range = size;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set->set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstArrayElement = 0;
    write.pBufferInfo = &buffer_info;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    vkUpdateDescriptorSets(state.device, 1, &write, 0, NULL);
}

void rhi_descriptor_set_write_image(rhi_descriptor_set* set, rhi_image* image, i32 binding)
{
    VkDescriptorImageInfo image_info = {0};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = image->image_view;
    image_info.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set->set;
    write.dstBinding = binding;
    write.descriptorCount = 1;
    write.dstArrayElement = 0;
    write.pImageInfo = &image_info;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

    vkUpdateDescriptorSets(state.device, 1, &write, 0, NULL);
}

void rhi_init_sampler(rhi_sampler* sampler)
{
    VkSamplerCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.magFilter = sampler->filter;
    create_info.minFilter = sampler->filter;
    create_info.addressModeU = sampler->address_mode;
    create_info.addressModeV = create_info.addressModeU;
    create_info.addressModeW = create_info.addressModeU;
    create_info.anisotropyEnable = 1;
    create_info.maxAnisotropy = state.physical_device_properties_2.properties.limits.maxSamplerAnisotropy;
    create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    create_info.compareEnable = 0;
    create_info.compareOp = VK_COMPARE_OP_NEVER;
    create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkResult res = vkCreateSampler(state.device, &create_info, NULL, &sampler->sampler);
    vk_check(res);
}

void rhi_free_sampler(rhi_sampler* sampler)
{
    vkDestroySampler(state.device, sampler->sampler, NULL);
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

void rhi_init_graphics_pipeline(rhi_pipeline* pipeline, rhi_pipeline_descriptor* descriptor)
{
    pipeline->bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
    pipeline->pipeline_type = PIPELINE_GRAPHICS;

    VkPipelineShaderStageCreateInfo pipeline_shader_stages[3];
    memset(pipeline_shader_stages, 0, sizeof(pipeline_shader_stages));
    i32 pipeline_shader_stage_count = 2;

    SpvReflectShaderModule input_layout_reflect;

    if (!descriptor->use_mesh_shaders)
    {
        SpvReflectResult reflect_result = spvReflectCreateShaderModule(descriptor->shaders.vs->byte_code_size, descriptor->shaders.vs->byte_code, &input_layout_reflect);
        assert(reflect_result == SPV_REFLECT_RESULT_SUCCESS);

        pipeline_shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        pipeline_shader_stages[0].module = descriptor->shaders.vs->shader_module;
        pipeline_shader_stages[0].pName = "main";

        pipeline_shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stages[1].module = descriptor->shaders.ps->shader_module;
        pipeline_shader_stages[1].pName = "main";
    }
    else
    {
        pipeline_shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stages[0].stage = VK_SHADER_STAGE_MESH_BIT_NV;
        pipeline_shader_stages[0].module = descriptor->shaders.ms->shader_module;
        pipeline_shader_stages[0].pName = "main";

        pipeline_shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stages[1].stage = VK_SHADER_STAGE_TASK_BIT_NV;
        pipeline_shader_stages[1].module = descriptor->shaders.ts->shader_module;
        pipeline_shader_stages[1].pName = "main";

        pipeline_shader_stages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        pipeline_shader_stages[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        pipeline_shader_stages[2].module = descriptor->shaders.ps->shader_module;
        pipeline_shader_stages[2].pName = "main";

        pipeline_shader_stage_count = 3;
    }

    VkVertexInputBindingDescription vertex_input_binding_desc = {0};
    VkPipelineVertexInputStateCreateInfo vertex_input_state_info = {0};
    VkPipelineInputAssemblyStateCreateInfo input_assembly = {0};
    VkVertexInputAttributeDescription* attribute_descriptions = NULL;
    SpvReflectInterfaceVariable** vs_input_vars = NULL;
    vertex_input_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    vertex_input_binding_desc.binding = 0;
    vertex_input_binding_desc.stride = 0; 
    vertex_input_binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    if (!descriptor->use_mesh_shaders)
    {
        u32 vs_input_var_count = 0;
        spvReflectEnumerateInputVariables(&input_layout_reflect, &vs_input_var_count, 0);
        vs_input_vars = calloc(vs_input_var_count, sizeof(SpvReflectInterfaceVariable*));
        spvReflectEnumerateInputVariables(&input_layout_reflect, &vs_input_var_count, vs_input_vars);

        attribute_descriptions = malloc(sizeof(VkVertexInputAttributeDescription) * vs_input_var_count);

        for (i32 location = 0; location < vs_input_var_count; location++)
        {
            for (u32 i = 0; i < vs_input_var_count; i++)
            {
                SpvReflectInterfaceVariable* var = vs_input_vars[i];

                if (var->location == (u32)location)
                {
                    VkVertexInputAttributeDescription attrib = { 0 };
    
                    attrib.location = var->location;
                    attrib.binding = 0;
                    attrib.format = (VkFormat)var->format;
                    attrib.offset = vertex_input_binding_desc.stride;

                    attribute_descriptions[i] = attrib;

                    vertex_input_binding_desc.stride += vk_get_format_size(attrib.format);
                }
            }
        }

        vertex_input_state_info.vertexBindingDescriptionCount = vertex_input_binding_desc.stride == 0 ? 0 : 1;
        vertex_input_state_info.pVertexBindingDescriptions = &vertex_input_binding_desc;
        vertex_input_state_info.vertexAttributeDescriptionCount = vs_input_var_count;
        vertex_input_state_info.pVertexAttributeDescriptions = attribute_descriptions;

        input_assembly.topology = descriptor->primitive_topology;
        input_assembly.primitiveRestartEnable = VK_FALSE;
    }

    VkDynamicState states[3] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {0};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = descriptor->depth_biased_enable ? 3 : 2;
    dynamic_state.pDynamicStates = states;

    VkViewport viewport = {0};
    viewport.width = (f32)platform.width;
    viewport.height = (f32)platform.height;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {0};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = state.swap_chain_extent;

    VkPipelineViewportStateCreateInfo viewport_state = {0};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {0};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = descriptor->polygon_mode;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = descriptor->cull_mode;
    rasterizer.frontFace = descriptor->front_face;
    rasterizer.depthBiasEnable = descriptor->depth_biased_enable ? VK_TRUE : VK_FALSE;
    
    VkPipelineMultisampleStateCreateInfo multisampling = {0};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blends[32] = {0};
    for (i32 i = 0; i < descriptor->color_attachment_count; i++)
    {
        color_blends[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blends[i].blendEnable = VK_FALSE;
    }

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {0};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = (VkCompareOp)descriptor->depth_op;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.minDepthBounds = 0.0f; // Optional
    depth_stencil.maxDepthBounds = 1.0f; // Optional
    depth_stencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending = {0};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;
    color_blending.attachmentCount = descriptor->color_attachment_count;
    color_blending.pAttachments = color_blends;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (descriptor->set_layout_count > 0)
    {
        VkDescriptorSetLayout layouts[16];
        for (i32 i = 0; i < descriptor->set_layout_count; i++)
            layouts[i] = descriptor->set_layouts[i]->layout;

        pipeline_layout_info.setLayoutCount = descriptor->set_layout_count;
        pipeline_layout_info.pSetLayouts = layouts;
    }
    if (descriptor->push_constant_size > 0)
    {
        VkPushConstantRange range = {0};
        range.offset = 0;
        range.size = descriptor->push_constant_size;
        range.stageFlags = VK_SHADER_STAGE_ALL;

        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &range;
    }

    VkResult res = vkCreatePipelineLayout(state.device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout);
    vk_check(res);

    VkPipelineRenderingCreateInfoKHR rendering_create_info = {0};
    rendering_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    rendering_create_info.colorAttachmentCount = descriptor->color_attachment_count;
    rendering_create_info.depthAttachmentFormat = descriptor->depth_attachment_format;
    rendering_create_info.stencilAttachmentFormat = descriptor->depth_attachment_format;
    rendering_create_info.pColorAttachmentFormats = (VkFormat*)descriptor->color_attachments_formats;
    rendering_create_info.pNext = VK_NULL_HANDLE;

    VkGraphicsPipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = pipeline_shader_stage_count;
    pipeline_info.pStages = pipeline_shader_stages;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = pipeline->pipeline_layout;
    pipeline_info.renderPass = VK_NULL_HANDLE;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.pNext = &rendering_create_info;

    if (!descriptor->use_mesh_shaders)
    {
        pipeline_info.pVertexInputState = &vertex_input_state_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
    }

    res = vkCreateGraphicsPipelines(state.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &pipeline->pipeline);
    vk_check(res);
}

void rhi_init_compute_pipeline(rhi_pipeline* pipeline, rhi_pipeline_descriptor* descriptor)
{
    pipeline->bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
    pipeline->pipeline_type = PIPELINE_COMPUTE;

    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (descriptor->set_layout_count > 0)
    {
        VkDescriptorSetLayout layouts[16];
        for (i32 i = 0; i < descriptor->set_layout_count; i++)
            layouts[i] = descriptor->set_layouts[i]->layout;

        pipeline_layout_info.setLayoutCount = descriptor->set_layout_count;
        pipeline_layout_info.pSetLayouts = layouts;
    }
    if (descriptor->push_constant_size > 0)
    {
        VkPushConstantRange range = {0};
        range.offset = 0;
        range.size = descriptor->push_constant_size;
        range.stageFlags = VK_SHADER_STAGE_ALL;

        pipeline_layout_info.pushConstantRangeCount = 1;
        pipeline_layout_info.pPushConstantRanges = &range;
    }

    VkResult res = vkCreatePipelineLayout(state.device, &pipeline_layout_info, NULL, &pipeline->pipeline_layout);
    vk_check(res);

    VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = descriptor->shaders.cs->shader_module;
    info.stage.pName = "main";
    info.layout = pipeline->pipeline_layout;

    res = vkCreateComputePipelines(state.device, VK_NULL_HANDLE, 1, &info, NULL, &pipeline->pipeline);
    vk_check(res);
}

void rhi_free_pipeline(rhi_pipeline* pipeline)
{
    vkDestroyPipeline(state.device, pipeline->pipeline, NULL);
    vkDestroyPipelineLayout(state.device, pipeline->pipeline_layout, NULL);
}

void rhi_allocate_buffer(rhi_buffer* buffer, u64 size, u32 buffer_usage)
{
    VkBufferCreateInfo buffer_create_info = {0};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.usage = (VkBufferUsageFlagBits)buffer_usage;

    VmaAllocationCreateInfo allocation_create_info = {0};
    allocation_create_info.usage = vk_get_memory_usage(buffer_create_info.usage);

    VkResult result = vmaCreateBuffer(state.allocator, &buffer_create_info, &allocation_create_info, &buffer->buffer, &buffer->allocation, NULL);
    vk_check(result);
}

void rhi_free_buffer(rhi_buffer* buffer)
{
    vmaDestroyBuffer(state.allocator, buffer->buffer, buffer->allocation);
}

void rhi_upload_buffer(rhi_buffer* buffer, void* data, u64 size)
{
    void* buf;
    vk_check(vmaMapMemory(state.allocator, buffer->allocation, &buf));
    memcpy(buf, data, size);
    vmaUnmapMemory(state.allocator, buffer->allocation);
}

void rhi_allocate_image(rhi_image* image, i32 width, i32 height, VkFormat format, u32 usage)
{
    image->width = width;
    image->height = height;
    image->format = format;
    image->image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->usage = usage;
    image->extent.width = width;
    image->extent.height = height;

    VkImageCreateInfo image_create_info = { 0 };
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.format = format;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = (VkImageUsageFlagBits)usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation = { 0 };
    allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult res = vmaCreateImage(state.allocator, &image_create_info, &allocation, &image->image, &image->allocation, NULL);
    vk_check(res);

    VkImageViewCreateInfo view_info = { 0 };
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = image->format;
    view_info.subresourceRange.aspectMask = vk_get_image_aspect(image->format);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    res = vkCreateImageView(state.device, &view_info, NULL, &image->image_view);
    vk_check(res);
}

void rhi_allocate_cubemap(rhi_image* image, i32 width, i32 height, VkFormat format, u32 usage)
{
    image->width = width;
    image->height = height;
    image->format = format;
    image->image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->usage = usage;
    image->extent.width = width;
    image->extent.height = height;

    VkImageCreateInfo image_create_info = { 0 };
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.format = format;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 6;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = (VkImageUsageFlagBits)usage;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocation = { 0 };
    allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult res = vmaCreateImage(state.allocator, &image_create_info, &allocation, &image->image, &image->allocation, NULL);
    vk_check(res);

    VkImageViewCreateInfo view_info = { 0 };
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    view_info.format = image->format;
    view_info.subresourceRange.aspectMask = vk_get_image_aspect(image->format);
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 6;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    res = vkCreateImageView(state.device, &view_info, NULL, &image->image_view);
    vk_check(res);
}

void rhi_load_image(rhi_image* image, const char* path)
{
    i32 width, height, channels = 0;
    u8* data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
    assert(data);
    i64 image_size = width * height * 4;

    image->format = VK_FORMAT_R8G8B8A8_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->width = width;
    image->height = height;
    image->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkImageCreateInfo image_create_info = { 0 };
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.format = image->format;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation = { 0 };
    allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult res = vmaCreateImage(state.allocator, &image_create_info, &allocation, &image->image, &image->allocation, NULL);
    vk_check(res);

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_buffer_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo staging_buffer_allocation_info = {0};

    VkBufferCreateInfo staging_buffer_info = { 0 };
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.size = image_size;

    VmaAllocationCreateInfo staging_buffer_alloc_info = { 0 };
    staging_buffer_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    res = vmaCreateBuffer(state.allocator, &staging_buffer_info, &staging_buffer_alloc_info, &staging_buffer, &staging_buffer_allocation, NULL);
    vk_check(res);

    void* upload_data;
    vmaMapMemory(state.allocator, staging_buffer_allocation, &upload_data);
    memcpy(upload_data, data, image_size);
    vmaUnmapMemory(state.allocator, staging_buffer_allocation);

    VkBufferImageCopy image_copy_region = {0};
    image_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy_region.imageSubresource.mipLevel = 0;
    image_copy_region.imageSubresource.baseArrayLayer = 0;
    image_copy_region.imageSubresource.layerCount = 1;
    image_copy_region.imageExtent.width = width;
    image_copy_region.imageExtent.height = height;
    image_copy_region.imageExtent.depth = 1;

    rhi_command_buf temp;
    rhi_init_upload_cmd_buf(&temp);
    rhi_begin_cmd_buf(&temp);
    rhi_cmd_img_transition_layout(&temp, image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0);
    vkCmdCopyBufferToImage(temp.buf, staging_buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy_region);
    rhi_cmd_img_transition_layout(&temp, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    rhi_end_cmd_buf(&temp);
    rhi_submit_upload_cmd_buf(&temp);

    vmaDestroyBuffer(state.allocator, staging_buffer, staging_buffer_allocation);
    stbi_image_free(data);

    VkImageViewCreateInfo view_info = { 0 };
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = image->format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    res = vkCreateImageView(state.device, &view_info, NULL, &image->image_view);
    assert(res == VK_SUCCESS);
}

void rhi_load_hdr_image(rhi_image* image, const char* path)
{
    i32 width, height, channels = 0;
    u16* data = stbi_load_16(path, &width, &height, &channels, STBI_rgb_alpha);
    assert(data);
    i64 image_size = width * height * 16;

    image->format = VK_FORMAT_R16G16B16A16_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->width = width;
    image->height = height;
    image->usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    
    VkImageCreateInfo image_create_info = { 0 };
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = width;
    image_create_info.extent.height = height;
    image_create_info.format = image->format;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocation = { 0 };
    allocation.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult res = vmaCreateImage(state.allocator, &image_create_info, &allocation, &image->image, &image->allocation, NULL);
    vk_check(res);

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VmaAllocation staging_buffer_allocation = VK_NULL_HANDLE;
    VmaAllocationInfo staging_buffer_allocation_info = {0};

    VkBufferCreateInfo staging_buffer_info = { 0 };
    staging_buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    staging_buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    staging_buffer_info.size = image_size;

    VmaAllocationCreateInfo staging_buffer_alloc_info = { 0 };
    staging_buffer_alloc_info.usage = VMA_MEMORY_USAGE_CPU_ONLY;

    res = vmaCreateBuffer(state.allocator, &staging_buffer_info, &staging_buffer_alloc_info, &staging_buffer, &staging_buffer_allocation, NULL);
    vk_check(res);

    void* upload_data;
    vmaMapMemory(state.allocator, staging_buffer_allocation, &upload_data);
    memcpy(upload_data, data, image_size);
    vmaUnmapMemory(state.allocator, staging_buffer_allocation);

    VkBufferImageCopy image_copy_region = {0};
    image_copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy_region.imageSubresource.mipLevel = 0;
    image_copy_region.imageSubresource.baseArrayLayer = 0;
    image_copy_region.imageSubresource.layerCount = 1;
    image_copy_region.imageExtent.width = width;
    image_copy_region.imageExtent.height = height;
    image_copy_region.imageExtent.depth = 1;

    rhi_command_buf temp;
    rhi_init_upload_cmd_buf(&temp);
    rhi_begin_cmd_buf(&temp);
    rhi_cmd_img_transition_layout(&temp, image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0);
    vkCmdCopyBufferToImage(temp.buf, staging_buffer, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy_region);
    rhi_cmd_img_transition_layout(&temp, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0);
    rhi_end_cmd_buf(&temp);
    rhi_submit_upload_cmd_buf(&temp);

    vmaDestroyBuffer(state.allocator, staging_buffer, staging_buffer_allocation);
    stbi_image_free(data);

    VkImageViewCreateInfo view_info = { 0 };
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image->image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = image->format;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    res = vkCreateImageView(state.device, &view_info, NULL, &image->image_view);
    assert(res == VK_SUCCESS);
}

void rhi_free_image(rhi_image* image)
{
    vkDestroyImageView(state.device, image->image_view, NULL);
    vmaDestroyImage(state.allocator, image->image, image->allocation);
}

void rhi_resize_image(rhi_image* image, i32 width, i32 height)
{
    if (image->image != VK_NULL_HANDLE)
    {
        image->width = width;
        image->height = height;
        rhi_free_image(image);
        rhi_allocate_image(image, image->width, image->height, image->format, image->usage);
    }
}

void rhi_init_descriptor_heap(rhi_descriptor_heap* heap, u32 type, u32 size)
{
    memset(heap, 0, sizeof(rhi_descriptor_heap));
    heap->heap_handle = aurora_platform_layer_halloc(sizeof(b32) * size);
    memset(heap->heap_handle, 0, sizeof(b32) * size);
    heap->type = type;
    heap->used = 0;
    heap->size = size;

    VkDescriptorSetAllocateInfo descriptor_set_info = {0};
    descriptor_set_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptor_set_info.descriptorSetCount = 1;
    descriptor_set_info.descriptorPool = state.descriptor_pool;
    descriptor_set_info.pSetLayouts = type == DESCRIPTOR_HEAP_IMAGE ? &state.image_heap_layout : &state.sampler_heap_layout;

    VkResult res = vkAllocateDescriptorSets(state.device, &descriptor_set_info, &heap->set);
    vk_check(res);
}

i32 rhi_find_available_descriptor(rhi_descriptor_heap* heap)
{
    for (i32 i = 0; i < heap->size; i++)
    {
        if (heap->heap_handle[i] == 0)
        {
            heap->heap_handle[i] = 1;
            heap->used++;
            return i;
        }
    }

    return -1;
}

void rhi_push_descriptor_heap_image(rhi_descriptor_heap* heap, rhi_image* image, i32 binding)
{
    VkDescriptorImageInfo image_info = {0};
    image_info.imageLayout = image->image_layout;
    image_info.imageView = image->image_view;
    image_info.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.dstArrayElement = binding;
    write.dstBinding = 0;
    write.dstSet = heap->set;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(state.device, 1, &write, 0, NULL);
}

void rhi_push_descriptor_heap_sampler(rhi_descriptor_heap* heap, rhi_sampler* sampler, i32 binding)
{
    VkDescriptorImageInfo image_info = {0};
    image_info.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.imageView = VK_NULL_HANDLE;
    image_info.sampler = sampler->sampler;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.dstArrayElement = binding;
    write.dstBinding = 0;
    write.dstSet = heap->set;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(state.device, 1, &write, 0, NULL);
}

void rhi_free_descriptor(rhi_descriptor_heap* heap, u32 descriptor)
{
    heap->heap_handle[descriptor] = 0;
    heap->used--;
}

void rhi_free_descriptor_heap(rhi_descriptor_heap* heap)
{
    vkFreeDescriptorSets(state.device, state.descriptor_pool, 1, &heap->set);
    free(heap->heap_handle);
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

void rhi_cmd_set_pipeline(rhi_command_buf* buf, rhi_pipeline* pipeline)
{
    vkCmdBindPipeline(buf->buf, pipeline->bind_point, pipeline->pipeline);
}

void rhi_cmd_set_vertex_buffer(rhi_command_buf* buf, rhi_buffer* buffer)
{
    VkBuffer buffers[] = { buffer->buffer };
    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(buf->buf, 0, 1, buffers, offsets);
}

void rhi_cmd_set_index_buffer(rhi_command_buf* buf, rhi_buffer* buffer)
{
    vkCmdBindIndexBuffer(buf->buf, buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
}

void rhi_cmd_set_descriptor_heap(rhi_command_buf* buf, rhi_pipeline* pipeline, rhi_descriptor_heap* heap, i32 binding)
{
    vkCmdBindDescriptorSets(buf->buf, pipeline->bind_point, pipeline->pipeline_layout, binding, 1, &heap->set, 0, NULL);
}

void rhi_cmd_set_descriptor_set(rhi_command_buf* buf, rhi_pipeline* pipeline, rhi_descriptor_set* set, i32 binding)
{
    vkCmdBindDescriptorSets(buf->buf, pipeline->bind_point, pipeline->pipeline_layout, binding, 1, &set->set, 0, NULL);
}

void rhi_cmd_set_push_constants(rhi_command_buf* buf, rhi_pipeline* pipeline, void* data, u32 size)
{
    vkCmdPushConstants(buf->buf, pipeline->pipeline_layout, VK_SHADER_STAGE_ALL, 0, size, data);
}

void rhi_cmd_draw_indexed(rhi_command_buf* buf, u32 count)
{
    vkCmdDrawIndexed(buf->buf, count, 1, 0, 0, 0);
}

void rhi_cmd_draw(rhi_command_buf* buf, u32 count)
{
    vkCmdDraw(buf->buf, count, 1, 0, 0);
}

void rhi_cmd_draw_meshlets(rhi_command_buf* buf, u32 count)
{
    vkCmdDrawMeshTasksNV(buf->buf, count, 0);
}

void rhi_cmd_dispatch(rhi_command_buf* buf, u32 x, u32 y, u32 z)
{
    vkCmdDispatch(buf->buf, x, y, z);
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
        color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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
        depth_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        depth_attachment.resolveMode = VK_RESOLVE_MODE_NONE;
        depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
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

void rhi_cmd_img_blit(rhi_command_buf* buf, rhi_image* src, rhi_image* dst, u32 srcl, u32 dstl)
{
    VkImageBlit region = { 0 };
    region.srcOffsets[1].x = src->extent.width;
    region.srcOffsets[1].y = src->extent.height;
    region.srcOffsets[1].z = 1;
    region.srcSubresource.layerCount = 1;
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstOffsets[1].x = dst->extent.width;
    region.dstOffsets[1].y = dst->extent.height;
    region.dstOffsets[1].z = 1;
    region.dstSubresource.layerCount = 1;
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    vkCmdBlitImage(buf->buf, src->image, srcl, dst->image, dstl, 1, &region, VK_FILTER_NEAREST);
}