#pragma once

#include "os/except.h"
#include "os/linux/socket.h"
#include "os/os.h"

namespace looper::os {

static inline looper::error os_error_to_looper(int error) {
    switch (error) {
        case 0:
            return error_success;
        case EAGAIN:
            return error_again;
        case EINPROGRESS:
            return error_in_progress;
        case EINTR:
            return error_interrupted;
        default:
            return -error;
    }
}

static inline looper::error get_call_error() {
    int code = errno;
    return os_error_to_looper(code);
}

static inline void throw_call_error() {
    auto error = get_call_error();
    if (error != error_success) {
        throw os_exception(error);
    }
}

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
    explicit linux_tcp_socket(os::descriptor fd);
    ~linux_tcp_socket() override;

    [[nodiscard]] descriptor get_descriptor() const override;
    [[nodiscard]] error get_internal_error() override;

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

class linux_tcp_server_socket : public tcp_server_socket {
public:
    linux_tcp_server_socket();
    ~linux_tcp_server_socket() override;

    [[nodiscard]] descriptor get_descriptor() const override;
    [[nodiscard]] error get_internal_error() override;

    void close() override;

    void bind(uint16_t port) override;
    void bind(std::string_view ip, uint16_t port) override;

    void listen(size_t backlog_size) override;
    std::shared_ptr<tcp_socket> accept() override;

private:
    int m_fd;
    linux_base_socket m_socket;
};

}