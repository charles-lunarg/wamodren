add_executable(renderer main.cpp)

target_link_libraries(renderer
    Vulkan::Vulkan
    glfw
    fmt::fmt
    magic_enum::magic_enum
)
