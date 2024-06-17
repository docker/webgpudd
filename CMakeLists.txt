cmake_minimum_required(VERSION 3.13)

project(
    webgpudd
    DESCRIPTION "WebGPU integration for Docker Desktop"
    LANGUAGES C CXX
)

option(WEBGPUDD_BUILD_CLIENT "enable WebGPU client library build" ON)
option(WEBGPUDD_BUILD_RUNTIME "enable WebGPU runtime build" ON)

set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_ENABLE_PIC ON)

if(NOT WEBGPUDD_BUILD_RUNTIME)
    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(DAWN_ENABLE_VULKAN OFF)
    set(DAWN_USE_WAYLAND OFF)
    set(DAWN_USE_X11 OFF)
    set(DAWN_USE_GLFW OFF)
    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_VULKAN OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(DAWN_BUILD_SAMPLES OFF)
    set(TINT_BUILD_TESTS OFF)
    set(TINT_BUILD_CMD_TOOLS OFF)
endif()

add_subdirectory(third_party/dawn)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fPIC -O2 -fno-exceptions -fno-rtti -Wall -Werror")

if(WEBGPUDD_BUILD_CLIENT)
    add_subdirectory(src/client)
endif()

if(WEBGPUDD_BUILD_RUNTIME)
    add_subdirectory(src/server)
endif()
