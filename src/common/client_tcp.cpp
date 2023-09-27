#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#if __linux__
#include <linux/vm_sockets.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "client_tcp.h"
#include "command_buffer.h"

TCPCommandTransport::TCPCommandTransport() noexcept {}

TCPCommandTransport::~TCPCommandTransport() noexcept {
    if (mConnfd >= 0) {
        close(mConnfd);
        mConnfd = -1;
    }
}

int TCPCommandClientConnection::initTCP() {
    struct addrinfo *srvaddrs;
    struct addrinfo hints;
    int connfd, ret;
    const char *webgpudd_addr = std::getenv("WEBGPUDD_ADDR");
    const char *webgpudd_port = std::getenv("WEBGPUDD_PORT");
    if (!webgpudd_addr)
        webgpudd_addr = "host.docker.internal";
    if (!webgpudd_port)
        webgpudd_port = "8080";

    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        return -errno;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    ret = getaddrinfo(webgpudd_addr, webgpudd_port, &hints, &srvaddrs);
    if (ret < 0) {
        return ret;
    }

    if (connect(connfd, srvaddrs->ai_addr, srvaddrs->ai_addrlen) < 0) {
        return -errno;
    }

    mConnfd = connfd;
    return 0;
}

int TCPCommandClientConnection::initUnix() {
    struct sockaddr_un srv_addr;
    int connfd;

    connfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connfd < 0) {
        return -errno;
    }

    std::memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, "/tmp/dd-dawn.sock", sizeof(srv_addr.sun_path) - 1);

    if (connect(connfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        return -errno;
    }

    mConnfd = connfd;
    return 0;
}

#if __linux__
int TCPCommandClientConnection::initVsock() {
    struct sockaddr_vm addr;
    int connfd;

    connfd = socket(AF_VSOCK, SOCK_STREAM, 0);
    if (connfd < 0) {
        return -errno;
    }

    std::memset(&addr, 0, sizeof(struct sockaddr_vm));
    addr.svm_family = AF_VSOCK;
    addr.svm_port = 9000;
    addr.svm_cid = VMADDR_CID_HOST;

    if (connect(connfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        return -errno;
    }

    mConnfd = connfd;
    return 0;
}
#endif

int TCPCommandClientConnection::Init() {
#if __linux__
    return initVsock();
#else
    return initTCP();
#endif
}

int TCPCommandTransport::Send(char* buf, size_t sz) {
    if (mConnfd < 0) {
        return false;
    }

    std::array<char, 65536> send_buf;
    ssize_t nsent;
    size_t soff = 0;
    for (; soff < sz; soff += send_buf.size()) {
        size_t chunksz = sz - soff;
        if (chunksz > send_buf.size()) {
            chunksz = send_buf.size();
        }
        std::memcpy(&send_buf[0], &buf[soff], chunksz);
        nsent = send(mConnfd, &send_buf[0], chunksz, 0);
        if (nsent < 0) {
            return -errno;
        }
    }
    fsync(mConnfd);
    return 0;
}


int TCPCommandTransport::Recv(RecvBuffer* rbuf) {
    ssize_t nread;
    std::array<char, 65536> recv_buf;
    bool recv_cmd = false;
    command_tlv cmd;
    ssize_t cmd_len, cmd_len_left, hdr_len_left = sizeof(command_tlv);
    void* cmd_buf;
    while (1) {
        nread = recv(mConnfd, &recv_buf[0], recv_buf.size(), 0);
        if (nread <= 0) {
            if (nread == 0) {
                return 0;
            }
            return -errno;
        }

        auto flush_recvd = false;
        auto bytes_left = nread;
        while (bytes_left > 0) {
            if (!recv_cmd) {
                if (bytes_left < hdr_len_left) {
                    std::memcpy((char *) &cmd + (sizeof(command_tlv) - hdr_len_left), &recv_buf[nread - bytes_left], bytes_left);
                    hdr_len_left = hdr_len_left - bytes_left;
                    break;
                } else {
                    std::memcpy((char *) &cmd + (sizeof(command_tlv) - hdr_len_left), &recv_buf[nread - bytes_left], hdr_len_left);
                    bytes_left -= hdr_len_left;
                }
                hdr_len_left = sizeof(command_tlv);
                if (cmd.type == 2) {
                    flush_recvd = true;
                    goto flush;
                }
                recv_cmd = true;
                cmd_buf = rbuf->GetCmdSpace(cmd.len);
                auto ncmdbytes = bytes_left;
                cmd_len_left = cmd_len = cmd.len;
                if (ncmdbytes >= (ssize_t) cmd.len) {
                    ncmdbytes = cmd.len;
                    recv_cmd = false;
                }
                std::memcpy(cmd_buf, &recv_buf[nread - bytes_left], ncmdbytes);
                bytes_left -= ncmdbytes;
                cmd_len_left -= ncmdbytes;
            } else {
                auto ncmdbytes = nread;
                if (nread >= cmd_len_left) {
                    ncmdbytes = cmd_len_left;
                    recv_cmd = false;
                }
                std::memcpy((char *)cmd_buf + cmd_len - cmd_len_left, (char *) &recv_buf[0], ncmdbytes);
                bytes_left -= ncmdbytes;
                cmd_len_left -= ncmdbytes;
            }

flush:
            if (flush_recvd) {
                // std::cerr << "flush received" << std::endl;
                rbuf->Flush(); // TODO: handle error
                flush_recvd = false;
            }
        }
    }
}
