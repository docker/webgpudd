#ifndef DAWN_CLIENT_H
#define DAWN_CLIENT_H

#include "../common/dawn_command_buffer.h"
#include "../common/dawn_command_transport.h"

namespace client {

CommandTransport* connect_tcp(const std::string& addr, const std::string& port);
CommandTransport* connect_unix(const std::string& socketPath);

} // namespace client

#endif // DAWN_CLIENT_H
