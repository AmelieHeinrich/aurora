#include "rhi.h"
#include "platform_layer.h"

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

    VkSwapchainKHR swap_chain;
    VkExtent2D swap_chain_extent;
    VkFormat swap_chain_format;
    VkImage* swap_chain_images;
    VkImageView swap_chain_image_views[FRAMES_IN_FLIGHT];
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
    }
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
}

void rhi_shutdown()
{
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        vkDestroyImageView(state.device, state.swap_chain_image_views[i], NULL);

    free(state.swap_chain_images);
    vkDestroySwapchainKHR(state.device, state.swap_chain, NULL);
    vkDestroyDevice(state.device, NULL);
    vkDestroySurfaceKHR(state.instance, state.surface, NULL);
    vkDestroyInstance(state.instance, NULL);
}