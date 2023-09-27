#include <chrono>
#include <iostream>
#include <string>

#include "command_buffer.h"

SendBuffer::SendBuffer() noexcept {}
SendBuffer::~SendBuffer() noexcept {}

size_t SendBuffer::GetMaximumAllocationSize() const {
    return mBuffer.size() - sizeof(command_tlv);
}

void* SendBuffer::GetCmdSpace(size_t size) {
    // std::cout << "allocating " << size << " bytes for an outgoing command" << std::endl;
    if (size > mBuffer.size()) {
        return nullptr;
    }
    command_tlv* cmd = (command_tlv *) &mBuffer[mOffset];
    cmd->type = 1;
    cmd->len = size;
    char* result = (char *) cmd + sizeof(command_tlv);
    if (mBuffer.size() - size - sizeof(command_tlv) < mOffset) {
        if (!Flush()) {
            return nullptr;
        }
        return GetCmdSpace(size);
    }

    mOffset += size + sizeof(command_tlv);
    return result;
}

bool SendBuffer::Flush() {
    command_tlv* cmd = (command_tlv *) &mBuffer[mOffset];
    cmd->type = 2;
    cmd->len = 0;
    mOffset += sizeof(command_tlv);
    auto result = mTransport->Send(&mBuffer[0], mOffset) == 0;
    mOffset = 0;
    return result;
}

void SendBuffer::SetTransport(CommandTransport* transport) {
    mTransport = transport;
}

RecvBuffer::RecvBuffer() noexcept {}
RecvBuffer::~RecvBuffer() noexcept {}

size_t RecvBuffer::GetMaximumAllocationSize() const {
    return mBuffer.size() - sizeof(command_tlv);
}

void* RecvBuffer::GetCmdSpace(size_t size) {
    if (size > mBuffer.size()) {
        return nullptr;
    }

    char* result = (char *) &mBuffer[mOffset];
    if (mBuffer.size() - size < mOffset) {
        if (!Flush()) {
            return nullptr;
        }
        return GetCmdSpace(size);
    }

    mOffset += size;
    return result;
}

bool RecvBuffer::Flush() {
    auto success = mHandler->HandleCommands((char *)&mBuffer[0], mOffset) != nullptr;
    mOffset = 0;
    return success;
}

void RecvBuffer::SetHandler(dawn::wire::CommandHandler* handler) {
    mHandler = handler;
}
