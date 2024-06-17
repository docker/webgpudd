cmake_minimum_required(VERSION 3.13)

project(
    webgpudd
    DESCRIPTION "WebGPU integration for Docker Desktop"
    LANGUAGES C CXX
)

set(DAWN_FETCH_DEPENDENCIES ON)

add_subdirectory(third_party/dawn)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-fPIC -O2 -fno-exceptions -fno-rtti -Wall -Werror")

add_subdirectory(src/server)
add_subdirectory(src/client)
