#include <mutex>
#include <thread>
#include <iostream>
#include <map>
#include <functional>

#include <dawn/dawn_proc_table.h>
#include <dawn/wire/WireClient.h>

#include "dawn_client.h"
#include "../common/dawn_command_buffer.h"

void webGPUDDSetProcs(const DawnProcTable*);

void webgpuDDInstanceReference(WGPUInstance instance);
void webGPUDDInstanceRelease(WGPUInstance instance);

void webGPUDDInstanceProcessEvents(WGPUInstance instance);
void webGPUDDInstanceRequestAdapter(WGPUInstance instance,
                                    WGPURequestAdapterOptions const * options,
                                    WGPURequestAdapterCallback callback,
                                    void * userdata);
WGPUFuture webGPUDDInstanceRequestAdapterF(WGPUInstance instance,
                                           WGPURequestAdapterOptions const * options,
                                           WGPURequestAdapterCallbackInfo callbackInfo);
void webGPUDDAdapterRequestDevice(WGPUAdapter adapter,
                                  WGPUDeviceDescriptor const * descriptor,
                                  WGPURequestDeviceCallback callback,
                                  void * userdata);
WGPUFuture webGPUDDAdapterRequestDeviceF(WGPUAdapter adapter,
                                         WGPUDeviceDescriptor const * descriptor,
                                         WGPURequestDeviceCallbackInfo callbackInfo);

WGPUAdapter webGPUDDDeviceGetAdapter(WGPUDevice device);
WGPUInstance webGPUDDAdapterGetInstance(WGPUAdapter adapter);

WGPUInstance webGPUDDCreateInstance(const WGPUInstanceDescriptor* desc);
WGPUInstance webGPUDDCreateInstanceNop(const WGPUInstanceDescriptor* desc);

void webGPUDDQueueWriteBuffer(WGPUQueue queue,
                              WGPUBuffer buffer,
                              uint64_t bufferOffset,
                              void const * data,
                              size_t size);

std::map<WGPUAdapter, WGPUInstance> adapterToInstance;
std::map<WGPUDevice, WGPUAdapter> deviceToAdapter;
DawnProcTable webGPUDD_procs;

struct webGPUDDRuntime {
    const DawnProcTable* wire_procs;
    dawn::wire::WireClient* wireClient;
    std::unique_ptr<std::thread> recvt;
    SendBuffer* c2sBuf;
    RecvBuffer* s2cBuf;
    dawn::wire::ReservedInstance ri;
    bool instanceReserved;
    bool threadRunning;
    CommandTransport* cmdt;
    std::mutex instanceLock;
    bool preinit_done;
};

static webGPUDDRuntime runtime;

#ifdef __cplusplus
extern "C" {
#endif

// webgpudd_preinit does any set up which can survive a fork (note: in general
// libwebgpu is not designed with being forked in mind), this is needed to work
// with runwasi, most importantly so that the connection can be initialised while
// we have a view of the root file system, but will be used later in the container,
// this is also why we need to postpone setting up the receive thread until after
// the fork
int webgpudd_preinit()
{
    runtime.wire_procs = &dawn::wire::client::GetProcs();
    webGPUDD_procs = dawn::wire::client::GetProcs();
    runtime.c2sBuf = new SendBuffer();
    runtime.s2cBuf = new RecvBuffer();

    dawn::wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = runtime.c2sBuf;

    runtime.wireClient = new dawn::wire::WireClient(clientDesc);

    runtime.cmdt = client::connect_unix("/run/host-services/webgpu.sock");
    if (!runtime.cmdt)
        return -1;

    runtime.s2cBuf->SetHandler(runtime.wireClient);
    runtime.c2sBuf->SetTransport(runtime.cmdt);

    webGPUDD_procs.createInstance = webGPUDDCreateInstance;

    webGPUDD_procs.instanceReference = webgpuDDInstanceReference;
    webGPUDD_procs.instanceRelease = webGPUDDInstanceRelease;

    webGPUDD_procs.instanceProcessEvents = webGPUDDInstanceProcessEvents;
    webGPUDD_procs.instanceRequestAdapter = webGPUDDInstanceRequestAdapter;
    webGPUDD_procs.instanceRequestAdapterF = webGPUDDInstanceRequestAdapterF;
    webGPUDD_procs.adapterRequestDevice = webGPUDDAdapterRequestDevice;
    webGPUDD_procs.adapterRequestDeviceF = webGPUDDAdapterRequestDeviceF;

    webGPUDD_procs.deviceGetAdapter = webGPUDDDeviceGetAdapter;
    webGPUDD_procs.adapterGetInstance = webGPUDDAdapterGetInstance;

    webGPUDD_procs.queueWriteBuffer = webGPUDDQueueWriteBuffer;

    webGPUDDSetProcs(&webGPUDD_procs);

    runtime.preinit_done = true;

    return 0;
}

#ifdef __cplusplus
}
#endif

int initWebGPUDD() {
    if (!runtime.preinit_done) {
        int ret = webgpudd_preinit();
        if (ret)
            return ret;
    }

    webGPUDD_procs.createInstance = webGPUDDCreateInstanceNop;

    auto thr = new std::thread([&] {
        runtime.cmdt->Recv(runtime.s2cBuf);
    });
    // TODO: would be cleaner to join on destruct, but detach will do for now
    thr->detach();
    runtime.recvt.reset(thr);
    return 0;
}

WGPUInstance getWebGPUDDInstance() {
    if (!runtime.instanceReserved) {
        runtime.ri = runtime.wireClient->ReserveInstance();
        // TODO: need to send id and generation to server
        runtime.instanceReserved = true;
    }
    return runtime.ri.instance;
}

int finaliseWebGPUDD() {
    runtime.cmdt->CommandTransport::~CommandTransport();
    //runtime.recvt->join();
    return 0;
}

int webGPUDDFlush() {
    return runtime.c2sBuf->Flush();
}

WGPUAdapter webGPUDDDeviceGetAdapter(WGPUDevice device) {
    return deviceToAdapter[device];
}

WGPUInstance webGPUDDAdapterGetInstance(WGPUAdapter adapter) {
    return adapterToInstance[adapter];
}

WGPUInstance webGPUDDCreateInstance(const WGPUInstanceDescriptor* desc) {
    std::lock_guard<std::mutex> guard(runtime.instanceLock);
    int ret = initWebGPUDD();
    if (ret)
        return nullptr;
    return getWebGPUDDInstance();
}

WGPUInstance webGPUDDCreateInstanceNop(const WGPUInstanceDescriptor* desc) {
    std::lock_guard<std::mutex> guard(runtime.instanceLock);
    return getWebGPUDDInstance();
}

void webgpuDDInstanceReference(WGPUInstance instance) {
    // TODO: bump internal refcount
    runtime.wire_procs->instanceReference(instance);
}

void webGPUDDInstanceRelease(WGPUInstance instance) {
    // TODO: maintain refcount and delete if needed
    finaliseWebGPUDD();
    runtime.wire_procs->instanceRelease(instance);
}

void webGPUDDInstanceProcessEvents(WGPUInstance instance) {
    runtime.wire_procs->instanceProcessEvents(instance);
    webGPUDDFlush();
}

struct adapterRequestState {
    WGPUInstance instance;
    WGPURequestAdapterCallback callback;
    void * userdata;
};

void adapterRequestCB(WGPURequestAdapterStatus status, WGPUAdapter adapter, char const * message, void * userdata) {
    struct adapterRequestState * state = (struct adapterRequestState *) userdata;
    if (adapter != nullptr) {
        adapterToInstance[adapter] = state->instance;
    }
    state->callback(status, adapter, message, state->userdata);
    delete state;
}

void webGPUDDInstanceRequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallback callback, void * userdata) {
    struct adapterRequestState * state = new adapterRequestState;
    state->instance = instance;
    state->callback = callback;
    state->userdata = userdata;
    runtime.wire_procs->instanceRequestAdapter(instance, options, adapterRequestCB, state);
    webGPUDDFlush();
}

WGPUFuture webGPUDDInstanceRequestAdapterF(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo) {
    auto future = runtime.wire_procs->instanceRequestAdapterF(instance, options, callbackInfo);
    webGPUDDFlush();
    return future;
}

struct deviceRequestState {
    WGPUAdapter adapter;
    WGPURequestDeviceCallback callback;
    void * userdata;
};

void deviceRequestCB(WGPURequestDeviceStatus status, WGPUDevice device, char const * message, void * userdata) {
    struct deviceRequestState * state = (struct deviceRequestState *) userdata;
    if (device != nullptr) {
        deviceToAdapter[device] = state->adapter;
    }
    state->callback(status, device, message, state->userdata);
    delete state;
}

void webGPUDDAdapterRequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallback callback, void * userdata) {
    struct deviceRequestState * state = new deviceRequestState;
    state->adapter = adapter;
    state->callback = callback;
    state->userdata = userdata;
    runtime.wire_procs->adapterRequestDevice(adapter, descriptor, deviceRequestCB, state);
    webGPUDDFlush();
}

WGPUFuture webGPUDDAdapterRequestDeviceF(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallbackInfo callbackInfo) {
    auto future = runtime.wire_procs->adapterRequestDeviceF(adapter, descriptor, callbackInfo);
    webGPUDDFlush();
    return future;
}

void webGPUDDQueueWriteBuffer(WGPUQueue queue, WGPUBuffer buffer, uint64_t bufferOffset, void const * data, size_t size) {
    runtime.wire_procs->queueWriteBuffer(queue, buffer, bufferOffset, data, size);
    webGPUDDFlush();
}
