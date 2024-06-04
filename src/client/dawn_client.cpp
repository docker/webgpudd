#include <array>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>

#include "dawn_client.h"
#include "../common/dawn_command_buffer.h"

CommandTransport* client::connect_tcp(const std::string& addr, const std::string& port) {
    struct addrinfo *srvaddrs;
    struct addrinfo hints;
    int connfd, ret;

    connfd = socket(AF_INET, SOCK_STREAM, 0);
    if (connfd < 0) {
        std::cerr << "libwebgpu: failed to create TCP socket: " << errno << std::endl;
        return nullptr;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = 0;
    hints.ai_protocol = 0;

    ret = getaddrinfo(addr.c_str(), port.c_str(), &hints, &srvaddrs);
    if (ret < 0) {
        std::cerr << "libwebgpu: failed to get address info: " << ret << std::endl;
        return nullptr;
    }

    if (connect(connfd, srvaddrs->ai_addr, srvaddrs->ai_addrlen) < 0) {
        std::cerr << "libwebgpu: failed to connect: " << errno << std::endl;
        return nullptr;
    }

    return new CommandTransport(connfd);
}

CommandTransport* client::connect_unix(const std::string& socketPath) {
    struct sockaddr_un srv_addr;
    int connfd;

    connfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connfd < 0) {
        std::cerr << "libwebgpu: failed to create unix socket: " << errno << std::endl;
        return nullptr;
    }

    std::memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, socketPath.c_str(), sizeof(srv_addr.sun_path) - 1);

    if (connect(connfd, (struct sockaddr *) &srv_addr, sizeof(srv_addr)) < 0) {
        std::cerr << "libwebgpu: failed to connect: " << errno << std::endl;
        return nullptr;
    }

    return new CommandTransport(connfd);
}
