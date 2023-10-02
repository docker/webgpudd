#ifndef WGPU_UTIL_H
#define WGPU_UTIL_H

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

WGPUInstance getWGPUInstance() {
#ifdef BACKEND_DAWN_NATIVE
    DawnProcTable procs = dawn::native::GetProcs();
    dawnProcSetProcs(&procs);
    WGPUInstanceDescriptor instanceDesc = {0};
    auto instance = wgpuCreateInstance(&instanceDesc);
#endif /* BACKEND_DAWN_NATIVE */

#ifdef BACKEND_WEBGPUDD
    int ret;
    ret = initWebGPUDD();
    if (ret) {
        std::cerr << "Failed to initialise the WebGPU client: " << ret << std::endl;
        errno = -ret;
        return nullptr;
    }
    auto instance = getWebGPUDDInstance();
#endif /* BACKEND_WEBGPUDD */

    return instance;
}

WGPUAdapter getWGPUAdapter(WGPUInstance instance) {
    WGPUAdapter adapter = nullptr;
    wgpuInstanceRequestAdapter(instance, nullptr, adapterRequestCb, &adapter);
#ifdef BACKEND_WEBGPUDD
    webGPUDDFlush();
#endif /* BACKEND_WEBGPUDD */
    {
        std::unique_lock lk(m);
        cv.wait(lk, [&] { return adapter != nullptr; });
    }
    return adapter;
}

WGPUDevice getWGPUDevice(WGPUAdapter adapter) {
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
    return device;
}

struct WGPUContext {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
};

int setUpWGPU(WGPUContext &ctx) {
    auto instance = getWGPUInstance();
    if (instance == nullptr) {
        std::cerr << "Failed to create a WebGPU instance" << std::endl;
        return -1;
    }
    auto adapter = getWGPUAdapter(instance);
    if (adapter == nullptr) {
        std::cerr << "Failed to get a WebGPU adapter" << std::endl;
        return -1;
    }
    auto device = getWGPUDevice(adapter);
    if (device == nullptr) {
        std::cerr << "Failed to get a WebGPU device" << std::endl;
        return -1;
    }
    auto queue = wgpuDeviceGetQueue(device);
    if (queue == nullptr) {
        std::cerr << "Failed to obtain device queue" << std::endl;
        return -1;
    }
    ctx.instance = instance;
    ctx.adapter = adapter;
    ctx.device = device;
    ctx.queue = queue;
    return 0;
}

void tearDownWGPU(WGPUContext &ctx) {
    if (ctx.instance) {}
    if (ctx.adapter) {}
    if (ctx.device) {}
    if (ctx.queue) {}
}

#endif /* WGPU_UTIL_H */
