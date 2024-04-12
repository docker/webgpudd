#include <mutex>
#include <thread>
#include <iostream>
#include <map>
#include <functional>

#include <dawn/dawn_proc_table.h>
#include <dawn/wire/WireClient.h>

#include "../common/client_tcp.h"
#include "../common/command_buffer.h"

void webGPUDDSetProcs(const DawnProcTable*);

void webgpuDDInstanceReference(WGPUInstance instance);
void webGPUDDInstanceRelease(WGPUInstance instance);

void webGPUDDInstanceProcessEvents(WGPUInstance instance);
void webGPUDDInstanceRequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallback callback, void * userdata);
WGPUFuture webGPUDDInstanceRequestAdapterF(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo);
void webGPUDDAdapterRequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallback callback, void * userdata);
WGPUFuture webGPUDDAdapterRequestDeviceF(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallbackInfo callbackInfo);

WGPUAdapter webGPUDDDeviceGetAdapter(WGPUDevice device);
WGPUInstance webGPUDDAdapterGetInstance(WGPUAdapter adapter);

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
    TCPCommandClientConnection cmdt;
};

static webGPUDDRuntime runtime;

int initWebGPUDD() {
    runtime.wire_procs = &dawn::wire::client::GetProcs();
    webGPUDD_procs = dawn::wire::client::GetProcs();
    runtime.c2sBuf = new SendBuffer();
    runtime.s2cBuf = new RecvBuffer();

    dawn::wire::WireClientDescriptor clientDesc = {};
    clientDesc.serializer = runtime.c2sBuf;

    runtime.wireClient = new dawn::wire::WireClient(clientDesc);

    int err = runtime.cmdt.Init();
    if (err < 0) {
        return err;
    }

    runtime.s2cBuf->SetHandler(runtime.wireClient);
    runtime.c2sBuf->SetTransport(&runtime.cmdt);

    webGPUDD_procs.instanceReference = webgpuDDInstanceReference;
    webGPUDD_procs.instanceRelease = webGPUDDInstanceRelease;

    webGPUDD_procs.instanceProcessEvents = webGPUDDInstanceProcessEvents;
    webGPUDD_procs.instanceRequestAdapter = webGPUDDInstanceRequestAdapter;
    webGPUDD_procs.instanceRequestAdapterF = webGPUDDInstanceRequestAdapterF;
    webGPUDD_procs.adapterRequestDevice = webGPUDDAdapterRequestDevice;
    webGPUDD_procs.adapterRequestDeviceF = webGPUDDAdapterRequestDeviceF;

    webGPUDD_procs.deviceGetAdapter = webGPUDDDeviceGetAdapter;
    webGPUDD_procs.adapterGetInstance = webGPUDDAdapterGetInstance;

    webGPUDDSetProcs(&webGPUDD_procs);

    runtime.recvt.reset(new std::thread([&] {
        runtime.cmdt.Recv(runtime.s2cBuf);
    }));
    return 0;
}

WGPUInstance getWebGPUDDInstance() {
    if (!runtime.instanceReserved) {
        runtime.ri = runtime.wireClient->ReserveInstance();
        // FIXME: need to send id and generation to server
        runtime.instanceReserved = true;
    }
    return runtime.ri.instance;
}

int finaliseWebGPUDD() {
    runtime.cmdt.TCPCommandClientConnection::~TCPCommandClientConnection();
    runtime.recvt->join();
    return 0;
}

// TODO: need to figure out where to hide flush, or if it could be avoided altogether
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
    int ret = initWebGPUDD();
    if (ret)
        return nullptr;
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

// TODO: request and process events should flush?
// * wgpuInstanceWaitAny?

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
