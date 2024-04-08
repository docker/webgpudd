#include <mutex>
#include <thread>

#include <dawn/dawn_proc.h>
#include <dawn/wire/WireClient.h>

#include "../common/client_tcp.h"
#include "../common/command_buffer.h"

struct webGPUDDRuntime {
    const DawnProcTable *procs;
    dawn::wire::WireClient* wireClient;
    std::unique_ptr<std::thread> recvt;
    SendBuffer* c2sBuf;
    RecvBuffer* s2cBuf;
    dawn::wire::ReservedInstance ri;
    bool instanceReserved;
    TCPCommandClientConnection cmdt;
};

static webGPUDDRuntime runtime;

#ifdef __cplusplus
extern "C" {
#endif

// wgpuCreateInstance
int initWebGPUDD() {
    runtime.procs = &dawn::wire::client::GetProcs();
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

    dawnProcSetProcs(runtime.procs);

    runtime.recvt.reset(new std::thread([&] {
        runtime.cmdt.Recv(runtime.s2cBuf);
    }));
    return 0;
}

// wgpuCreateInstance
WGPUInstance getWebGPUDDInstance() {
    if (!runtime.instanceReserved) {
        runtime.ri = runtime.wireClient->ReserveInstance();
        runtime.instanceReserved = true;
    }
    return runtime.ri.instance;
}

// wgpuInstanceRelease
int finaliseWebGPUDD() {
    runtime.cmdt.~TCPCommandTransport();
    runtime.recvt->join();
    return 0;
}

// TODO: need to figure out where to hide flush, or if it could be avoided altogether
int webGPUDDFlush() {
    return runtime.c2sBuf->Flush();
}

#ifdef __cplusplus
}
#endif
