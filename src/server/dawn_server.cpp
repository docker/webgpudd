#include <array>
#include <iostream>
#include <string>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "../common/dawn_command_buffer.h"
#include "../common/dawn_command_transport.h"
#include "dawn_server.h"

TCPCommandServer::TCPCommandServer() noexcept {}

TCPCommandServer::~TCPCommandServer() noexcept {
    if (mListenfd >= 0) {
        close(mListenfd);
        mListenfd = -1;
    }
}

int TCPCommandServer::init_impl(const TCPCommandServerConfig& cfg) {
    struct sockaddr_in srv_addr;
    int listenfd;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        return -errno;
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    srv_addr.sin_port = htons(8080);

    if (bind(listenfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        return -errno;
    }

    if (listen(listenfd, 1) < 0) {
        return -errno;
    }

    mListenfd = listenfd;
    return 0;
}

CommandTransport* TCPCommandServer::accept_impl() {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);
    auto connfd = accept(mListenfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (connfd < 0) {
        return nullptr;
    }
    auto tcsc = new CommandTransport(connfd);
    return tcsc;
}

FDPassingCommandServer::FDPassingCommandServer() noexcept {}

FDPassingCommandServer::~FDPassingCommandServer() noexcept {
    if (mListenfd >= 0) {
        close(mListenfd);
        mListenfd = -1;
    }
}

int FDPassingCommandServer::init_impl(const std::string& ctlSocket) {
    struct sockaddr_un srv_addr;
    int connfd;

    connfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connfd < 0) {
        return -errno;
    }

    std::memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, ctlSocket.c_str(), ctlSocket.size());

    if (connect(connfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        return -errno;
    }

    mListenfd = connfd;
    return 0;
}

CommandTransport* FDPassingCommandServer::accept_impl() {
    char buf[512];
    struct iovec e = {buf, 512};
    char cmsg[CMSG_SPACE(sizeof(int))];
    struct msghdr m = {NULL, 0, &e, 1, cmsg, sizeof(cmsg), 0};
    int ret = recvmsg(mListenfd, &m, 0);
    if (ret < 0) {
        return nullptr;
    }
    struct cmsghdr *c = CMSG_FIRSTHDR(&m);
    int cfd = *(int *)CMSG_DATA(c);
    char ack = 0xFD;
    ret = send(mListenfd, &ack, 1, 0);
    if (ret < 0) {
        return nullptr;
    }
    auto tcsc = new CommandTransport(cfd);
    return tcsc;
}
