#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>

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
    VkQueue main_queue{};
    VkSurfaceCapabilitiesKHR surface_capabilities{};
    VkSwapchainKHR swapchain{};
    VkFormat swapchain_image_format{};
    VkColorSpaceKHR swapchain_image_colorspace{};
    VkRect2D swapchain_image_render_area{};
    VkCommandPool submission_command_pool{};
    uint32_t current_swapchain_frame_index = 0;
    std::vector<swapchain_frame> swapchain_frames;

    uint32_t current_submission_frame_index = 0;
    std::vector<submission_frame> submission_frames;

    uint64_t frame_count = 0;

    // Simple Gradient pipeline
    VkDescriptorSetLayout gradient_descriptor_set_layout{};
    VkPipelineLayout gradient_pipeline_layout{};
    VkPipeline gradient_pipeline{};
    VkDescriptorPool gradient_descriptor_pool{};
    VkDescriptorSet gradient_descriptor_set{};
};

enum class pipeline_type
{
    graphics,
    compute
};

struct pipeline_create_details
{
    pipeline_type type;
    std::string shader_name;
};

int init_renderer(init_settings &settings, renderer &rend);
VkResult render(renderer &rend);

void shutdown_renderer(renderer &rend);
