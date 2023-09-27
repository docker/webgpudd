#ifndef SERVER_TCP_H
#define SERVER_TCP_H

class TCPCommandTransport;

class TCPCommandServerConnection : public TCPCommandTransport {
  public:
    int Init(int connfd);
};

class TCPCommandServer {
  public:
    TCPCommandServer() noexcept;
    ~TCPCommandServer() noexcept;

    int initTCP();
    int initUnix();
    int initDDFd();
    int Init();
    TCPCommandServerConnection* Accept();
    TCPCommandServerConnection* acceptFD();

  private:
    int mListenfd = -1;
};

#endif // SERVER_TCP_H
