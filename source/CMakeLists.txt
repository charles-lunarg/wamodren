add_executable(renderer
    main.cpp
    renderer.hpp
    renderer.cpp
    window.hpp
    window.cpp
)

target_link_libraries(renderer
    Vulkan::Vulkan
    glfw
    fmt::fmt
    magic_enum::magic_enum
)

if (ENABLE_ASAN)
    target_compile_options(renderer PUBLIC -fsanitize=address)
    target_link_options(renderer PUBLIC -fsanitize=address)
endif()
