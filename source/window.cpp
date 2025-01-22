#include "window.hpp"
#include <fmt/format.h>

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

GLFWwindow *create_window(init_settings &settings)
{
    if (!glfwInit())
    {
        fmt::print("Failed to initialize glfw");
        return nullptr;
    }
    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    auto glfw_window = glfwCreateWindow(settings.window_width, settings.window_height, "WaModRen", NULL, NULL);
    if (glfw_window == nullptr)
    {
        fmt::print("Failed to create glfw window");
        return nullptr;
    }
    glfwSetWindowCloseCallback(glfw_window, window_close_callback);
    glfwSetKeyCallback(glfw_window, glfw_key_callback);

    return glfw_window;
}
