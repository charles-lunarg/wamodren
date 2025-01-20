#include <vector>
#include <string>

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>

void glfw_error_callback(int code, const char *description)
{
    fmt::print("GLFW Error Code {}: {}\n", code, description);
}

void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void window_close_callback(GLFWwindow *window)
{
    fmt::print("Window is closing!");
}

struct init_settings
{
    int window_width = 800;
    int window_height = 600;
};

struct swapchain_frame
{
    VkImage image{};
    VkImageView image_view{};
};
struct submission_frame
{
    VkCommandBuffer command_buffer{};
    VkSemaphore acquire_swapchain_semaphore{};
    VkSemaphore present_swapchain_semaphore{};
    VkFence fence{};
};

struct renderer
{
    GLFWwindow *glfw_window{};
    VkInstance inst{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical_device{};
    VkDevice device{};
    VkQueue main_queue;
    VkSwapchainKHR swapchain{};
    VkFormat swapchain_image_format{};
    VkColorSpaceKHR swapchain_image_colorspace{};
    VkCommandPool submission_command_pool{};
    uint32_t current_swapchain_frame_index = 0;
    std::vector<swapchain_frame> swapchain_frames;

    uint32_t current_submission_frame_index = 0;
    std::vector<submission_frame> submission_frames;
};

static constexpr VkImageSubresourceRange entire_subresource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

int init(init_settings &settings, renderer &rend)
{
    if (!glfwInit())
    {
        fmt::print("Failed to initialize glfw");
        return -1;
    }
    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    rend.glfw_window = glfwCreateWindow(settings.window_width, settings.window_height, "WaModRen", NULL, NULL);
    if (rend.glfw_window == nullptr)
    {
        fmt::print("Failed to create glfw window");
        return -1;
    }
    glfwSetWindowCloseCallback(rend.glfw_window, window_close_callback);
    glfwSetKeyCallback(rend.glfw_window, glfw_key_callback);
    uint32_t glfw_extension_count = 0;
    const char **required_glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::vector<const char *> required_instance_extensions;
    for (uint32_t i = 0; i < glfw_extension_count; i++)
    {
        required_instance_extensions.push_back(required_glfw_extensions[i]);
    }
    VkApplicationInfo app_create_info{};
    app_create_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_create_info.apiVersion = VK_API_VERSION_1_3;
    app_create_info.pApplicationName = "WaModRen";

    VkInstanceCreateInfo inst_create_info{};
    inst_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    inst_create_info.pApplicationInfo = &app_create_info;
    inst_create_info.enabledExtensionCount = static_cast<uint32_t>(required_instance_extensions.size());
    inst_create_info.ppEnabledExtensionNames = required_instance_extensions.data();

    VkResult err = VK_SUCCESS;
    err = vkCreateInstance(&inst_create_info, nullptr, &rend.inst);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create instance with error code {}", magic_enum::enum_name(err));
        return -1;
    }

    VkSurfaceKHR surface;
    err = glfwCreateWindowSurface(rend.inst, rend.glfw_window, NULL, &rend.surface);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create VkSurfaceKHR with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    uint32_t physical_device_count = 1;
    err = vkEnumeratePhysicalDevices(rend.inst, &physical_device_count, &rend.physical_device);
    if (err < VK_SUCCESS)
    {
        fmt::print("Failed to enumerate physical devices with error code {}", magic_enum::enum_name(err));
        return -1;
    }

    uint32_t queue_count = 1;
    VkQueueFamilyProperties queue_family_properties{};
    vkGetPhysicalDeviceQueueFamilyProperties(rend.physical_device, &queue_count, &queue_family_properties);
    if ((queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
        (queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 ||
        (queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT) == 0)
    {
        fmt::print("VkPhysicalDevice's first queue doesn't support all required queue operations of graphics, compute, and transfer");
        return -1;
    }
    std::vector<const char *> required_device_extensions{
        "VK_KHR_swapchain"};
    std::vector<VkExtensionProperties> available_device_extensions{};
    uint32_t device_extension_count = 0;
    err = vkEnumerateDeviceExtensionProperties(rend.physical_device, nullptr, &device_extension_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get count of VkPhysicalDevice's extensions with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    available_device_extensions.resize(device_extension_count, VkExtensionProperties{});
    err = vkEnumerateDeviceExtensionProperties(rend.physical_device, nullptr, &device_extension_count, available_device_extensions.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get VkPhysicalDevice's extensions with error code {}", magic_enum::enum_name(err));
        return -1;
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
            return -1;
        }
    }

    VkPhysicalDeviceFeatures2 physical_device_features{};
    physical_device_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos{};
    queue_create_infos.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_infos.pQueuePriorities = &queue_priority;
    queue_create_infos.queueCount = 1;
    queue_create_infos.queueFamilyIndex = 0;

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pNext = &physical_device_features;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());
    device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_infos;
    err = vkCreateDevice(rend.physical_device, &device_create_info, nullptr, &rend.device);
    if (err != VK_SUCCESS)
    {
        fmt::print("Unable to create device with error code {}", magic_enum::enum_name(err));
        return -1;
    }

    vkGetDeviceQueue(rend.device, 0, 0, &rend.main_queue);

    VkSurfaceCapabilitiesKHR surface_capabilities{};
    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(rend.physical_device, rend.surface, &surface_capabilities);
    if (err != VK_SUCCESS)
    {
        fmt::print("Unable to get physical device surface capabilities with err {}", magic_enum::enum_name(err));
        return -1;
    }
    uint32_t surface_format_count = 0;
    std::vector<VkSurfaceFormatKHR> surface_formats{};
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(rend.physical_device, rend.surface, &surface_format_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get count of VkSurfaceFormatKHRs with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    surface_formats.resize(surface_format_count, VkSurfaceFormatKHR{});
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(rend.physical_device, rend.surface, &surface_format_count, surface_formats.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get VkSurfaceFormatKHRs with error code {}", magic_enum::enum_name(err));
        return -1;
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
            return -1;
        }
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = rend.surface;
    swapchain_create_info.minImageCount = (surface_capabilities.minImageCount > 3) ? surface_capabilities.minImageCount : 3;
    swapchain_create_info.imageFormat = rend.swapchain_image_format;
    swapchain_create_info.imageColorSpace = rend.swapchain_image_colorspace;
    swapchain_create_info.imageExtent = VkExtent2D{static_cast<uint32_t>(settings.window_width), static_cast<uint32_t>(settings.window_height)};
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = nullptr;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_create_info.clipped = false;

    err = vkCreateSwapchainKHR(rend.device, &swapchain_create_info, nullptr, &rend.swapchain);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create swapchain with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    uint32_t swapchain_image_count = 0;
    err = vkGetSwapchainImagesKHR(rend.device, rend.swapchain, &swapchain_image_count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to get swapchain image count with code {}", magic_enum::enum_name(err));
        return -1;
    }
    std::vector<VkImage> swapchain_images(swapchain_image_count, VkImage{});
    err = vkGetSwapchainImagesKHR(rend.device, rend.swapchain, &swapchain_image_count, swapchain_images.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to get swapchain image count with code {}", magic_enum::enum_name(err));
        return -1;
    }

    rend.swapchain_frames.resize(swapchain_image_count, {});

    for (uint32_t i = 0; i < rend.swapchain_frames.size(); i++)
    {
        rend.swapchain_frames.at(i).image = swapchain_images.at(i);
    }
    for (auto &swapchain_frame : rend.swapchain_frames)
    {

        VkImageViewCreateInfo swapchain_image_view_create_info{};
        swapchain_image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        swapchain_image_view_create_info.format = rend.swapchain_image_format;
        swapchain_image_view_create_info.image = swapchain_frame.image;
        swapchain_image_view_create_info.subresourceRange = entire_subresource_range;
        swapchain_image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;

        err = vkCreateImageView(rend.device, &swapchain_image_view_create_info, nullptr, &swapchain_frame.image_view);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create swapchain image view with code {}", magic_enum::enum_name(err));
            return -1;
        }
    }

    VkCommandPoolCreateInfo command_buffer_create_info{};
    command_buffer_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    command_buffer_create_info.queueFamilyIndex = 0;
    command_buffer_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    err = vkCreateCommandPool(rend.device, &command_buffer_create_info, nullptr, &rend.submission_command_pool);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create command pool with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    VkCommandBufferAllocateInfo command_buffer_allocate_info{};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.commandPool = rend.submission_command_pool;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = swapchain_image_count;
    std::vector<VkCommandBuffer> command_buffers(swapchain_image_count, {});
    err = vkAllocateCommandBuffers(rend.device, &command_buffer_allocate_info, command_buffers.data());
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to create command buffers with error code {}", magic_enum::enum_name(err));
        return -1;
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
            return -1;
        }
        err = vkCreateSemaphore(rend.device, &semaphore_create_info, nullptr, &submission_frame.present_swapchain_semaphore);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create present semaphore with code {}", magic_enum::enum_name(err));
            return -1;
        }
        VkFenceCreateInfo fence_create_info{};
        fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        err = vkCreateFence(rend.device, &fence_create_info, nullptr, &submission_frame.fence);
        if (err != VK_SUCCESS)
        {
            fmt::print("Failed to create fence with code {}", magic_enum::enum_name(err));
            return -1;
        }
    }
    return 0;
}

int render(renderer &rend)
{
    VkResult err = VK_SUCCESS;
    auto &current_submission_frame = rend.submission_frames.at(rend.current_submission_frame_index);

    err = vkWaitForFences(rend.device, 1, &current_submission_frame.fence, VK_TRUE, 10000000000);
    if (err == VK_TIMEOUT)
    {
        fmt::print("vkWaitForFences on submission frame index {} exceeded timeout with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }
    else if (err != VK_SUCCESS)
    {
        fmt::print("vkWaitForFences on submission frame index {} failed with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }
    err = vkResetFences(rend.device, 1, &current_submission_frame.fence);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to reset fence from submission frame index {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }

    uint32_t next_swapchain_image_index = 0;
    err = vkAcquireNextImageKHR(rend.device, rend.swapchain, 1000000000, current_submission_frame.acquire_swapchain_semaphore, nullptr, &next_swapchain_image_index);
    if (err == VK_SUBOPTIMAL_KHR)
    {
        fmt::print("vkAcquireNextImageKHR reported VK_SUBOPTIMAL_KHR from swapchain frame index {}", rend.current_swapchain_frame_index);
        return -1;
    }
    else if (err != VK_SUCCESS)
    {
        fmt::print("Failed to reset fence from swapchain frame index {} with code {}", rend.current_swapchain_frame_index, magic_enum::enum_name(err));
        return -1;
    }

    auto &current_swapchain_frame = rend.swapchain_frames.at(next_swapchain_image_index);
    auto &cmd_buf = current_submission_frame.command_buffer;

    err = vkResetCommandBuffer(cmd_buf, 0);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to reset command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }
    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(cmd_buf, &command_buffer_begin_info);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to being command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }
    VkClearColorValue clear_value{0.f, 1.f, 0.f, 0.f};
    vkCmdClearColorImage(cmd_buf, current_swapchain_frame.image, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL, &clear_value, 1, &entire_subresource_range);

    err = vkEndCommandBuffer(cmd_buf);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to end command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }

    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &current_submission_frame.command_buffer;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &current_submission_frame.acquire_swapchain_semaphore;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &current_submission_frame.present_swapchain_semaphore;
    submit_info.pWaitDstStageMask = &wait_stages;
    err = vkQueueSubmit(rend.main_queue, 1, &submit_info, current_submission_frame.fence);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to submit command buffer from submission frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.pImageIndices = &next_swapchain_image_index;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &rend.swapchain;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &current_submission_frame.present_swapchain_semaphore;
    present_info.pResults = nullptr;

    vkQueuePresentKHR(rend.main_queue, &present_info);
    if (err != VK_SUCCESS)
    {
        fmt::print("Failed to present swapchain frame {} with code {}", rend.current_submission_frame_index, magic_enum::enum_name(err));
        return -1;
    }
    rend.current_submission_frame_index = (rend.current_submission_frame_index + 1) % 2;

    return 0;
}

int shutdown(renderer &rend)
{
    vkDeviceWaitIdle(rend.device);
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

    glfwDestroyWindow(rend.glfw_window);
    glfwTerminate();
    return 0;
}

int main()
{
    init_settings settings{};
    renderer rend{};
    int ret = init(settings, rend);
    if (ret != 0)
    {
        return ret;
    }
    while (!glfwWindowShouldClose(rend.glfw_window))
    {
        glfwPollEvents();
        ret = render(rend);
        if (ret != 0)
        {
            return ret;
        }
    }
    ret = shutdown(rend);
    if (ret != 0)
    {
        return ret;
    }
    return 0;
}
