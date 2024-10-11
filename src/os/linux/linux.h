#pragma once

#include "os/linux/socket.h"
#include "os/os.h"

namespace looper::os {

class linux_event : public event {
public:
    linux_event();
    ~linux_event() override;

    [[nodiscard]] descriptor get_descriptor() const override;

    void set() override;
    void clear() override;

private:
    int m_fd;
};

class linux_tcp_socket : public tcp_socket {
public:
    linux_tcp_socket();
    ~linux_tcp_socket() override;

    [[nodiscard]] descriptor get_descriptor() const override;

    void close() override;

    void bind(uint16_t port) override;

    bool connect(std::string_view ip, uint16_t port) override;
    void finalize_connect() override;

    size_t read(uint8_t* buffer, size_t buffer_size) override;
    size_t write(const uint8_t* buffer, size_t size) override;

private:
    int m_fd;
    linux_base_socket m_socket;
};

}