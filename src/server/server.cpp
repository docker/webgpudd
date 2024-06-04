#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <dawn/wire/WireServer.h>

#include "../common/client_tcp.h"
#include "../common/command_buffer.h"
#include "server_tcp.h"

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

// args for:
// * control socket path
// * fd passing version
// * tcp version
// * vsock version
// * buffer size (?)
//
// webgpudd serve-fd <control socket path>
// webgpudd serve-tcp <port name>
// webgpudd serve-vsock <vsock number>
int main(int argc, char** argv) {
    if (argc < 3) {
        // TODO: print help
        return 1;
    }

    if (std::strncmp(argv[1], "handle-fd", 9) != 0) {
        // TODO: print help
        return 1;
    }
    std::string ctlSocket(argv[2]);

    DawnProcTable backendProcs = dawn::native::GetProcs();

    auto c2sBuf = new RecvBuffer();
    auto s2cBuf = new SendBuffer();

    dawn::wire::WireServerDescriptor serverDesc = {};
    serverDesc.procs = &backendProcs;
    serverDesc.serializer = s2cBuf;

    auto wireServer = new dawn::wire::WireServer(serverDesc);
    DDWGPUServer ddsrv(wireServer, s2cBuf);

    auto wi = std::make_unique<dawn::native::Instance>();
    if (wi == nullptr) {
        std::cerr << "failed to create instance" << std::endl;
        return -1;
    }

    TCPCommandServer tcs;
    int ret = tcs.Init(ctlSocket);
    if (ret) {
        std::cerr << "failed to initialise command server: " << ret << std::endl;
        return ret;
    }

    auto tcsc = tcs.Accept();

    c2sBuf->SetHandler(&ddsrv);
    s2cBuf->SetTransport(tcsc);

    // TODO: need to get id and generation from the client
    const dawn::wire::Handle hnd = {
        .id = 1,
        .generation = 0,
    };
    wireServer->InjectInstance(wi->Get(), hnd);

    std::thread recvt([&] {
        tcsc->Recv(c2sBuf);
    });

    recvt.join();

    return 0;
}
