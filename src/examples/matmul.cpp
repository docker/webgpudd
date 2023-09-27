#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <webgpu.h>

#ifdef BACKEND_DAWN_NATIVE
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#endif /* BACKEND_DAWN_NATIVE */

#ifdef BACKEND_WEBGPUDD
#include <webgpudd.h>
#endif /* BACKEND_WEBGPUDD */

std::mutex m;
std::condition_variable cv;

void adapterRequestCb(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * userdata) {
    if (message != nullptr) {
        std::cerr << "adapter request message: " << message << std::endl;
    }
    {
        std::unique_lock lk(m);
        *((WGPUAdapter*) userdata) = adapter;
    }
    cv.notify_all();
}

void deviceRequestCb(WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * userdata) {
    if (message != nullptr) {
        std::cerr << "device request message: " << message << std::endl;
    }
    {
        std::unique_lock lk(m);
        *((WGPUDevice*) userdata) = device;
    }
    cv.notify_all();
}

static void handle_buffer_map(WGPUBufferMapAsyncStatus status, void *userdata) {
    {
        std::unique_lock lk(m);
        *((bool*)userdata) = true;
    }
    cv.notify_all();
}

void PrintDeviceError(WGPUErrorType errorType, const char* message, void*) {
    const char* errorTypeName = "";
    switch (errorType) {
        case WGPUErrorType_Validation:
            errorTypeName = "Validation";
            break;
        case WGPUErrorType_OutOfMemory:
            errorTypeName = "Out of memory";
            break;
        case WGPUErrorType_Unknown:
            errorTypeName = "Unknown";
            break;
        case WGPUErrorType_DeviceLost:
            errorTypeName = "Device lost";
            break;
        default:
            return;
    }
    std::cerr << errorTypeName << " error: " << message << std::endl;
}

void DeviceLogCallback(WGPULoggingType type, const char* message, void*) {
    std::cerr << "Device log: " << message << std::endl;
}

void multiplyMatricesParallel(uint32_t* a, uint32_t* b, uint32_t* res, uint32_t dim) {
    uint32_t batchsz = 64;
    std::vector<std::thread*> threads;

    for (int by = 0; by < dim; by += batchsz) {
        for (int bx = 0; bx < dim; bx += batchsz) {
            auto t = new std::thread([&, by, bx] {
                for (int i = by; i < by+batchsz; ++i) {
                    for (int j = bx; j < bx+batchsz; ++j) {
                        res[j + dim * i] = 0;
                        for (int k = 0; k < dim; ++k)
                            res[j + dim * i] += a[k + dim * i] * b[j + dim * k];
                    }
                }
            });
            threads.push_back(t);
        }
    }

    for (auto t : threads) {
        t->join();
        delete t;
    }
}


void multiplyMatrices(uint32_t* a, uint32_t* b, uint32_t* res, uint32_t dim) {
    multiplyMatricesParallel(a, b, res, dim);
}

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

    auto smod = wgpuDeviceCreateShaderModule(device, &shdesc);
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
    auto compute_pipeline = wgpuDeviceCreateComputePipeline(device, &pipd);

    auto bind_group_layout = wgpuComputePipelineGetBindGroupLayout(compute_pipeline, 0);

    WGPUBufferDescriptor res_staging_desc = {
                  .label = "result_staging_buffer",
                  .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto res_staging = wgpuDeviceCreateBuffer(device, &res_staging_desc);

    WGPUBufferDescriptor res_storage_desc = {
                  .label = "result_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto res_storage = wgpuDeviceCreateBuffer(device, &res_storage_desc);

    WGPUBufferDescriptor ma_storage_desc = {
                  .label = "input_a_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto ma_storage = wgpuDeviceCreateBuffer(device, &ma_storage_desc);

    WGPUBufferDescriptor mb_storage_desc = {
                  .label = "input_a_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = msize,
                  .mappedAtCreation = false,
              };
    auto mb_storage = wgpuDeviceCreateBuffer(device, &mb_storage_desc);

    WGPUBufferDescriptor dim_storage_desc = {
                  .label = "dim_storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = sizeof(dim),
                  .mappedAtCreation = false,
              };
    auto dim_storage = wgpuDeviceCreateBuffer(device, &dim_storage_desc);

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
    auto bind_group = wgpuDeviceCreateBindGroup(device, &bgd);

    wgpuQueueWriteBuffer(queue, res_storage, 0, &res[0], msize);
    wgpuQueueWriteBuffer(queue, ma_storage, 0, &a[0], msize);
    wgpuQueueWriteBuffer(queue, mb_storage, 0, &b[0], msize);
    wgpuQueueWriteBuffer(queue, dim_storage, 0, &dim, sizeof(dim));

    WGPUCommandEncoderDescriptor cedesc = {
        .label = "command_encoder",
    };
    auto cenc = wgpuDeviceCreateCommandEncoder(device, &cedesc);
    if (cenc == nullptr) {
        std::cerr << "Failed to create a command encoder" << std::endl;
        return -1;
    }

    WGPUComputePassDescriptor cpdesc = {
        .label = "compute_pass",
    };
    auto compute_pass_encoder = wgpuCommandEncoderBeginComputePass(cenc, &cpdesc);
    wgpuComputePassEncoderSetPipeline(compute_pass_encoder, compute_pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, 0, bind_group, 0, NULL);
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, dim/16, dim/16, 1);
    wgpuComputePassEncoderEnd(compute_pass_encoder);

    wgpuCommandEncoderCopyBufferToBuffer(cenc, res_storage, 0, res_staging, 0, msize);
    WGPUCommandBufferDescriptor cbufd = {
                             .label = "command_buffer",
                         };

    auto command_buffer = wgpuCommandEncoderFinish(cenc, &cbufd);

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
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "GPU time = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[µs]" << std::endl;

    auto buf = (uint32_t *)wgpuBufferGetConstMappedRange(res_staging, 0, msize);
#ifdef BACKEND_WEBGPUDD
    webGPUDDFlush();
#endif /* BACKEND_WEBGPUDD */
    if (buf == nullptr) {
        std::cerr << "Failed to retrieve computation results" << std::endl;
        return -1;
    }
    std::cout << "RESULT: " << buf[0] << ", " << buf[1] << ", " << buf[2] << ", " << buf[3] << std::endl;

    return 0;
}

int main(int argc, char** argv) {
    int ret;
#ifdef BACKEND_DAWN_NATIVE
    DawnProcTable procs = dawn::native::GetProcs();
    dawnProcSetProcs(&procs);
    WGPUInstanceDescriptor instanceDesc = {0};
    auto instance = wgpuCreateInstance(&instanceDesc);
#endif /* BACKEND_DAWN_NATIVE */

#ifdef BACKEND_WEBGPUDD
    ret = initWebGPUDD();
    if (ret) {
        std::cerr << "Failed to initialise the WebGPU client: " << ret << std::endl;
        return ret;
    }
    auto instance = getWebGPUDDInstance();
#endif /* BACKEND_WEBGPUDD */

    if (instance == nullptr) {
        std::cerr << "Failed to create a WebGPU instance" << std::endl;
        return -1;
    }

    WGPUAdapter adapter = nullptr;
    wgpuInstanceRequestAdapter(instance, nullptr, adapterRequestCb, &adapter);
#ifdef BACKEND_WEBGPUDD
    webGPUDDFlush();
#endif /* BACKEND_WEBGPUDD */
    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return adapter != nullptr; });
    }

    WGPUDevice device = nullptr;
    wgpuAdapterRequestDevice(adapter, nullptr, deviceRequestCb, &device);
#ifdef BACKEND_WEBGPUDD
    webGPUDDFlush();
#endif /* BACKEND_WEBGPUDD */
    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return device != nullptr; });
    }

    wgpuDeviceSetUncapturedErrorCallback(device, PrintDeviceError, nullptr);
    wgpuDeviceSetLoggingCallback(device, DeviceLogCallback, nullptr);

    auto queue = wgpuDeviceGetQueue(device);
    if (queue == nullptr) {
        std::cerr << "Failed to obtain device queue" << std::endl;
    }

    constexpr uint32_t dimension = 768;
    constexpr uint32_t size = dimension * dimension;
    std::array<uint32_t, size> res;
    std::array<uint32_t, size> ma;
    std::array<uint32_t, size> mb;

    for (int i = 0; i < ma.size(); ++i)
        ma[i] = 2;
    for (int i = 0; i < mb.size(); ++i)
        mb[i] = 3;

    ret = multiplyMatricesGPU(instance, device, queue, &ma[0], &mb[0], &res[0], dimension);
    if (ret) {
        std::cerr << "GPU computation failed" << std::endl;
        return ret;
    }

    std::chrono::steady_clock::time_point beginCPU = std::chrono::steady_clock::now();
    multiplyMatrices(&ma[0], &mb[0], &res[0], dimension);
    std::chrono::steady_clock::time_point endCPU = std::chrono::steady_clock::now();
    std::cout << "CPU time = " << std::chrono::duration_cast<std::chrono::microseconds>(endCPU - beginCPU).count() << "[µs]" << std::endl;
    std::cout << "RESULT: " << res[0] << ", " << res[1] << ", " << res[2] << ", " << res[3] << std::endl;

#ifdef BACKEND_WEBGPUDD
    finaliseWebGPUDD();
#endif /* BACKEND_WEBGPUDD */
    return 0;
}
