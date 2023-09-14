#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <dawn/dawn_proc.h>
#include <dawn/webgpu.h>
#include <dawn/wire/WireClient.h>

#include "client_tcp.h"
#include "command_buffer.h"

std::mutex m;
std::condition_variable cv;

void adapterRequestCb(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * userdata) {
    if (message != nullptr) {
        std::cout << "adapter request message: " << message << std::endl;
    }
    std::cout << "assigining adapter handle" << std::endl;
    {
        std::unique_lock lk(m);
        *((WGPUAdapter*) userdata) = adapter;
    }
    cv.notify_all();
}

static void handle_buffer_map(WGPUBufferMapAsyncStatus status, void *userdata) {
    std::cout << "buffer_map status=" << status << std::endl;
    {
        std::unique_lock lk(m);
        *((bool*)userdata) = true;
    }
    cv.notify_all();
}

static void handle_queue_done(WGPUQueueWorkDoneStatus status, void * userdata) {
    std::cout << "queue done status=" << status << std::endl;
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
    std::cout << errorTypeName << " error: " << message << std::endl;
}

void DeviceLogCallback(WGPULoggingType type, const char* message, void*) {
    std::cout << "Device log: " << message << std::endl;
}

int main(int argc, char** argv) {
    DawnProcTable procs = dawn::wire::client::GetProcs();

    auto c2sBuf = new SendBuffer();
    auto s2cBuf = new RecvBuffer();

    dawn::wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = c2sBuf;

    auto wireClient = new dawn::wire::WireClient(clientDesc);

    TCPCommandClientConnection cmdt;

    int err = cmdt.Init();
    if (err < 0) {
        return err;
    }

    s2cBuf->SetHandler(wireClient);
    c2sBuf->SetTransport(&cmdt);

    dawnProcSetProcs(&procs);
    // procs.deviceSetUncapturedErrorCallback(cDevice, PrintDeviceError, nullptr);
    // procs.deviceSetDeviceLostCallback(cDevice, DeviceLostCallback, nullptr);
    // procs.deviceSetLoggingCallback(cDevice, DeviceLogCallback, nullptr);

    std::thread recvt([&] {
        cmdt.Recv(s2cBuf);
    });

    std::cout << "creating instance" << std::endl;

    //WGPUInstanceDescriptor widesc;
    //auto wi = wgpuCreateInstance(&widesc);
    auto wi = wireClient->ReserveInstance();

    std::cout << "reserved instance " << wi.id << " generation " << wi.generation << std::endl;

    // std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "requesting adapter" << std::endl;

    WGPUAdapter adapter = nullptr;

    wgpuInstanceRequestAdapter(wi.instance, nullptr, adapterRequestCb, &adapter);
    c2sBuf->Flush();

    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return adapter != nullptr; });
    }

    std::cout << "adapter received" << std::endl;

    WGPUAdapterProperties adapterProperties;
    wgpuAdapterGetProperties(adapter, &adapterProperties);

    std::cout << adapterProperties.vendorName << " " << adapterProperties.architecture << std::endl;

    //auto device = wgpuAdapterCreateDevice(adapter, nullptr);
    //c2sBuf->Flush();

    //if (device == nullptr) {
    //    std::cout << "device is null" << std::endl;
    //} else {
    //    std::cout << "device is not null" << std::endl;
    //}

    auto device = wireClient->ReserveDevice();
    std::cout << "reserved device " << device.id << " generation " << device.generation << std::endl;

    procs.deviceSetUncapturedErrorCallback(device.device, PrintDeviceError, nullptr);
    procs.deviceSetLoggingCallback(device.device, DeviceLogCallback, nullptr);

    WGPUFeatureName features[100];

    auto nfeatures = wgpuDeviceEnumerateFeatures(device.device, features);
    c2sBuf->Flush();
    std::cout << "enumerated " << nfeatures << " features" << std::endl;
    for (int i = 0; i < nfeatures; ++i) {
        std::cout << "feature: " << features[nfeatures] << std::endl;
    }

    auto queue = wgpuDeviceGetQueue(device.device);
    c2sBuf->Flush();
    if (queue == nullptr) {
        std::cout << "queue is null" << std::endl;
    } else {
        std::cout << "queue is not null" << std::endl;
    }

    WGPUShaderModuleDescriptor shdesc = {};
    WGPUShaderModuleWGSLDescriptor wgsldesc = {};

    wgsldesc.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
    //wgsldesc.code = R"(
    //    @group(0)
    //    @binding(0)
    //    var<storage, read_write> arr: array<u32>;

    //    @compute
    //    @workgroup_size(1)
    //    fn main() {
    //        arr[0] = arr[0] + arr[1] + arr[2] + arr[3];
    //        while (true) {
    //            arr[1] += 1;
    //        }
    //    }
    //)";

    wgsldesc.code = R"(
        @group(0)
        @binding(0)
        var<storage, read_write> v_indices: array<u32>; // this is used as both input and output for convenience

        // The Collatz Conjecture states that for any integer n:
        // If n is even, n = n/2
        // If n is odd, n = 3n+1
        // And repeat this process for each new n, you will always eventually reach 1.
        // Though the conjecture has not been proven, no counterexample has ever been found.
        // This function returns how many times this recurrence needs to be applied to reach 1.
        fn collatz_iterations(n_base: u32) -> u32{
            var n: u32 = n_base;
            var i: u32 = 0u;
            loop {
                if (n <= 1u) {
                    break;
                }
                if (n % 2u == 0u) {
                    n = n / 2u;
                }
                else {
                    // Overflow? (i.e. 3*n + 1 > 0xffffffffu?)
                    if (n >= 1431655765u) {   // 0x55555555u
                        return 4294967295u;   // 0xffffffffu
                    }

                    n = 3u * n + 1u;
                }
                i = i + 1u;
            }
            return i;
        }

        @compute
        @workgroup_size(1)
        fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
            v_indices[global_id.x] = collatz_iterations(v_indices[global_id.x]);
        }
    )";

    shdesc.nextInChain = (WGPUChainedStruct*) &wgsldesc;
    shdesc.label = "compute_shader";

    auto smod = wgpuDeviceCreateShaderModule(device.device, &shdesc);
    c2sBuf->Flush();
    if (smod == nullptr) {
        std::cout << "shader module is null" << std::endl;
    } else {
        std::cout << "shader module is not null" << std::endl;
    }

    while (1) {
    std::cout << "DEBUG: numbers" << std::endl;
    uint32_t numbers[] = {1, 690234923, 32234234, 4234234};
    uint32_t numbers_size = sizeof(numbers);
    uint32_t numbers_length = numbers_size / sizeof(uint32_t);
    std::cout << "DEBUG: numbers done" << std::endl;

    WGPUBufferDescriptor bufd = {
                  .label = "staging_buffer",
                  .usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
                  .size = numbers_size,
                  .mappedAtCreation = false,
              };
    std::cout << "DEBUG: creating buffer" << std::endl;
    auto staging_buffer = wgpuDeviceCreateBuffer(device.device, &bufd);
    c2sBuf->Flush();
    WGPUBufferDescriptor buf2d = {
                  .label = "storage_buffer",
                  .usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst |
                           WGPUBufferUsage_CopySrc,
                  .size = numbers_size,
                  .mappedAtCreation = false,
              };
    std::cout << "DEBUG: creating buffer 2" << std::endl;
    auto storage_buffer = wgpuDeviceCreateBuffer(device.device, &buf2d);
    c2sBuf->Flush();
    WGPUComputePipelineDescriptor pipd = {
                  .label = "compute_pipeline",
                  .compute =
                      (const WGPUProgrammableStageDescriptor){
                          .module = smod,
                          .entryPoint = "main",
                      },
              };
    auto compute_pipeline = wgpuDeviceCreateComputePipeline(device.device, &pipd);
    c2sBuf->Flush();
    std::cout << "DEBUG: getting bind group layout" << std::endl;
    auto bind_group_layout =
      wgpuComputePipelineGetBindGroupLayout(compute_pipeline, 0);
    c2sBuf->Flush();

    WGPUBindGroupEntry bge[] = {
                          (const WGPUBindGroupEntry){
                              .binding = 0,
                              .buffer = storage_buffer,
                              .offset = 0,
                              .size = numbers_size,
                          },
                      };
    WGPUBindGroupDescriptor bgd = {
                  .label = "bind_group",
                  .layout = bind_group_layout,
                  .entryCount = 1,
                  .entries = bge,
              };
    std::cout << "DEBUG: creating bind group" << std::endl;
    auto bind_group = wgpuDeviceCreateBindGroup(device.device, &bgd);
    c2sBuf->Flush();

    WGPUCommandEncoderDescriptor cedesc = {
        .label = "command_encoder",
    };
    auto cenc = wgpuDeviceCreateCommandEncoder(device.device, &cedesc);
    c2sBuf->Flush();
    if (cenc == nullptr) {
        std::cout << "encoder is null" << std::endl;
    } else {
        std::cout << "encoder is not null" << std::endl;
    }

    WGPUComputePassDescriptor cpdesc = {
        .label = "compute_pass",
    };
    auto compute_pass_encoder = wgpuCommandEncoderBeginComputePass(cenc, &cpdesc);
    c2sBuf->Flush();
    wgpuComputePassEncoderSetPipeline(compute_pass_encoder, compute_pipeline);
    c2sBuf->Flush();
    wgpuComputePassEncoderSetBindGroup(compute_pass_encoder, 0, bind_group, 0, NULL);
    c2sBuf->Flush();
    wgpuComputePassEncoderDispatchWorkgroups(compute_pass_encoder, numbers_length, 1, 1);
    c2sBuf->Flush();
    wgpuComputePassEncoderEnd(compute_pass_encoder);
    c2sBuf->Flush();

    wgpuCommandEncoderCopyBufferToBuffer(cenc, storage_buffer, 0, staging_buffer, 0, numbers_size);
    c2sBuf->Flush();
    WGPUCommandBufferDescriptor cbufd = {
                             .label = "command_buffer",
                         };

    auto command_buffer = wgpuCommandEncoderFinish(cenc, &cbufd);
    c2sBuf->Flush();

    wgpuQueueWriteBuffer(queue, storage_buffer, 0, &numbers, numbers_size);
    c2sBuf->Flush();
    wgpuQueueSubmit(queue, 1, &command_buffer);
    c2sBuf->Flush();

    // wgpuDeviceTick(device.device);
    // c2sBuf->Flush();

    // // bool queueDone = false;
    // // wgpuQueueOnSubmittedWorkDone(queue, 0xffffffff, handle_queue_done, &queueDone);
    // // c2sBuf->Flush();
    // // std::cout << numbers[0] << std::endl;
    // // std::cout << "DEBUG: will wait for queue" << std::endl;
    // // {
    // //     std::unique_lock lk(m);
    // //     cv.wait(lk, [&] { return queueDone; });
    // // }

    // wgpuDeviceTick(device.device);

    bool done = false;
    wgpuBufferMapAsync(staging_buffer, WGPUMapMode_Read, 0, numbers_size,
                       handle_buffer_map, &done);
    c2sBuf->Flush();
    //wgpuDevicePoll(device.device, true, NULL);
    wgpuDeviceTick(device.device);
    c2sBuf->Flush();
    std::cout << "DEBUG: will wait for buffer" << std::endl;
    while (!done) {
        using namespace std::chrono_literals;
        std::unique_lock lk(m);
        cv.wait_for(lk, 1us, [&] { return done; });
        wgpuDeviceTick(device.device);
    }

    std::cout << "DEBUG: about to get result" << std::endl;

    auto buf = (uint32_t *)wgpuBufferGetConstMappedRange(staging_buffer, 0, numbers_size);
    c2sBuf->Flush();
    if (buf == nullptr) {
        std::cout << "RESULT IS NULL" << std::endl;
    } else {
        std::cout << "RESULT: " << buf[0] << ", " << buf[1] << ", " << buf[2] << ", " << buf[3] << std::endl;
    }

    }

    recvt.join();

    return 0;
}
