cmake_minimum_required(VERSION 3.25)

project(WriteAModernRenderer)

option(ENABLE_ASAN "Compile with Address Sanitizer")


include(cmake/get_package_deps.cmake)

add_subdirectory(source)
