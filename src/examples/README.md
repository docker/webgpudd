# WebGPU DD examples

This directory contains examples of how to use the Docker Desktop WebGPU
runtime. In essence those are mostly pure WebGPU code, with a small bit of
webgpudd-specific init and teardown.

The following instructions assume the examples are being built inside Docker
Desktop with WebGPU integration enabled.

To build the matrix multiplication example simply run:

    g++ --std=c++17 -O2 -D BACKEND_GPU -D BACKEND_WEBGPUDD matmul.cpp -o matmul -lwebgpudd

and to run it:

    ./matmul
