#include <vector>
#include <string>

#include <fmt/format.h>
#include <magic_enum.hpp>
#include <vulkan/vulkan_core.h>
#include <GLFW/glfw3.h>

#include "renderer.hpp"
#include "window.hpp"

int main()
{
    init_settings settings{};
    renderer rend{};
    rend.glfw_window = create_window(settings);
    if (!rend.glfw_window)
    {
        return -1;
    }
    int ret = init_renderer(settings, rend);
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
        rend.frame_count++;
    }
    shutdown_renderer(rend);
    glfwDestroyWindow(rend.glfw_window);
    glfwTerminate();
    return 0;
}
