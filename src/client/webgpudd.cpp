#include <mutex>
#include <thread>

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

// wgpuCreateInstance
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

    webGPUDDSetProcs(&webGPUDD_procs);

    runtime.recvt.reset(new std::thread([&] {
        runtime.cmdt.Recv(runtime.s2cBuf);
    }));
    return 0;
}

// wgpuCreateInstance
WGPUInstance getWebGPUDDInstance() {
    if (!runtime.instanceReserved) {
        runtime.ri = runtime.wireClient->ReserveInstance();
        // FIXME: need to send id and generation to server
        runtime.instanceReserved = true;
    }
    return runtime.ri.instance;
}

// wgpuInstanceRelease
int finaliseWebGPUDD() {
    runtime.cmdt.TCPCommandClientConnection::~TCPCommandClientConnection();
    runtime.recvt->join();
    return 0;
}

// TODO: need to figure out where to hide flush, or if it could be avoided altogether
int webGPUDDFlush() {
    return runtime.c2sBuf->Flush();
}

WGPUInstance webGPUDDCreateInstance(const WGPUInstanceDescriptor* desc) {
    int ret = initWebGPUDD();
    if (ret)
        return nullptr;
    return getWebGPUDDInstance();
}

void webgpuDDInstanceReference(WGPUInstance instance) {
    // TODO: bump internal refcount
    // call wire
    runtime.wire_procs->instanceReference(instance);
}

void webGPUDDInstanceRelease(WGPUInstance instance) {
    // TODO: maintain refcount and delete if needed
    // call wire
    finaliseWebGPUDD();
    runtime.wire_procs->instanceRelease(instance);
}

// TODO: request and process events should flush?
// * wgpuInstanceWaitAny?

void webGPUDDInstanceProcessEvents(WGPUInstance instance) {
    runtime.wire_procs->instanceProcessEvents(instance);
    webGPUDDFlush();
}

void webGPUDDInstanceRequestAdapter(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallback callback, void * userdata) {
    runtime.wire_procs->instanceRequestAdapter(instance, options, callback, userdata);
    webGPUDDFlush();
}

WGPUFuture webGPUDDInstanceRequestAdapterF(WGPUInstance instance, WGPURequestAdapterOptions const * options, WGPURequestAdapterCallbackInfo callbackInfo) {
    auto future = runtime.wire_procs->instanceRequestAdapterF(instance, options, callbackInfo);
    webGPUDDFlush();
    return future;
}

void webGPUDDAdapterRequestDevice(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallback callback, void * userdata) {
    runtime.wire_procs->adapterRequestDevice(adapter, descriptor, callback, userdata);
    webGPUDDFlush();
}

WGPUFuture webGPUDDAdapterRequestDeviceF(WGPUAdapter adapter, WGPUDeviceDescriptor const * descriptor, WGPURequestDeviceCallbackInfo callbackInfo) {
    auto future = runtime.wire_procs->adapterRequestDeviceF(adapter, descriptor, callbackInfo);
    webGPUDDFlush();
    return future;
}
