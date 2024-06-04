#ifndef COMMAND_TRANSPORT_H
#define COMMAND_TRANSPORT_H

class RecvBuffer;

class CommandTransport {
  public:
    CommandTransport(int connfd) : mConnfd(connfd) {}
    ~CommandTransport();

    int Send(char* buf, size_t sz);
    int Recv(RecvBuffer* rb);

  private:
    int mConnfd = -1;
};

#endif // COMMAND_TRANSPORT_H
