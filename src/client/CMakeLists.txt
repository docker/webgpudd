add_library(webgpudd SHARED "webgpudd.cpp")
target_sources(webgpudd PRIVATE
    "dawn_client.cpp"
    "dawn_client.h"
    "proc.cpp"
    "../common/dawn_command_buffer.cpp"
    "../common/dawn_command_buffer.h"
    "../common/dawn_command_transport.cpp"
    "../common/dawn_command_transport.h"
)
target_link_libraries(webgpudd PUBLIC
    dawn_wire
    tint_lang_wgsl_reader_parser
)
