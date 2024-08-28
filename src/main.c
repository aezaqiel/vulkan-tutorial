#include "defines.h"

#include <volk.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cglm/cglm.h>

#define MAX_FRAMES_IN_FLIGHT 2

typedef struct vertex {
    vec2 position;
    vec3 color;
} vertex;

typedef struct queue_family {
    VkQueue queue;
    unsigned char index;
} queue_family;

typedef struct device_state {
    VkPhysicalDevice physical_device;
    VkDevice device;
    queue_family graphics_queue;
    queue_family present_queue;
} device_state;

typedef struct surface_state {
    VkSurfaceKHR surface;
    VkSurfaceCapabilitiesKHR capabilities;
    VkFormat format;
    VkColorSpaceKHR color_space;
    VkPresentModeKHR present_mode;
    VkExtent2D extent;
} surface_state;

typedef struct swapchain_state {
    VkSwapchainKHR swapchain;
    u32 image_count;
    VkImage* images;
    VkImageView* image_views;
} swapchain_state;

typedef struct pipeline_state {
    VkPipelineLayout layout;
    VkPipeline pipeline;
} pipeline_state;

typedef struct application_state {
    GLFWwindow* window;
    VkInstance instance;
    VkDebugUtilsMessengerEXT debug_messenger;
    surface_state surface;
    device_state device;
    swapchain_state swapchain;
    VkRenderPass render_pass;
    VkFramebuffer* framebuffers;
    pipeline_state graphics_pipeline;
    VkCommandPool command_pool;
    VkCommandBuffer* command_buffers;
    VkSemaphore* image_available_semaphores;
    VkSemaphore* render_finished_semaphores;
    VkFence* in_flight_fences;
    unsigned char current_frame;
    bool framebuffer_resized;
    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
} application_state;

static void glfw_error_callback(int code, const char* description)
{
    fprintf(stderr, "glfw error %d: %s\n", code, description);
}

static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
    (void)width;
    (void)height;

    application_state* state = (application_state*)glfwGetWindowUserPointer(window);
    state->framebuffer_resized = true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    (void)message_severity;
    (void)message_type;
    (void)user_data;

    fprintf(stderr, "%s", callback_data->pMessage);

    return VK_FALSE;
}

void recreate_swapchain(application_state* state);

void initialize_window(application_state* state)
{
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    state->window = glfwCreateWindow(1280, 720, "vulkan tutorial", NULL, NULL);
    glfwSetWindowUserPointer(state->window, state);
    glfwSetFramebufferSizeCallback(state->window, glfw_framebuffer_resize_callback);
}

void create_instance(application_state* state)
{
    VkApplicationInfo info;
    info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    info.pNext = NULL;
    info.pApplicationName = "vulkan tutorial";
    info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    info.pEngineName = "no engine";
    info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    info.apiVersion = VK_API_VERSION_1_3;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#ifndef NDEBUG
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

    VkInstanceCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.pApplicationInfo = &info;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = NULL;
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    create_info.ppEnabledExtensionNames = extensions;

#ifndef NDEBUG
    const char* layers[] = {
        "VK_LAYER_KHRONOS_validation"
    };

    create_info.enabledLayerCount = sizeof(layers) / sizeof(layers[0]);
    create_info.ppEnabledLayerNames = layers;

    VkDebugUtilsMessengerCreateInfoEXT messenger_info;
    messenger_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    messenger_info.pNext = NULL;
    messenger_info.flags = 0;
    // messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    messenger_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
    messenger_info.pfnUserCallback = debug_messenger_callback;
    messenger_info.pUserData = NULL;

    create_info.pNext = &messenger_info;

#endif

    if (vkCreateInstance(&create_info, NULL, &state->instance) != VK_SUCCESS) {
        fprintf(stderr, "failed to create instance\n");
    }

    volkLoadInstance(state->instance);

#ifndef NDEBUG
    if (vkCreateDebugUtilsMessengerEXT(state->instance, &messenger_info, NULL, &state->debug_messenger) != VK_SUCCESS) {
        fprintf(stderr, "failed to create debug messenger\n");
    }
#endif
}

void create_surface(application_state* state)
{
    if (glfwCreateWindowSurface(state->instance, state->window, NULL, &state->surface.surface) != VK_SUCCESS) {
        fprintf(stderr, "failed to create surface\n");
    }
}

bool physical_device_suitable(VkPhysicalDevice device, application_state* state)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);

    printf("checking physical device: %s\n", properties.deviceName);

    if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        return false;
    }

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    if (!features.geometryShader) {
        return false;
    }

    unsigned int queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)calloc(queue_family_count, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    bool graphics_queue_available = false;
    bool present_queue_available = false;

    for (unsigned int i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_queue_available = true;
        }

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, state->surface.surface, &present_supported);
        if (present_supported == VK_TRUE) {
            present_queue_available = true;
        }
    }

    free(queue_families);

    if (!graphics_queue_available || !present_queue_available) {
        return false;
    }

    unsigned int format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, state->surface.surface, &format_count, NULL);

    if (format_count == 0) {
        return false;
    }

    unsigned int present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, state->surface.surface, &present_mode_count, NULL);

    if (present_mode_count == 0) {
        return false;
    }

    return true;
}

void pick_physical_device(application_state* state)
{
    state->device.physical_device = VK_NULL_HANDLE;

    unsigned int device_count = 0;
    vkEnumeratePhysicalDevices(state->instance, &device_count, NULL);
    VkPhysicalDevice* devices = (VkPhysicalDevice*)calloc(device_count, sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(state->instance, &device_count, devices);

    for (unsigned int i = 0; i < device_count; ++i) {
        if (physical_device_suitable(devices[i], state)) {
            state->device.physical_device = devices[i];
            VkPhysicalDeviceProperties properties;
            vkGetPhysicalDeviceProperties(state->device.physical_device, &properties);
            printf("physical device: %s\n", properties.deviceName);
            break;
        }
    }

    free(devices);

    if (state->device.physical_device == VK_NULL_HANDLE) {
        fprintf(stderr, "no suitable physical device found\n");
        return;
    }

    unsigned int queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(state->device.physical_device, &queue_family_count, NULL);
    VkQueueFamilyProperties* queue_families = (VkQueueFamilyProperties*)calloc(queue_family_count, sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(state->device.physical_device, &queue_family_count, queue_families);

    bool graphics_queue_found = true;
    bool present_queue_found = true;

    for (unsigned int i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            state->device.graphics_queue.index = i;
            graphics_queue_found = true;
        }

        VkBool32 present_supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(state->device.physical_device, i, state->surface.surface, &present_supported);
        if (present_supported == VK_TRUE) {
            state->device.present_queue.index = i;
            present_queue_found = true;
        }

        if (graphics_queue_found && present_queue_found) {
            break;
        }
    }

    free(queue_families);
}

void create_device(application_state* state)
{
    float queue_priority[] = {1.0f};

    unsigned int queue_count = state->device.graphics_queue.index == state->device.present_queue.index ? 1 : 2;

    VkDeviceQueueCreateInfo* queue_infos = (VkDeviceQueueCreateInfo*)calloc(queue_count, sizeof(VkDeviceQueueCreateInfo));
    queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_infos[0].pNext = NULL;
    queue_infos[0].flags = 0;
    queue_infos[0].queueFamilyIndex = state->device.graphics_queue.index;
    queue_infos[0].queueCount = 1;
    queue_infos[0].pQueuePriorities = queue_priority;

    if (queue_count == 2) {
        queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_infos[1].pNext = NULL;
        queue_infos[1].flags = 0;
        queue_infos[1].queueFamilyIndex = state->device.present_queue.index;
        queue_infos[1].queueCount = 1;
        queue_infos[1].pQueuePriorities = queue_priority;
    }

    VkPhysicalDeviceFeatures features;
    features.robustBufferAccess = VK_FALSE;
    features.fullDrawIndexUint32 = VK_FALSE;
    features.imageCubeArray = VK_FALSE;
    features.independentBlend = VK_FALSE;
    features.geometryShader = VK_FALSE;
    features.tessellationShader = VK_FALSE;
    features.sampleRateShading = VK_FALSE;
    features.dualSrcBlend = VK_FALSE;
    features.logicOp = VK_FALSE;
    features.multiDrawIndirect = VK_FALSE;
    features.drawIndirectFirstInstance = VK_FALSE;
    features.depthClamp = VK_FALSE;
    features.depthBiasClamp = VK_FALSE;
    features.fillModeNonSolid = VK_FALSE;
    features.depthBounds = VK_FALSE;
    features.wideLines = VK_FALSE;
    features.largePoints = VK_FALSE;
    features.alphaToOne = VK_FALSE;
    features.multiViewport = VK_FALSE;
    features.samplerAnisotropy = VK_FALSE;
    features.textureCompressionETC2 = VK_FALSE;
    features.textureCompressionASTC_LDR = VK_FALSE;
    features.textureCompressionBC = VK_FALSE;
    features.occlusionQueryPrecise = VK_FALSE;
    features.pipelineStatisticsQuery = VK_FALSE;
    features.vertexPipelineStoresAndAtomics = VK_FALSE;
    features.fragmentStoresAndAtomics = VK_FALSE;
    features.shaderTessellationAndGeometryPointSize = VK_FALSE;
    features.shaderImageGatherExtended = VK_FALSE;
    features.shaderStorageImageExtendedFormats = VK_FALSE;
    features.shaderStorageImageMultisample = VK_FALSE;
    features.shaderStorageImageReadWithoutFormat = VK_FALSE;
    features.shaderStorageImageWriteWithoutFormat = VK_FALSE;
    features.shaderUniformBufferArrayDynamicIndexing = VK_FALSE;
    features.shaderSampledImageArrayDynamicIndexing = VK_FALSE;
    features.shaderStorageBufferArrayDynamicIndexing = VK_FALSE;
    features.shaderStorageImageArrayDynamicIndexing = VK_FALSE;
    features.shaderClipDistance = VK_FALSE;
    features.shaderCullDistance = VK_FALSE;
    features.shaderFloat64 = VK_FALSE;
    features.shaderInt64 = VK_FALSE;
    features.shaderInt16 = VK_FALSE;
    features.shaderResourceResidency = VK_FALSE;
    features.shaderResourceMinLod = VK_FALSE;
    features.sparseBinding = VK_FALSE;
    features.sparseResidencyBuffer = VK_FALSE;
    features.sparseResidencyImage2D = VK_FALSE;
    features.sparseResidencyImage3D = VK_FALSE;
    features.sparseResidency2Samples = VK_FALSE;
    features.sparseResidency4Samples = VK_FALSE;
    features.sparseResidency8Samples = VK_FALSE;
    features.sparseResidency16Samples = VK_FALSE;
    features.sparseResidencyAliased = VK_FALSE;
    features.variableMultisampleRate = VK_FALSE;
    features.inheritedQueries = VK_FALSE;

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME
    };

    VkDeviceCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.queueCreateInfoCount = queue_count;
    create_info.pQueueCreateInfos = queue_infos;
    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = NULL;
    create_info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
    create_info.ppEnabledExtensionNames = extensions;
    create_info.pEnabledFeatures = &features;

    if (vkCreateDevice(state->device.physical_device, &create_info, NULL, &state->device.device) != VK_SUCCESS) {
        fprintf(stderr, "failed to create device\n");
    }

    volkLoadDevice(state->device.device);

    vkGetDeviceQueue(state->device.device, state->device.graphics_queue.index, 0, &state->device.graphics_queue.queue);
    vkGetDeviceQueue(state->device.device, state->device.present_queue.index, 0, &state->device.present_queue.queue);
}

void query_surface_capabilities(application_state* state)
{
    unsigned int format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(state->device.physical_device, state->surface.surface, &format_count, NULL);
    VkSurfaceFormatKHR* surface_formats = (VkSurfaceFormatKHR*)calloc(format_count, sizeof(VkSurfaceFormatKHR));
    vkGetPhysicalDeviceSurfaceFormatsKHR(state->device.physical_device, state->surface.surface, &format_count, surface_formats);

    bool preferred_format = false;
    for (unsigned int i = 0; i < format_count; ++i) {
        if (surface_formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            state->surface.format = surface_formats[i].format;
            state->surface.color_space = surface_formats[i].colorSpace;
            preferred_format = true;
            break;
        }
    }

    if (!preferred_format) {
        state->surface.format = surface_formats[0].format;
        state->surface.color_space = surface_formats[0].colorSpace;
    }

    free(surface_formats);

    unsigned int present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(state->device.physical_device, state->surface.surface, &present_mode_count, NULL);
    VkPresentModeKHR* present_modes = (VkPresentModeKHR*)calloc(present_mode_count, sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(state->device.physical_device, state->surface.surface, &present_mode_count, present_modes);

    bool preferred_present_mode = false;
    for (unsigned int i = 0; i < present_mode_count; ++i) {
        if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            state->surface.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
            preferred_present_mode = true;
            break;
        }
    }

    if (!preferred_present_mode) {
        state->surface.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    }

    free(present_modes);

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state->device.physical_device, state->surface.surface, &state->surface.capabilities);

    if (state->surface.capabilities.currentExtent.width != UINT32_MAX) {
        state->surface.extent = state->surface.capabilities.currentExtent;
    } else {
        unsigned int width;
        unsigned int height;

        glfwGetFramebufferSize(state->window, (int*)&width, (int*)&height);

        width = width < state->surface.capabilities.minImageExtent.width ? state->surface.capabilities.minImageExtent.width : width;
        width = width > state->surface.capabilities.maxImageExtent.width ? state->surface.capabilities.maxImageExtent.width : width;

        height = height < state->surface.capabilities.minImageExtent.height ? state->surface.capabilities.minImageExtent.height : height;
        height = height > state->surface.capabilities.maxImageExtent.height ? state->surface.capabilities.maxImageExtent.height : height;

        state->surface.extent = (VkExtent2D){ width, height };
    }
}

void create_swapchain(application_state* state)
{
    query_surface_capabilities(state);

    unsigned int image_count = state->surface.capabilities.minImageCount + 1;
    if (state->surface.capabilities.maxImageCount > 0 && image_count > state->surface.capabilities.maxImageCount) {
        image_count = state->surface.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info;
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.surface = state->surface.surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = state->surface.format;
    create_info.imageColorSpace = state->surface.color_space;
    create_info.imageExtent = state->surface.extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (state->device.graphics_queue.index != state->device.present_queue.index) {
        unsigned int indices[] = {
            state->device.graphics_queue.index,
            state->device.present_queue.index
        };

        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = sizeof(indices) / sizeof(indices[0]);
        create_info.pQueueFamilyIndices = indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = NULL;
    }

    create_info.preTransform = state->surface.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = state->surface.present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(state->device.device, &create_info, NULL, &state->swapchain.swapchain) != VK_SUCCESS) {
        fprintf(stderr, "failed to create swapchain\n");
    }

    vkGetSwapchainImagesKHR(state->device.device, state->swapchain.swapchain, &image_count, NULL);
    state->swapchain.images = (VkImage*)realloc(state->swapchain.images, sizeof(VkImage) * image_count);
    vkGetSwapchainImagesKHR(state->device.device, state->swapchain.swapchain, &image_count, state->swapchain.images);

    state->swapchain.image_views = (VkImageView*)realloc(state->swapchain.image_views, sizeof(VkImageView) * image_count);
    for (unsigned int i = 0; i < image_count; ++i) {
        VkImageViewCreateInfo image_view_info;
        image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        image_view_info.pNext = NULL;
        image_view_info.flags = 0;
        image_view_info.image = state->swapchain.images[i];
        image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        image_view_info.format = state->surface.format;
        image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        image_view_info.subresourceRange.baseMipLevel = 0;
        image_view_info.subresourceRange.levelCount = 1;
        image_view_info.subresourceRange.baseArrayLayer = 0;
        image_view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(state->device.device, &image_view_info, NULL, &state->swapchain.image_views[i]) != VK_SUCCESS) {
            fprintf(stderr, "failed to create image view, index %u\n", i);
        }
    }

    state->swapchain.image_count = image_count;
}

VkShaderModule compile_shader_file(const char* filepath, application_state* state)
{
    FILE* f;
    fopen_s(&f, filepath, "rb");

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* content = malloc(fsize + 1);
    fread(content, fsize, 1, f);
    fclose(f);

    content[fsize] = 0;

    VkShaderModuleCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.codeSize = fsize;
    create_info.pCode = (const unsigned int*)content;

    VkShaderModule module;
    if (vkCreateShaderModule(state->device.device, &create_info, NULL, &module) != VK_SUCCESS) {
        fprintf(stderr, "failed to create shader module from file %s\n", filepath);
        return VK_NULL_HANDLE;
    }

    free(content);

    return module;
}

void create_render_pass(application_state* state)
{
    VkAttachmentDescription color_attachment;
    color_attachment.flags = 0;
    color_attachment.format = state->surface.format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference;
    color_attachment_reference.attachment = 0;
    color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass;
    subpass.flags = 0;
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.inputAttachmentCount = 0;
    subpass.pInputAttachments = NULL;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;
    subpass.pResolveAttachments = NULL;
    subpass.pDepthStencilAttachment = NULL;
    subpass.preserveAttachmentCount = 0;
    subpass.pPreserveAttachments = NULL;

    VkSubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dependencyFlags = 0;

    VkRenderPassCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &color_attachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;
    create_info.dependencyCount = 1;
    create_info.pDependencies = &dependency;

    if (vkCreateRenderPass(state->device.device, &create_info, NULL, &state->render_pass) != VK_SUCCESS) {
        fprintf(stderr, "failed to create render pass\n");
    }
}

void create_framebuffers(application_state* state)
{
    state->framebuffers = (VkFramebuffer*)realloc(state->framebuffers, sizeof(VkFramebuffer) * state->swapchain.image_count);

    for (unsigned int i = 0; i < state->swapchain.image_count; ++i) {
        VkFramebufferCreateInfo create_info;
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.pNext = NULL;
        create_info.flags = 0;
        create_info.renderPass = state->render_pass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = &state->swapchain.image_views[i];
        create_info.width = state->surface.extent.width;
        create_info.height = state->surface.extent.height;
        create_info.layers = 1;

        if (vkCreateFramebuffer(state->device.device, &create_info, NULL, &state->framebuffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "failed to create framebuffer, index %u\n", i);
        }
    }
}

void create_graphics_pipeline(application_state* state)
{
    VkShaderModule vert_module = compile_shader_file("shaders/triangle_buffer.vert.spv", state);
    VkShaderModule frag_module = compile_shader_file("shaders/triangle_buffer.frag.spv", state);

    VkPipelineShaderStageCreateInfo shader_stages[2];
    shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[0].pNext = NULL;
    shader_stages[0].flags = 0;
    shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stages[0].module = vert_module;
    shader_stages[0].pName = "main";
    shader_stages[0].pSpecializationInfo = NULL;

    shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stages[1].pNext = NULL;
    shader_stages[1].flags = 0;
    shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shader_stages[1].module = frag_module;
    shader_stages[1].pName = "main";
    shader_stages[1].pSpecializationInfo = NULL;

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info;
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.pNext = NULL;
    dynamic_state_info.flags = 0;
    dynamic_state_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(dynamic_states[0]);
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkVertexInputAttributeDescription vertex_attributes_description[2];
    vertex_attributes_description[0].location = 0;
    vertex_attributes_description[0].binding = 0;
    vertex_attributes_description[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertex_attributes_description[0].offset = offsetof(vertex, position);

    vertex_attributes_description[1].location = 1;
    vertex_attributes_description[1].binding = 0;
    vertex_attributes_description[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertex_attributes_description[1].offset = offsetof(vertex, color);

    VkVertexInputBindingDescription vertex_binding_description;
    vertex_binding_description.binding = 0;
    vertex_binding_description.stride = sizeof(vertex);
    vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertex_input_info;
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.pNext = NULL;
    vertex_input_info.flags = 0;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &vertex_binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = sizeof(vertex_attributes_description) / sizeof(vertex_attributes_description[0]);
    vertex_input_info.pVertexAttributeDescriptions = vertex_attributes_description;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info;
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.pNext = NULL;
    input_assembly_info.flags = 0;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = state->surface.extent.width;
    viewport.height = state->surface.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor;
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = state->surface.extent;

    VkPipelineViewportStateCreateInfo viewport_info;
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.pNext = NULL;
    viewport_info.flags = 0;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_info;
    rasterization_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_info.pNext = NULL;
    rasterization_info.flags = 0;
    rasterization_info.depthClampEnable = VK_FALSE;
    rasterization_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_info.depthBiasEnable = VK_FALSE;
    rasterization_info.depthBiasConstantFactor = 0.0f;
    rasterization_info.depthBiasClamp = 0.0f;
    rasterization_info.depthBiasSlopeFactor = 0.0f;
    rasterization_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample_info;
    multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_info.pNext = NULL;
    multisample_info.flags = 0;
    multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_info.sampleShadingEnable = VK_FALSE;
    multisample_info.minSampleShading = 1.0f;
    multisample_info.pSampleMask = NULL;
    multisample_info.alphaToCoverageEnable = VK_FALSE;
    multisample_info.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_info;
    color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.pNext = NULL;
    color_blend_info.flags = 0;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;
    color_blend_info.blendConstants[0] = 0.0f;
    color_blend_info.blendConstants[1] = 0.0f;
    color_blend_info.blendConstants[2] = 0.0f;
    color_blend_info.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo layout_info;
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pNext = NULL;
    layout_info.flags = 0;
    layout_info.setLayoutCount = 0;
    layout_info.pSetLayouts = NULL;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = NULL;

    if (vkCreatePipelineLayout(state->device.device, &layout_info, NULL, &state->graphics_pipeline.layout) != VK_SUCCESS) {
        fprintf(stderr, "failed to create pipeline layout\n");
    }

    VkGraphicsPipelineCreateInfo pipeline_info;
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.pNext = NULL;
    pipeline_info.flags = 0;
    pipeline_info.stageCount = sizeof(shader_stages) / sizeof(shader_stages[0]);
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pTessellationState = NULL;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterization_info;
    pipeline_info.pMultisampleState = &multisample_info;
    pipeline_info.pDepthStencilState = NULL;
    pipeline_info.pColorBlendState = &color_blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = state->graphics_pipeline.layout;
    pipeline_info.renderPass = state->render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(state->device.device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &state->graphics_pipeline.pipeline) != VK_SUCCESS) {
        fprintf(stderr, "failed to create graphics pipeline\n");
    }

    vkDestroyShaderModule(state->device.device, frag_module, NULL);
    vkDestroyShaderModule(state->device.device, vert_module, NULL);
}

void create_command_pool(application_state* state)
{
    VkCommandPoolCreateInfo pool_info;
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.pNext = NULL;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = state->device.graphics_queue.index;

    if (vkCreateCommandPool(state->device.device, &pool_info, NULL, &state->command_pool) != VK_SUCCESS) {
        fprintf(stderr, "failed to create command pool\n");
    }
}

void allocate_command_buffer(application_state* state)
{
    state->command_buffers = (VkCommandBuffer*)calloc(MAX_FRAMES_IN_FLIGHT, sizeof(VkCommandBuffer));

    VkCommandBufferAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.pNext = NULL;
    allocate_info.commandPool = state->command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    if (vkAllocateCommandBuffers(state->device.device, &allocate_info, state->command_buffers) != VK_SUCCESS) {
        fprintf(stderr, "failed to allocate command buffer\n");
    }
}

void create_sync_objects(application_state* state)
{
    state->image_available_semaphores = (VkSemaphore*)calloc(MAX_FRAMES_IN_FLIGHT, sizeof(VkSemaphore));
    state->render_finished_semaphores = (VkSemaphore*)calloc(MAX_FRAMES_IN_FLIGHT, sizeof(VkSemaphore));
    state->in_flight_fences = (VkFence*)calloc(MAX_FRAMES_IN_FLIGHT, sizeof(VkFence));

    VkSemaphoreCreateInfo semaphore_info;
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphore_info.pNext = NULL;
    semaphore_info.flags = 0;

    VkFenceCreateInfo fence_info;
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.pNext = NULL;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(state->device.device, &semaphore_info, NULL, &state->image_available_semaphores[i]);
        vkCreateSemaphore(state->device.device, &semaphore_info, NULL, &state->render_finished_semaphores[i]);
        vkCreateFence(state->device.device, &fence_info, NULL, &state->in_flight_fences[i]);
    }
}

void record_command_buffer(VkCommandBuffer command_buffer, unsigned int index, application_state* state)
{
    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = NULL;

    if (vkBeginCommandBuffer(command_buffer, &begin_info) != VK_SUCCESS) {
        fprintf(stderr, "failed to begin recording command buffer\n");
    }

    VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo render_pass_info;
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.pNext = NULL;
    render_pass_info.renderPass = state->render_pass;
    render_pass_info.framebuffer = state->framebuffers[index];
    render_pass_info.renderArea.offset = (VkOffset2D){0, 0};
    render_pass_info.renderArea.extent = state->surface.extent;
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state->graphics_pipeline.pipeline);

    VkBuffer vertex_buffers[] = {state->vertex_buffer};
    VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    VkViewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = state->surface.extent.width;
    viewport.height = state->surface.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor;
    scissor.offset = (VkOffset2D){0, 0};
    scissor.extent = state->surface.extent;

    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS) {
        fprintf(stderr, "failed to record command buffer\n");
    }
}

void draw_frame(application_state* state)
{
    vkWaitForFences(state->device.device, 1, &state->in_flight_fences[state->current_frame], VK_TRUE, UINT64_MAX);

    unsigned int image_index;
    VkResult result = vkAcquireNextImageKHR(state->device.device, state->swapchain.swapchain, UINT64_MAX, state->image_available_semaphores[state->current_frame], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain(state);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "failed to acquire swapchain image\n");
    }

    vkResetFences(state->device.device, 1, &state->in_flight_fences[state->current_frame]);

    vkResetCommandBuffer(state->command_buffers[state->current_frame], 0);
    record_command_buffer(state->command_buffers[state->current_frame], image_index, state);

    VkSemaphore wait_semaphores[] = {state->image_available_semaphores[state->current_frame]};
    VkSemaphore signal_semaphores[] = {state->render_finished_semaphores[state->current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = sizeof(wait_semaphores) / sizeof(wait_semaphores[0]);
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &state->command_buffers[state->current_frame];
    submit_info.signalSemaphoreCount = sizeof(signal_semaphores) / sizeof(signal_semaphores[0]);
    submit_info.pSignalSemaphores = signal_semaphores;

    if (vkQueueSubmit(state->device.graphics_queue.queue, 1, &submit_info, state->in_flight_fences[state->current_frame]) != VK_SUCCESS) {
        fprintf(stderr, "failed to submit draw command buffer\n");
    }

    VkSwapchainKHR swapchains[] = {state->swapchain.swapchain};

    VkPresentInfoKHR present_info;
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pNext = NULL;
    present_info.waitSemaphoreCount = sizeof(signal_semaphores) / sizeof(signal_semaphores[0]);
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = sizeof(swapchains) / sizeof(swapchains[0]);
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = NULL;

    result = vkQueuePresentKHR(state->device.present_queue.queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || state->framebuffer_resized) {
        state->framebuffer_resized = false;
        recreate_swapchain(state);
    } else if (result != VK_SUCCESS) {
        fprintf(stderr, "failed to present swapchain image\n");
    }

    state->current_frame = (state->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void recreate_swapchain(application_state* state)
{
    vkDeviceWaitIdle(state->device.device);

    vkDestroyPipeline(state->device.device, state->graphics_pipeline.pipeline, NULL);
    vkDestroyPipelineLayout(state->device.device, state->graphics_pipeline.layout, NULL);

    for (unsigned int i = 0; i < state->swapchain.image_count; ++i) {
        vkDestroyFramebuffer(state->device.device, state->framebuffers[i], NULL);
    }

    vkDestroyRenderPass(state->device.device, state->render_pass, NULL);

    for (unsigned int i = 0; i < state->swapchain.image_count; ++i) {
        vkDestroyImageView(state->device.device, state->swapchain.image_views[i], NULL);
    }

    vkDestroySwapchainKHR(state->device.device, state->swapchain.swapchain, NULL);

    create_swapchain(state);
    create_render_pass(state);
    create_graphics_pipeline(state);
    create_framebuffers(state);
}

unsigned int find_memory_type(application_state* state, unsigned int type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(state->device.physical_device, &memory_properties);

    for (unsigned int i = 0; i < memory_properties.memoryTypeCount; ++i) {
        if (type_filter & (1 << i) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    fprintf(stderr, "failed to find suitable memory type\n");

    return -1;
}

void create_buffer(application_state* state, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* buffer_memory)
{
    VkBufferCreateInfo create_info;
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.pNext = NULL;
    create_info.flags = 0;
    create_info.size = size;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = NULL;

    if (vkCreateBuffer(state->device.device, &create_info, NULL, buffer) != VK_SUCCESS) {
        fprintf(stderr, "failed to create buffer\n");
        return;
    }

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(state->device.device, *buffer, &memory_requirements);

    VkMemoryAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = NULL;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(state, memory_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(state->device.device, &allocate_info, NULL, buffer_memory) != VK_SUCCESS) {
        fprintf(stderr, "failed to allocate buffer memory\n");
        return;
    }

    vkBindBufferMemory(state->device.device, *buffer, *buffer_memory, 0);
}

void copy_buffer(application_state* state, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocate_info;
    allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocate_info.pNext = NULL;
    allocate_info.commandPool = state->command_pool;
    allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(state->device.device, &allocate_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info;
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.pNext = NULL;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    begin_info.pInheritanceInfo = NULL;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region;
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;

    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info;
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.pNext = NULL;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = NULL;
    submit_info.pWaitDstStageMask = NULL;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 0;
    submit_info.pSignalSemaphores = NULL;

    vkQueueSubmit(state->device.graphics_queue.queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(state->device.graphics_queue.queue);

    vkFreeCommandBuffers(state->device.device, state->command_pool, 1, &command_buffer);
}

void create_vertex_buffer(application_state* state)
{
    vertex vertices[] = {
        {{ 0.0f, -0.5f}, {0.8f, 0.2f, 0.2f}},
        {{ 0.5f,  0.5f}, {0.2f, 0.8f, 0.2f}},
        {{-0.5f,  0.5f}, {0.2f, 0.2f, 0.8f}}
    };

    VkDeviceSize vertices_size = sizeof(vertices[0]) * (sizeof(vertices) / sizeof(vertices[0]));

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    create_buffer(state, vertices_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer, &staging_buffer_memory);

    void* data;
    vkMapMemory(state->device.device, staging_buffer_memory, 0, vertices_size, 0, &data);
    memcpy(data, vertices, vertices_size);
    vkUnmapMemory(state->device.device, staging_buffer_memory);

    create_buffer(state, vertices_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &state->vertex_buffer, &state->vertex_buffer_memory);

    copy_buffer(state, staging_buffer, state->vertex_buffer, vertices_size);

    vkDestroyBuffer(state->device.device, staging_buffer, NULL);
    vkFreeMemory(state->device.device, staging_buffer_memory, NULL);
}

int main(void)
{
    application_state* state = (application_state*)malloc(sizeof(application_state));
    state = memset(state, 0, sizeof(application_state));

    initialize_window(state);

    if (volkInitialize() != VK_SUCCESS) {
        fprintf(stderr, "failed to initialize vulkan loader\n");
        return -1;
    }

    create_instance(state);
    create_surface(state);
    pick_physical_device(state);
    create_device(state);
    create_swapchain(state);
    create_render_pass(state);
    create_framebuffers(state);
    create_graphics_pipeline(state);
    create_command_pool(state);
    allocate_command_buffer(state);
    create_sync_objects(state);
    create_vertex_buffer(state);

    while (!glfwWindowShouldClose(state->window)) {
        glfwPollEvents();
        draw_frame(state);
    }

    vkDeviceWaitIdle(state->device.device);

    vkDestroyBuffer(state->device.device, state->vertex_buffer, NULL);
    vkFreeMemory(state->device.device, state->vertex_buffer_memory, NULL);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyFence(state->device.device, state->in_flight_fences[i], NULL);
        vkDestroySemaphore(state->device.device, state->render_finished_semaphores[i], NULL);
        vkDestroySemaphore(state->device.device, state->image_available_semaphores[i], NULL);
    }
    free(state->in_flight_fences);
    free(state->render_finished_semaphores);
    free(state->image_available_semaphores);

    vkDestroyCommandPool(state->device.device, state->command_pool, NULL);
    free(state->command_buffers);

    vkDestroyPipeline(state->device.device, state->graphics_pipeline.pipeline, NULL);
    vkDestroyPipelineLayout(state->device.device, state->graphics_pipeline.layout, NULL);

    for (unsigned int i = 0; i < state->swapchain.image_count; ++i) {
        vkDestroyFramebuffer(state->device.device, state->framebuffers[i], NULL);
    }
    free(state->framebuffers);

    vkDestroyRenderPass(state->device.device, state->render_pass, NULL);

    for (unsigned int i = 0; i < state->swapchain.image_count; ++i) {
        vkDestroyImageView(state->device.device, state->swapchain.image_views[i], NULL);
    }
    vkDestroySwapchainKHR(state->device.device, state->swapchain.swapchain, NULL);
    free(state->swapchain.image_views);
    free(state->swapchain.images);

    vkDestroyDevice(state->device.device, NULL);

    vkDestroySurfaceKHR(state->instance, state->surface.surface, NULL);

#ifndef NDEBUG
    vkDestroyDebugUtilsMessengerEXT(state->instance, state->debug_messenger, NULL);
#endif
    vkDestroyInstance(state->instance, NULL);

    glfwDestroyWindow(state->window);
    glfwTerminate();

    free(state);

    return 0;
}
