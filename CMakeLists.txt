cmake_minimum_required(VERSION 3.20)
project(uvco LANGUAGES C CXX)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

option(WITH_LOCAL_LIBUV "force build with local libuv repo fetched from github" OFF)

if(NOT WITH_LOCAL_LIBUV)
    find_package(UV)
endif()

if(NOT UV_FOUND)
    message("fetch libuv ...")
    include(FetchContent)
    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG v1.41.0
        GIT_SHALLOW 1
        )
    FetchContent_MakeAvailable(libuv)
endif()

include(ExternalProject)
ExternalProject_Add(cppcoro
    GIT_REPOSITORY https://github.com/dolmens/cppcoro.git
    CONFIGURE_COMMAND ""
    BUILD_COMMAND     ""
    UPDATE_COMMAND    ""
    INSTALL_COMMAND   ""
    LOG_DOWNLOAD ON
    )
ExternalProject_Get_Property(cppcoro SOURCE_DIR)
set(CppcoroIncludeDir ${SOURCE_DIR}/include)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
add_compile_options(-Wall -Wextra)

add_library(uvco INTERFACE)

target_include_directories(uvco INTERFACE
    ${CMAKE_SOURCE_DIR}/include
    )

target_link_libraries(uvco INTERFACE
    uv
    )

add_subdirectory(samples)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
