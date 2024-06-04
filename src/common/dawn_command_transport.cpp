#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "dawn_command_buffer.h"
#include "dawn_command_transport.h"

CommandTransport::~CommandTransport() {
    if (mConnfd >= 0) {
        shutdown(mConnfd, SHUT_RDWR);
        close(mConnfd);
        mConnfd = -1;
    }
}

int CommandTransport::Send(char* buf, size_t sz) {
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

int CommandTransport::Recv(RecvBuffer* rbuf) {
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
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
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
                rbuf->Flush(); // TODO: handle error
                flush_recvd = false;
            }
        }
    }
}
