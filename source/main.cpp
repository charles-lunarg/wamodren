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

struct init_settings
{
    int window_width = 800;
    int window_height = 600;
};

struct renderer
{
    GLFWwindow *glfw_window{};
    VkInstance inst{};
    VkSurfaceKHR surface{};
    VkPhysicalDevice physical_device{};
    VkDevice device{};
    VkSwapchainKHR swapchain{};
    VkImage swapchain_images{};
    VkImageView swapchain_image_views{};
    VkFormat swapchain_image_format{};
    VkColorSpaceKHR swapchain_image_colorspace{};
};

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
    glfwSetKeyCallback(rend.glfw_window, glfw_key_callback);
    uint32_t count = 0;
    const char **required_glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector<const char *> required_instance_extensions;
    for (uint32_t i = 0; i < count; i++)
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
    count = 1;
    err = vkEnumeratePhysicalDevices(rend.inst, &count, &rend.physical_device);
    if (err < VK_SUCCESS)
    {
        fmt::print("Failed to enumerate physical devices with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    VkQueueFamilyProperties queue_family_properties{};
    vkGetPhysicalDeviceQueueFamilyProperties(rend.physical_device, &count, &queue_family_properties);
    if ((queue_family_properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0 ||
        (queue_family_properties.queueFlags & VK_QUEUE_COMPUTE_BIT) == 0 ||
        (queue_family_properties.queueFlags & VK_QUEUE_TRANSFER_BIT) == 0)
    {
        fmt::print("VkPhysicalDevice's first queue doesn't support all required queue operations of graphics, compute, and transfer");
        return -1;
    }
    std::vector<const char *> required_device_extensions{};
    std::vector<VkExtensionProperties> available_device_extensions{};
    count = 0;
    err = vkEnumerateDeviceExtensionProperties(rend.physical_device, nullptr, &count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get count of VkPhysicalDevice's extensions with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    available_device_extensions.resize(count, VkExtensionProperties{});
    err = vkEnumerateDeviceExtensionProperties(rend.physical_device, nullptr, &count, available_device_extensions.data());
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

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.enabledExtensionCount = static_cast<uint32_t>(required_device_extensions.size());
    device_create_info.ppEnabledLayerNames = required_device_extensions.data();
    err = vkCreateDevice(rend.physical_device, &device_create_info, nullptr, &rend.device);
    if (err != VK_SUCCESS)
    {
        fmt::print("Unable to create device with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    count = 0;
    std::vector<VkSurfaceFormatKHR> surface_formats{};
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(rend.physical_device, rend.surface, &count, nullptr);
    if (err != VK_SUCCESS)
    {
        fmt::print("Cannot get count of VkSurfaceFormatKHRs with error code {}", magic_enum::enum_name(err));
        return -1;
    }
    surface_formats.resize(count, VkSurfaceFormatKHR{});
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(rend.physical_device, rend.surface, &count, surface_formats.data());
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
    swapchain_create_info.minImageCount = 3;
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
        fmt::print("Cannot find suitable swapchain image format");
        return -1;
    }
    return 0;
}

int shutdown(renderer &rend)
{

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
    }
    ret = shutdown(rend);
    if (ret != 0)
    {
        return ret;
    }
    return 0;
}
