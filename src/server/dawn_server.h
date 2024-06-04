#ifndef DAWN_SERVER_H
#define DAWN_SERVER_H

class CommandTransport;

template <class Server, class ServerConfig>
class CommandServer {
  public:
    int Init(const ServerConfig& cfg) {
        return static_cast<Server*>(this)->init_impl(cfg);
    }

    CommandTransport* Accept() {
        return static_cast<Server*>(this)->accept_impl();
    }
};

struct TCPCommandServerConfig {
    int addr;
    int port;
};

class TCPCommandServer : public CommandServer<TCPCommandServer, TCPCommandServerConfig> {
  public:
    TCPCommandServer() noexcept;
    ~TCPCommandServer() noexcept;

    int init_impl(const TCPCommandServerConfig& cfg);
    CommandTransport* accept_impl();

  private:
    int mListenfd = -1;
};

class FDPassingCommandServer : public CommandServer<FDPassingCommandServer, std::string> {
  public:
    FDPassingCommandServer() noexcept;
    ~FDPassingCommandServer() noexcept;

    int init_impl(const std::string& ctlSocket);
    CommandTransport* accept_impl();

  private:
    int mListenfd = -1;
};

#endif // DAWN_SERVER_H
