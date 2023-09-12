#include <array>
#include <cstring>
#include <string>
#include <iostream>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

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
    struct sockaddr_in srv_addr, peer_addr;
    socklen_t peer_addr_size;
    int connfd;

    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        return -errno;
    }

    int yes = 1;
    int result = setsockopt(connfd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes, sizeof(int));

    std::memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    srv_addr.sin_port = htons(8080);

    if (connect(connfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        return -errno;
    }

    mConnfd = connfd;
    return 0;
}

int TCPCommandClientConnection::initUnix() {
    struct sockaddr_un srv_addr, peer_addr;
    socklen_t peer_addr_size;
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

int TCPCommandClientConnection::Init() {
    return initUnix();
}

int TCPCommandTransport::Send(char* buf, size_t sz) {
    if (mConnfd < 0) {
        return false;
    }

    std::array<char, 65536> send_buf;
    ssize_t nsent;
    size_t soff = 0;
    // std::cout << "sending a " << sz << " byte command" << std::endl;
    for (; soff < sz; soff += send_buf.size()) {
        size_t chunksz = sz - soff;
        if (chunksz > send_buf.size()) {
            chunksz = send_buf.size();
        }
        std::memcpy(&send_buf[0], &buf[soff], chunksz);
        nsent = send(mConnfd, &send_buf[0], chunksz, 0);
        if (nsent < 0) {
            // std::cout << "failed to send command: " << errno << std::endl;
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
    size_t cmd_len, cmd_len_left, hdr_len_left = sizeof(command_tlv);
    void* cmd_buf;
    while (1) {
        // std::cout << "about to receive command data on " << mConnfd << std::endl;

        nread = recv(mConnfd, &recv_buf[0], recv_buf.size(), 0);
        if (nread <= 0) {
            if (nread == 0) {
                // std::cout << "peer terminated connection" << std::endl;
                return 0;
            }
            // std::cout << "failed to receive command data: " << nread << " " << strerror(errno) << std::endl;
            return -errno;
        }

        // std::cout << "read " << nread << " byte buffer" << std::endl;

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
                if (ncmdbytes >= cmd.len) {
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
            if (flush_recvd) { // TODO: could also send a "flush" TLV
                rbuf->Flush(); // TODO: handle error
                flush_recvd = false;
            }
        }
    }
}
