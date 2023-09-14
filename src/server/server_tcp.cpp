#include <array>
#include <iostream>
#include <string>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "../common/client_tcp.h"
#include "../common/command_buffer.h"
#include "server_tcp.h"

int TCPCommandServerConnection::Init(int connfd) {
    mConnfd = connfd;
    return 0;
}

TCPCommandServer::TCPCommandServer() noexcept {}

TCPCommandServer::~TCPCommandServer() noexcept {
    if (mListenfd >= 0) {
        close(mListenfd);
        mListenfd = -1;
    }
}

int TCPCommandServer::initTCP() {
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

int TCPCommandServer::initUnix() {
    struct sockaddr_un srv_addr;
    int listenfd;

    listenfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenfd < 0) {
        return -errno;
    }

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, "/tmp/dd-dawn.sock", sizeof(srv_addr.sun_path) - 1);

    if (bind(listenfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        return -errno;
    }

    if (listen(listenfd, 1) < 0) {
        return -errno;
    }

    mListenfd = listenfd;
    return 0;
}

int TCPCommandServer::Init() {
    return initTCP();
}

TCPCommandServerConnection* TCPCommandServer::Accept() {
    struct sockaddr_in peer_addr;
    socklen_t peer_addr_size = sizeof(peer_addr);
    auto connfd = accept(mListenfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
    if (connfd < 0) {
        return nullptr;
    }
    auto tcsc = new TCPCommandServerConnection();
    tcsc->Init(connfd);
    return tcsc;
}
