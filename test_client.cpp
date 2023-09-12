#include <iostream>
#include <string>

#include "client_tcp.h"

int main() {
    SendBuffer send_buf;
    RecvBuffer recv_buf;
    char* spc = (char *) send_buf.GetCmdSpace(20);
    std::memcpy(spc, std::string("12345678900987654321").c_str(), 20);
    spc = (char *) send_buf.GetCmdSpace(20);
    std::memcpy(spc, std::string("09876543211234567890").c_str(), 20);

    TCPCommandClientConnection cmdt;
    int err = cmdt.Init();
    if (err < 0) {
        std::cout << "failed to initialise client connection: " << err << std::endl;
        return err;
    }
    send_buf.SetTransport(&cmdt);

    send_buf.Flush();

    return 0;
}
