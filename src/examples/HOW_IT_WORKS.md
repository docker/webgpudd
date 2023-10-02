# How it works?

* CDI is used to amend the container with the WebGPU client library, headers
  and comms endpoint

    docker run --device "docker.com/gpu=webgpu" --rm -it dc23-demo

* the matrix multiplication example is rebuilt and linked against the WebGPU
  client library

    -- host --                  |    -- DD VM --
                                |
    .---- DD backend -----.     |     .------- Container ---------.
    | .- WebGPU server -. |     |     |.------- workload --------.|
    | |                 | |     |     || .- WebGPU client lib -. ||
    | |                 |-|--vsocket--||-|                     | ||
    | |                 | |     |     || '---------------------' ||
    | '-----------------' |     |     |'-------------------------'|
    '---------------------'     |     '---------------------------'

* the WebGPU server and client library are based on Dawn
* the WebGPU client library provides the WebGPU API, as defined by webgpu.h
