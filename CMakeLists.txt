cmake_minimum_required(VERSION 3.20)
project(uvco LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

find_package(UV REQUIRED)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra)

add_library(uvco INTERFACE)

target_include_directories(uvco INTERFACE
    ${CMAKE_SOURCE_DIR}/include
    )

target_link_libraries(uvco INTERFACE
    UV::uv
    )

add_subdirectory(samples)
