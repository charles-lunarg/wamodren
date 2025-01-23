#include "renderer.hpp"

#include <fstream>
#include <cstdlib>

#include <fmt/format.h>
#include <magic_enum.hpp>

static constexpr VkImageSubresourceRange single_color_image_subresource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

static const std::vector<const char *> required_device_extensions{
    "VK_KHR_swapchain", "VK_KHR_maintenance5"};

VkResult init_instance(init_settings &settings, renderer &rend)
{
    uint32_t glfw_extension_count = 0;
    const char **required_glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::vector<const char *> required_instance_extensions;
    for (uint32_t i = 0; i < glfw_extension_count; i++)
    {
        required_instance_extensions.push_back(required_glfw_extensions[i]);
    }
    VkApplicationInfo app_create_info{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "WaModRen",
        .apiVersion = VK_API_VERSION_1_4,
    };

    VkInstanceCreateInfo inst_create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_create_info,
        .enabledExtensionCount = static_cast<uint32_t>(required_instance_extensions.size()),
        .ppEnabledExtensionNames = required_instance_extensions.data(),
    };

    VkResult err = VK_SUCCESS;
    err = vkCreateInstance(&inst_create_info, nullptr, &rend.inst);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create instance with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    VkSurfaceKHR surface{};
    err = glfwCreateWindowSurface(rend.inst, rend.glfw_window, NULL, &rend.surface);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create VkSurfaceKHR with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult validate_physical_device(renderer &rend, VkPhysicalDevice physical_device)
{

    VkPhysicalDeviceProperties physical_device_properties{};
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
    uint32_t version = physical_device_properties.apiVersion;
    fmt::print("Using physical device {} with api version {}.{}.{}\n", physical_device_properties.deviceName,
               VK_API_VERSION_MAJOR(version), VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));

    if (!glfwGetPhysicalDevicePresentationSupport(rend.inst, physical_device, 0))
    {
        fmt::print("Physical Device does not support presentation");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    uint32_t queue_count = 1;
    VkQueueFamilyProperties queue_family_properties{};
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_count, &queue_family_properties);
    if ((queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
        (queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 ||
        (queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT) == 0)
    {
        fmt::print("VkPhysicalDevice's first queue doesn't support all required queue operations of graphics, compute, and transfer");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::vector<VkExtensionProperties> available_device_extensions{};
    uint32_t device_extension_count = 0;
    VkResult err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get count of VkPhysicalDevice's extensions with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    available_device_extensions.resize(device_extension_count, VkExtensionProperties{});
    err = vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &device_extension_count, available_device_extensions.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get VkPhysicalDevice's extensions with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (const auto &required_ext : required_device_extensions)
    {
        bool found = false;
        for (const auto &avail_ext : available_device_extensions)
        {
            if (strcmp(required_ext, avail_ext.extensionName) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            fmt::print("Required device extensions {} is not in the list of available device extensions", required_ext);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    VkPhysicalDeviceMaintenance5FeaturesKHR available_features_maintenance5{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR,
    };
    VkPhysicalDeviceVulkan13Features available_features_1_3{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &available_features_maintenance5,
    };
    VkPhysicalDeviceVulkan12Features available_features_1_2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &available_features_1_3,
    };
    VkPhysicalDeviceVulkan11Features available_features_1_1{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .pNext = &available_features_1_2,
    };
    VkPhysicalDeviceFeatures2 available_physical_device_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &available_features_1_1,
    };
    vkGetPhysicalDeviceFeatures2(physical_device, &available_physical_device_features);

    // Required Physical Device Features across all versions

    if (
        available_features_1_2.timelineSemaphore != VK_TRUE ||
        available_features_1_2.bufferDeviceAddress != VK_TRUE ||
        available_features_1_2.descriptorIndexing != VK_TRUE ||
        available_features_1_2.uniformBufferStandardLayout != VK_TRUE ||
        available_features_1_2.descriptorBindingPartiallyBound != VK_TRUE ||

        available_features_1_3.dynamicRendering != VK_TRUE ||
        available_features_1_3.maintenance4 != VK_TRUE ||
        available_features_1_3.synchronization2 != VK_TRUE ||

        available_features_maintenance5.maintenance5 != VK_TRUE)
    {
        fmt::print("Missing required physical device features!");
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, rend.surface, &rend.surface_capabilities);
    if (err != VK_SUCCESS)
    {
        fmt::print("Unable to get physical device surface capabilities with err {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    return VK_SUCCESS;
}

VkResult choose_physical_device(init_settings &settings, renderer &rend)
{

    uint32_t physical_device_count = 0;
    VkResult err = vkEnumeratePhysicalDevices(rend.inst, &physical_device_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to enumerate physical device count with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkPhysicalDevice> physical_devices(physical_device_count, {});
    err = vkEnumeratePhysicalDevices(rend.inst, &physical_device_count, physical_devices.data());
    if (err != VK_SUCCESS || physical_device_count != physical_devices.size())
    {
        fmt::print("Failed to enumerate physical devices with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    for (auto const &physical_device : physical_devices)
    {
        if (validate_physical_device(rend, physical_device) == VK_SUCCESS)
        {
            rend.physical_device = physical_device;
            break;
        }
    }
    if (rend.physical_device == VK_NULL_HANDLE)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult init_device(init_settings &settings, renderer &rend)
{

    VkPhysicalDeviceMaintenance5FeaturesKHR enabled_features_maintenance5{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_5_FEATURES_KHR,
        .maintenance5 = VK_TRUE,
    };

    VkPhysicalDeviceVulkan13Features enabled_features_1_3{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabled_features_maintenance5,
        .synchronization2 = VK_TRUE,
        .dynamicRendering = VK_TRUE,
        .maintenance4 = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features enabled_features_1_2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &enabled_features_1_3,
        .descriptorIndexing = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .uniformBufferStandardLayout = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    VkPhysicalDeviceVulkan11Features enabled_features_1_1{
        enabled_features_1_1.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        enabled_features_1_1.pNext = &enabled_features_1_2,
    };

    VkPhysicalDeviceFeatures2 enabled_physical_device_features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &enabled_features_1_1,
    };

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = 0,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };

    VkDeviceCreateInfo device_create_info{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabled_physical_device_features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_infos,
        .enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size()),
        .ppEnabledExtensionNames = required_device_extensions.data(),
    };
    VkResult err = vkCreateDevice(rend.physical_device, &device_create_info, nullptr, &rend.device);
    if (err != VK_SUCCESS)
    {
        fmt::print("Unable to create device with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    vkGetDeviceQueue(rend.device, 0, 0, &rend.main_queue);
    return VK_SUCCESS;
}
VkResult init_swapchain(init_settings &settings, renderer &rend)
{

    uint32_t surface_format_count = 0;
    std::vector<VkSurfaceFormatKHR> surface_formats{};
    VkResult err = vkGetPhysicalDeviceSurfaceFormatsKHR(rend.physical_device, rend.surface, &surface_format_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get count of VkSurfaceFormatKHRs with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    surface_formats.resize(surface_format_count, VkSurfaceFormatKHR{});
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(rend.physical_device, rend.surface, &surface_format_count, surface_formats.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get VkSurfaceFormatKHRs with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    rend.swapchain_image_format = VK_FORMAT_R8G8B8A8_SRGB;
    bool swapchain_image_format_found = false;
    for (const auto &format : surface_formats)
    {
        if (rend.swapchain_image_format == format.format)
        {
            swapchain_image_format_found = true;
            rend.swapchain_image_colorspace = format.colorSpace;
            break;
        }
    }
    if (!swapchain_image_format_found)
    {
        rend.swapchain_image_format = VK_FORMAT_B8G8R8A8_SRGB;
        for (const auto &format : surface_formats)
        {
            if (rend.swapchain_image_format == format.format)
            {
                swapchain_image_format_found = true;
                rend.swapchain_image_colorspace = format.colorSpace;
                break;
            }
        }
        if (!swapchain_image_format_found)
        {
            fmt::print("Cannot find suitable swapchain image format");
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = rend.surface,
        .minImageCount = (rend.surface_capabilities.minImageCount > 3) ? rend.surface_capabilities.minImageCount : 3,
        .imageFormat = rend.swapchain_image_format,
        .imageColorSpace = rend.swapchain_image_colorspace,
        .imageExtent = VkExtent2D{static_cast<uint32_t>(settings.window_width), static_cast<uint32_t>(settings.window_height)},
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = false,
    };
    err = vkCreateSwapchainKHR(rend.device, &swapchain_create_info, nullptr, &rend.swapchain);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create swapchain with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    rend.swapchain_image_render_area.extent = {static_cast<uint32_t>(settings.window_width), static_cast<uint32_t>(settings.window_height)};

    uint32_t swapchain_image_count = 0;
    err = vkGetSwapchainImagesKHR(rend.device, rend.swapchain, &swapchain_image_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to get swapchain image count with code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkImage> swapchain_images(swapchain_image_count, VkImage{});
    err = vkGetSwapchainImagesKHR(rend.device, rend.swapchain, &swapchain_image_count, swapchain_images.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to get swapchain image count with code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    rend.swapchain_frames.resize(swapchain_image_count, {});

    for (uint32_t i = 0; i < rend.swapchain_frames.size(); i++)
    {
        rend.swapchain_frames.at(i).image = swapchain_images.at(i);
    }
    for (auto &swapchain_frame : rend.swapchain_frames)
    {

        VkImageViewCreateInfo swapchain_image_view_create_info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain_frame.image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = rend.swapchain_image_format,
            .subresourceRange = single_color_image_subresource_range,
        };

        err = vkCreateImageView(rend.device, &swapchain_image_view_create_info, nullptr, &swapchain_frame.image_view);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create swapchain image view with code {}", magic_enum::enum_name(err));
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}
VkResult init_frame_data(init_settings &settings, renderer &rend)
{

    VkCommandPoolCreateInfo command_buffer_create_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = 0,
    };
    VkResult err = vkCreateCommandPool(rend.device, &command_buffer_create_info, nullptr, &rend.submission_command_pool);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create command pool with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    VkCommandBufferAllocateInfo command_buffer_allocate_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = rend.submission_command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(rend.swapchain_frames.size()),
    };
    std::vector<VkCommandBuffer> command_buffers(rend.swapchain_frames.size(), {});
    err = vkAllocateCommandBuffers(rend.device, &command_buffer_allocate_info, command_buffers.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create command buffers with error code {}", magic_enum::enum_name(err));
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    // Double buffer the work we submit onto the GPU
    rend.submission_frames.resize(2, {});
    rend.submission_frames.at(0).command_buffer = command_buffers.at(0);
    rend.submission_frames.at(1).command_buffer = command_buffers.at(1);
    for (auto &submission_frame : rend.submission_frames)
    {

        VkSemaphoreCreateInfo semaphore_create_info{};
        semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        err = vkCreateSemaphore(rend.device, &semaphore_create_info, nullptr, &submission_frame.acquire_swapchain_semaphore);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create acquire semaphore with code {}", magic_enum::enum_name(err));
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        err = vkCreateSemaphore(rend.device, &semaphore_create_info, nullptr, &submission_frame.present_swapchain_semaphore);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create present semaphore with code {}", magic_enum::enum_name(err));
            return VK_ERROR_INITIALIZATION_FAILED;
        }
        VkFenceCreateInfo fence_create_info{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        err = vkCreateFence(rend.device, &fence_create_info, nullptr, &submission_frame.fence);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create fence with code {}", magic_enum::enum_name(err));
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }
    return VK_SUCCESS;
}

VkResult init_pipeline_layout(renderer &rend)
{
    std::vector<VkDescriptorSetLayoutBinding> bindings{
        VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
        VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_ALL,
        },
    };
    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data(),
    };

    VkResult err = vkCreateDescriptorSetLayout(rend.device, &descriptor_set_layout_create_info, nullptr, &rend.descriptor_set_layout);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create descriptor set layout");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    std::vector<VkPushConstantRange> push_constants{
        VkPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0,
            .size = 4,
        },
    };

    VkPipelineLayoutCreateInfo pipeline_layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &rend.descriptor_set_layout,
        .pushConstantRangeCount = static_cast<uint32_t>(push_constants.size()),
        .pPushConstantRanges = push_constants.data(),
    };

    err = vkCreatePipelineLayout(rend.device, &pipeline_layout_create_info, nullptr, &rend.pipeline_layout);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create pipeline layout");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    return VK_SUCCESS;
}

VkResult create_graphics_pipeline(renderer &rend, pipeline_create_details const &details, VkPipeline &out_pipeline)
{
    std::string path_to_shader_source = DATA_DIRECTORY "/shaders/" + details.shader_name + ".slang";
    std::string output_filename = details.shader_name + ".spv";
    std::ifstream file_check(path_to_shader_source);
    if (!file_check.is_open())
    {
        fmt::print("The file at path {} could not be found", path_to_shader_source);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    file_check.close();

    std::string slang_invocation = "slangc ";
    slang_invocation.append(path_to_shader_source);
    slang_invocation.append(" -target spirv");
    slang_invocation.append(" -I " DATA_DIRECTORY "/shaders");
    slang_invocation.append(" -o ").append(output_filename);

    int err = system(slang_invocation.c_str());
    if (err != 0)
    {
        fmt::print("The slang invocation \"{}\" failed with error code {}", slang_invocation, err);
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    std::ifstream shader_file(output_filename, std::ios::ate | std::ios::binary); // start at end
    if (!shader_file.is_open())
    {
        fmt::print("The compiled shader {} could not be found", path_to_shader_source);
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    size_t file_size = (size_t)shader_file.tellg();
    std::vector<uint32_t> compiled_contents(file_size / sizeof(uint32_t));
    shader_file.seekg(0); // go back to the beginning
    shader_file.read((char *)compiled_contents.data(), file_size);
    shader_file.close();

    switch (details.type)
    {
    case (pipeline_type::graphics):
    {
    }
    case (pipeline_type::compute):
    {
        VkShaderModuleCreateInfo shader_module_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = static_cast<uint32_t>(compiled_contents.size() * sizeof(uint32_t)),
            .pCode = compiled_contents.data(),
        };

        VkComputePipelineCreateInfo compute_pipeline_create_info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .stage = VkPipelineShaderStageCreateInfo{
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = &shader_module_info,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .pName = "main",
            },
            .layout = rend.pipeline_layout,

        };
        VkResult res = vkCreateComputePipelines(rend.device, nullptr, 1, &compute_pipeline_create_info, nullptr, &out_pipeline);
        if (res != VK_SUCCESS)
        {
            return res;
        }
    }
    }

    return VK_SUCCESS;
}

VkResult init_graphics_pipelines(init_settings &settings, renderer &rend)
{
    VkPipeline pipeline{};
    if (create_graphics_pipeline(rend, pipeline_create_details{pipeline_type::compute, "hello-world"}, pipeline) != VK_SUCCESS)
    {
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    rend.pipeline = pipeline;
    return VK_SUCCESS;
}
VkResult render(renderer &rend)
{
    VkResult err = VK_SUCCESS;
    auto &current_submission_frame = rend.submission_frames.at(rend.current_submission_frame_index);
    uint64_t timeout = 1000000000;

    err = vkWaitForFences(rend.device, 1, &current_submission_frame.fence, VK_TRUE, timeout);
    if (err == VK_TIMEOUT)
    {
        fmt::print("vkWaitForFences on submission frame index {} exceeded timeout {}ns", rend.current_submission_frame_index, timeout);
        return VK_TIMEOUT;
    }
    else if (err != VK_SUCCESS)
    {
        fmt::print("vkWaitForFences on submission frame index {} failed with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }

    err = vkResetFences(rend.device, 1, &current_submission_frame.fence);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to reset fence from submission frame index {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }

    uint32_t next_swapchain_image_index = 0;
    err = vkAcquireNextImageKHR(rend.device, rend.swapchain, timeout, current_submission_frame.acquire_swapchain_semaphore, nullptr, &next_swapchain_image_index);
    if (err == VK_TIMEOUT)
    {
        fmt::print("vkAcquireNextImageKHR on submission frame index {} exceeded timeout {}ns", rend.current_submission_frame_index, timeout);
        return err;
    }
    else if (err == VK_SUBOPTIMAL_KHR)
    {
        fmt::print("vkAcquireNextImageKHR reported VK_SUBOPTIMAL_KHR from submission frame index {}", rend.current_submission_frame_index);
        return err;
    }
    else if (err != VK_SUCCESS)
    {
        fmt::print("vkAcquireNextImageKHR from submission frame index {} failed with error code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }

    auto &current_swapchain_frame = rend.swapchain_frames.at(next_swapchain_image_index);
    auto &command_buffer = current_submission_frame.command_buffer;

    err = vkResetCommandBuffer(command_buffer, 0);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to reset command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }
    VkCommandBufferBeginInfo command_buffer_begin_info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    err = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to being command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }

    // Transition swapchain image from UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    VkImageMemoryBarrier2 image_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = current_swapchain_frame.image,
        .subresourceRange = single_color_image_subresource_range,
    };

    VkDependencyInfo dependency_info_undefined_to_color_attach{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_memory_barrier};

    vkCmdPipelineBarrier2(command_buffer, &dependency_info_undefined_to_color_attach);

    VkRenderingAttachmentInfoKHR rendering_attachment_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView = current_swapchain_frame.image_view,
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{0.f, (100 + rend.frame_count) % 128 / 256.f, 0.f, 0.f},
    };

    VkRenderingInfoKHR rendering_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = rend.swapchain_image_render_area,
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &rendering_attachment_info,
    };

    vkCmdBeginRendering(command_buffer, &rendering_info);
    vkCmdEndRendering(command_buffer);

    VkImageMemoryBarrier2 to_present_src_image_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT_KHR,
        .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
        .dstAccessMask = VK_ACCESS_2_NONE,
        .oldLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = 0,
        .dstQueueFamilyIndex = 0,
        .image = current_swapchain_frame.image,
        .subresourceRange = single_color_image_subresource_range,
    };

    VkDependencyInfo dependency_info_color_attach_to_present_src{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &to_present_src_image_memory_barrier,
    };

    vkCmdPipelineBarrier2(command_buffer, &dependency_info_color_attach_to_present_src);

    err = vkEndCommandBuffer(command_buffer);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to end command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_submission_frame.acquire_swapchain_semaphore,
        .pWaitDstStageMask = &wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &current_submission_frame.command_buffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &current_submission_frame.present_swapchain_semaphore,
    };
    err = vkQueueSubmit(rend.main_queue, 1, &submit_info, current_submission_frame.fence);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to submit command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }

    VkPresentInfoKHR present_info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &current_submission_frame.present_swapchain_semaphore,
        .swapchainCount = 1,
        .pSwapchains = &rend.swapchain,
        .pImageIndices = &next_swapchain_image_index,
        .pResults = nullptr,
    };

    vkQueuePresentKHR(rend.main_queue, &present_info);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to present swapchain frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return VK_ERROR_UNKNOWN;
    }
    rend.current_submission_frame_index = (rend.current_submission_frame_index + 1) % 2;

    return VK_SUCCESS;
}

int init_renderer(init_settings &settings, renderer &rend)
{
    if (init_instance(settings, rend) != VK_SUCCESS)
        return -1;
    if (choose_physical_device(settings, rend) != VK_SUCCESS)
        return -1;
    if (init_device(settings, rend) != VK_SUCCESS)
        return -1;
    if (init_swapchain(settings, rend) != VK_SUCCESS)
        return -1;
    if (init_frame_data(settings, rend) != VK_SUCCESS)
        return -1;

    // Check that shader compiler is available
    if (system("slangc") != 0)
    {
        fmt::print("slangc not found!");
        return -1;
    }
    if (init_pipeline_layout(rend) != VK_SUCCESS)
        return -1;
    if (init_graphics_pipelines(settings, rend) != VK_SUCCESS)
        return -1;
    return 0;
}

void shutdown_renderer(renderer &rend)
{
    vkDeviceWaitIdle(rend.device);
    vkDestroyPipeline(rend.device, rend.pipeline, nullptr);
    vkDestroyPipelineLayout(rend.device, rend.pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(rend.device, rend.descriptor_set_layout, nullptr);
    for (auto &swapchain_frame : rend.swapchain_frames)
    {
        vkDestroyImageView(rend.device, swapchain_frame.image_view, nullptr);
    }
    for (auto &submission_frame : rend.submission_frames)
    {
        vkDestroySemaphore(rend.device, submission_frame.acquire_swapchain_semaphore, nullptr);
        vkDestroySemaphore(rend.device, submission_frame.present_swapchain_semaphore, nullptr);
        vkDestroyFence(rend.device, submission_frame.fence, nullptr);
    }
    vkDestroyCommandPool(rend.device, rend.submission_command_pool, nullptr);

    vkDestroySwapchainKHR(rend.device, rend.swapchain, nullptr);
    vkDestroyDevice(rend.device, nullptr);
    vkDestroySurfaceKHR(rend.inst, rend.surface, nullptr);
    vkDestroyInstance(rend.inst, nullptr);
}
