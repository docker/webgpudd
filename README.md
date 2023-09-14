# Docker Desktop WebGPU runtime

## Structure

This repo contains code needed to built the Docker Desktop WebGPU runtime. The
runtime is spit into two parts:

* the server - meant to run on the host system and expose GPU capabilities to
the client (currently only buildable and tested on macOS);
* the client - a shared library which will be shipped in the Docker Desktop VM
image, which provides the WebGPU symbols as specified in webgpu.h and a couple
of additional calls needed to initialise the runtime (buildable and tested on
macOS and Linux).

Aside from these the repo contains examples of how to use libwebgpudd.so
(currently `matmul.cpp`, build target `matmul-dd`).

## Building host binaries

The Docker Desktop WebGPU runtime is built on top of Dawn (https://dawn.googlesource.com/dawn)
so start by downloading and building it. To do that, follow instructions in
https://dawn.googlesource.com/dawn/+/HEAD/docs/building.md .

Once that is done you can build the Docker Desktop WebGPU runtime by running:

    DAWN_BUILD_DIR=<path to the Dawn build> make

To build just the client library:

    DAWN_BUILD_DIR=<path to the Dawn build> make libwebgpudd

To build the server:

    DAWN_BUILD_DIR=<path to the Dawn build> make server

To build the matrix multiplication example code:

    DAWN_BUILD_DIR=<path to the Dawn build> make matmul-dd

## Building DD binaries

To build the client side of webgpudd simply run:

    docker build .

For now this creates an image with libwebgpu and examples installed.
Instructions on how to use the examples also get installed in the image. Don't
forget to run:

    ./out/server

on the host before running the example code inside DD.

## Running samples on macOS

First start the server:

    ./out/server

The server will print `ready to receive connections` when it is ready to work.

Then run:

    LIBRARY_PATH=./out ./out/matmul-dd

Once the client is done running close it with `^C`, this will terminate the
client and server.
