#include "client_tcp.h"
#include "server_tcp.h"

int createServerListener(RecvBuffer* rbuf) {
    TCPCommandServer tcs;

    auto err = tcs.Init();
    if (err < 0) {
        return err;
    }

    while (1) {
        auto conn = tcs.Accept();
        if (conn == nullptr) {
            continue;
        }

        conn->Recv(rbuf);
        //rbuf->Flush();
        delete(conn);
    }
}

int main() {
    RecvBuffer cmd_buf;
    createServerListener(&cmd_buf);
}
