#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/wire/WireServer.h>

#include "client_tcp.h"
#include "command_buffer.h"
#include "server_tcp.h"

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

class DDWGPUServer : public dawn::wire::CommandHandler {
  public:
    DDWGPUServer(dawn::wire::CommandHandler* srv, SendBuffer* buf) : mSrv(srv), mBuf(buf) {}

    const volatile char* HandleCommands(const volatile char* commands, size_t size) override {
        auto ret = mSrv->HandleCommands(commands, size);
        mBuf->Flush();
        return ret;
    }

  private:
    dawn::wire::CommandHandler* mSrv;
    SendBuffer* mBuf;
};

int main(int argc, char** argv) {
    DawnProcTable backendProcs = dawn::native::GetProcs();

    auto c2sBuf = new RecvBuffer();
    auto s2cBuf = new SendBuffer();

    dawn::wire::WireServerDescriptor serverDesc = {};
    serverDesc.procs = &backendProcs;
    serverDesc.serializer = s2cBuf;

    auto wireServer = new dawn::wire::WireServer(serverDesc);
    DDWGPUServer ddsrv(wireServer, s2cBuf);

    // dawnProcSetProcs(&backendProcs);

    // WGPUInstanceDescriptor widesc;
    // auto wi = wgpuCreateInstance(&widesc);

    auto wi = std::make_unique<dawn::native::Instance>();
    if (wi == nullptr) {
        std::cout << "failed to create instance" << std::endl;
        return -1;
    }

    auto as = wi->EnumerateAdapters();
    std::cout << "enumerated " << as.size() << " adapters" << std::endl;

    auto backendAdapter = as[0];
    WGPUDeviceDescriptor ddesc = {};
    auto backendDevice = backendAdapter.CreateDevice(&ddesc);

    backendProcs.deviceSetUncapturedErrorCallback(backendDevice, PrintDeviceError, nullptr);
    backendProcs.deviceSetLoggingCallback(backendDevice, DeviceLogCallback, nullptr);

    TCPCommandServer tcs;
    tcs.Init();
    auto tcsc = tcs.Accept();

    c2sBuf->SetHandler(&ddsrv);
    s2cBuf->SetTransport(tcsc);

    wireServer->InjectInstance(wi->Get(), 1, 0);
    wireServer->InjectDevice(backendDevice, 1, 0);

    std::thread recvt([&] {
        tcsc->Recv(c2sBuf);
    });

    recvt.join();

    return 0;
}
