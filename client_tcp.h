#ifndef CLIENT_TCP_H
#define CLIENT_TCP_H

#include "command_buffer.h"

class TCPCommandTransport : public CommandTransport {
  public:
    TCPCommandTransport() noexcept;
    ~TCPCommandTransport() noexcept;

    int Send(char* buf, size_t sz) override;
    int Recv(RecvBuffer* rb);

    int mConnfd = -1;
};

class TCPCommandClientConnection : public TCPCommandTransport {
  public:
    int initTCP();
    int initUnix();
    int Init();
};

#endif // CLIENT_TCP_H
