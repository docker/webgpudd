#include <chrono>
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

int main(int argc, char** argv) {
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
        std::cout << "failed to create instance" << std::endl;
        return -1;
    }

    TCPCommandServer tcs;
    tcs.Init();

    std::cout << "ready to receive connections" << std::endl;
    auto tcsc = tcs.Accept();

    c2sBuf->SetHandler(&ddsrv);
    s2cBuf->SetTransport(tcsc);

    wireServer->InjectInstance(wi->Get(), 1, 0);

    std::thread recvt([&] {
        tcsc->Recv(c2sBuf);
    });

    recvt.join();

    return 0;
}
