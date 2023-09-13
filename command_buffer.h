#ifndef COMMAND_BUFFER_H
#define COMMAND_BUFFER_H

#include <array>

#include <dawn/wire/Wire.h>

struct __attribute__((packed)) command_tlv {
    int type;
    size_t len;
};

class CommandTransport {
  public:
    virtual int Send(char* buf, size_t sz) = 0;
};

class SendBuffer final : public dawn::wire::CommandSerializer {
  public:
    SendBuffer() noexcept;
    ~SendBuffer() noexcept;

    size_t GetMaximumAllocationSize() const override;

    void* GetCmdSpace(size_t size) override;
    bool Flush() override;

    void SetTransport(CommandTransport* transport);

  private:
    CommandTransport* mTransport = nullptr;
    size_t mOffset = 0;
    std::array<char, 1000000> mBuffer;
};

class RecvBuffer final : public dawn::wire::CommandSerializer {
  public:
    RecvBuffer() noexcept;
    ~RecvBuffer() noexcept;

    size_t GetMaximumAllocationSize() const override;

    void* GetCmdSpace(size_t size) override;
    bool Flush() override;

    void SetHandler(dawn::wire::CommandHandler* handler);

  private:
    dawn::wire::CommandHandler* mHandler = nullptr;
    size_t mOffset = 0;
    std::array<char, 1000000> mBuffer;
};

#endif  // COMMAND_BUFFER_H
