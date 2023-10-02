#ifndef MATMUL_GPU_H
#define MATMUL_GPU_H

#include "wgpu_util.hpp"

int multiplyMatricesGPU(WGPUInstance instance, WGPUDevice device, WGPUQueue queue, uint32_t* a, uint32_t* b, uint32_t* res, uint32_t dim) {
    uint32_t mlen = dim * dim;
    uint32_t msize = mlen * sizeof(a[0]);

    WGPUShaderModuleDescriptor shdesc = {};
    WGPUShaderModuleWGSLDescriptor wgsldesc = {};
    wgsldesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    wgsldesc.code = R"(
        @group(0)
        @binding(0)
        var<storage, read_write> results: array<u32>;

        @group(0)
        @binding(1)
        var<storage, read> input_a: array<u32>;

        @group(0)
        @binding(2)
        var<storage, read> input_b: array<u32>;

        @group(0)
        @binding(3)
        var<storage, read> dimension: u32;

        @compute
        @workgroup_size(16, 16, 1)
        fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
            let batch = global_id.z;
            let row = global_id.x + batch * 16;
            let col = global_id.y + batch * 16;

            var sum: u32 = 0;
            for (var i: u32 = 0; i < dimension; i = i+1) {
                sum = sum + input_a[i+dimension*row] * input_b[col+dimension*i];
            }
            results[row * dimension + col] = sum;
        }
    )";
    shdesc.nextInChain = (WGPUChainedStruct*) &wgsldesc;
    shdesc.label = "compute_shader";
    auto smod = wgpuDeviceCreateShaderModule(device, &shdesc); // TODO: release
    if (smod == nullptr) {
        std::cerr << "Failed to create the shader module" << std::endl;
        return -1;
    }

    WGPUComputePipelineDescriptor pipd = {
                  .label = "compute_pipeline",
                  .compute =
                      (const WGPUProgrammableStageDescriptor){
                          .module = smod,
                          .entryPoint = "main",
                      },
              };
    auto compute_pipeline = wgpuDeviceCreateComputePipeline(device, &pipd); // TODO: release

    auto bind_group_layout = wgpuComputePipelineGetBindGroupLayout(compute_pipeline, 0); // TODO: release

    WGPUBufferDescriptor res_staging_desc = {
                  .label = "result_staging_buffer",
                  .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto res_staging = wgpuDeviceCreateBuffer(device, &res_staging_desc); // TODO: unmap and release?

    WGPUBufferDescriptor res_storage_desc = {
                  .label = "result_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto res_storage = wgpuDeviceCreateBuffer(device, &res_storage_desc); // TODO: unmap and release?

    WGPUBufferDescriptor ma_storage_desc = {
                  .label = "input_a_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto ma_storage = wgpuDeviceCreateBuffer(device, &ma_storage_desc); // TODO: unmap and release?

    WGPUBufferDescriptor mb_storage_desc = {
                  .label = "input_a_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto mb_storage = wgpuDeviceCreateBuffer(device, &mb_storage_desc); // TODO: unmap and release?

    WGPUBufferDescriptor dim_storage_desc = {
                  .label = "dim_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = sizeof(dim),
                  .mappedAtCreation = false,
              };
    auto dim_storage = wgpuDeviceCreateBuffer(device, &dim_storage_desc); // TODO: unmap and release?

    WGPUBindGroupEntry bge[] = {
        (const WGPUBindGroupEntry){
            .binding = 0,
            .buffer = res_storage,
            .offset = 0,
            .size = msize,
        },
        (const WGPUBindGroupEntry){
            .binding = 1,
            .buffer = ma_storage,
            .offset = 0,
            .size = msize,
        },
        (const WGPUBindGroupEntry){
            .binding = 2,
            .buffer = mb_storage,
            .offset = 0,
            .size = msize,
        },
        (const WGPUBindGroupEntry){
            .binding = 3,
            .buffer = dim_storage,
            .offset = 0,
            .size = sizeof(dim),
        },
    };
    WGPUBindGroupDescriptor bgd = {
                  .label = "bind_group",
                  .layout = bind_group_layout,
                  .entryCount = 4,
                  .entries = bge,
              };
    auto bind_group = wgpuDeviceCreateBindGroup(device, &bgd); // TODO: release

    wgpuQueueWriteBuffer(queue, res_storage, 0, &res[0], msize);
    wgpuQueueWriteBuffer(queue, ma_storage, 0, &a[0], msize);
    wgpuQueueWriteBuffer(queue, mb_storage, 0, &b[0], msize);
    wgpuQueueWriteBuffer(queue, dim_storage, 0, &dim, sizeof(dim));

    WGPUCommandEncoderDescriptor cedesc = {
        .label = "command_encoder",
    };
    auto cenc = wgpuDeviceCreateCommandEncoder(device, &cedesc); // TODO: release
    if (cenc == nullptr) {
        std::cerr << "Failed to create a command encoder" << std::endl;
        return -1;
    }

    WGPUComputePassDescriptor cpdesc = {
        .label = "compute_pass",
    };
    auto compute_pass_encoder = wgpuCommandEncoderBeginComputePass(cenc, &cpdesc); // TODO: release
    wgpuComputePassEncoderSetPipeline(compute_pass_encoder, compute_pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, 0, bind_group, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, dim/16, dim/16, 1);
    wgpuComputePassEncoderEnd(compute_pass_encoder);

    wgpuCommandEncoderCopyBufferToBuffer(cenc, res_storage, 0, res_staging, 0, msize);

    WGPUCommandBufferDescriptor cbufd = {
                             .label = "command_buffer",
                         };
    auto command_buffer = wgpuCommandEncoderFinish(cenc, &cbufd); // TODO: release

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    wgpuQueueSubmit(queue, 1, &command_buffer);

    bool done = false;
    wgpuBufferMapAsync(res_staging, WGPUMapMode_Read, 0, msize, handle_buffer_map, &done);
    while (!done) {
        wgpuInstanceProcessEvents(instance);
#ifdef BACKEND_WEBGPUDD
        webGPUDDFlush();
#endif /* BACKEND_WEBGPUDD */
        using namespace std::chrono_literals;
        std::unique_lock lk(m);
        cv.wait_for(lk, 1us, [&] { return done; });
    }

    auto buf = (uint32_t *)wgpuBufferGetConstMappedRange(res_staging, 0, msize);
    if (buf == nullptr) {
        std::cerr << "Failed to retrieve computation results" << std::endl;
        return -1;
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "[" << dim << "x" << dim << "] of " << a[0] << "s * " << "[" << dim << "x" << dim << "] of " << b[0] << "s" << std::endl;
    std::cout << "RESULT: [" << dim << "x" << dim << "] of " << buf[0] << "s" << std::endl;
    std::cout << "GPU time = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;

    return 0;
}

#endif /* MATMUL_GPU_H */
