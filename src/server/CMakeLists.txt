add_executable(com.docker.webgpu-runtime "webgpu_server.cpp")
target_sources(com.docker.webgpu-runtime PRIVATE
    "dawn_server.cpp"
    "dawn_server.h"
    "../common/dawn_command_buffer.cpp"
    "../common/dawn_command_buffer.h"
    "../common/dawn_command_transport.cpp"
    "../common/dawn_command_transport.h"
)
target_link_libraries(com.docker.webgpu-runtime PUBLIC
    dawncpp
    dawn_native
    dawn_wire
    dl
)
