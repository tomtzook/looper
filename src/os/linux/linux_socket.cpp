
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "os/linux/linux.h"
#include "os/os.h"

namespace looper::os::tcp {

static looper::error create_tcp_socket(os::descriptor& descriptor_out) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

static looper::error configure_blocking(os::descriptor descriptor, bool blocking) {
    auto flags = fcntl(descriptor, F_GETFL, 0);
    if (flags == -1) {
        return get_call_error();
    }

    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(descriptor, F_SETFL, flags)) {
        return get_call_error();
    }

    return error_success;
}

static looper::error setoption(os::descriptor descriptor, int level, int opt, void* value, size_t size) {
    if (::setsockopt(descriptor, level, opt, value, size)) {
        return get_call_error();
    }

    return error_success;
}

static looper::error set_default_options(os::descriptor descriptor) {
    int value = 1;
    auto status = setoption(descriptor, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    if (status != error_success) {
        return status;
    }

    status = setoption(descriptor, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
    if (status != error_success) {
        return status;
    }

    return error_success;
}

struct tcp {
    os::descriptor fd;
    bool disabled;
    bool closed;
};

looper::error create(tcp** tcp_out) {
    os::descriptor descriptor;
    auto status = create_tcp_socket(descriptor);
    if (status != error_success) {
        return status;
    }

    status = configure_blocking(descriptor, false);
    if (status != error_success) {
        ::close(descriptor);
        return status;
    }

    status = set_default_options(descriptor);
    if (status != error_success) {
        ::close(descriptor);
        return status;
    }

    auto* _tcp = reinterpret_cast<tcp*>(malloc(sizeof(tcp)));
    if (_tcp == nullptr) {
        ::close(descriptor);
        return error_allocation;
    }

    _tcp->fd = descriptor;
    _tcp->disabled = false;
    _tcp->closed = false;

    *tcp_out = _tcp;
    return error_success;
}

void close(tcp* tcp) {
    ::close(tcp->fd);

    free(tcp);
}

descriptor get_descriptor(tcp* tcp) {
    return tcp->fd;
}

looper::error get_internal_error(tcp* tcp, looper::error& error_out) {
    int code;
    socklen_t len = sizeof(code);
    if (::getsockopt(tcp->fd, SOL_SOCKET, SO_ERROR, &code, &len)) {
        return get_call_error();
    }

    error_out = os_error_to_looper(code);
    return error_success;
}

looper::error bind(tcp* tcp, uint16_t port) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(tcp->fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

looper::error bind(tcp* tcp, std::string_view ip, uint16_t port) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::bind(tcp->fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

looper::error connect(tcp* tcp, std::string_view ip, uint16_t port) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::connect(tcp->fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        const auto error_code = get_call_error();
        if (error_code == error_in_progress) {
            // while in non-blocking mode, socket operations may return inprogress as a result
            // to operations they have not yet finished. this is fine.
            tcp->disabled = true;
            return error_in_progress;
        }

        return get_call_error();
    }

    return error_success;
}

looper::error finalize_connect(tcp* tcp) {
    if (tcp->closed) {
        return error_fd_closed;
    }

    tcp->disabled = false;

    // for non-blocking connect, we need to make sure it actually succeeded in the end.
    looper::error error;
    auto status = get_internal_error(tcp, error);
    if (status != error_success) {
        return status;
    }
    if (error != error_success) {
        return error;
    }

    return error_success;
}

looper::error read(tcp* tcp, uint8_t* buffer, size_t buffer_size, size_t& read_out) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    if (buffer_size == 0) {
        read_out = 0;
        return error_success;
    }

    const auto result = ::read(tcp->fd, buffer, buffer_size);
    if (result == 0) {
        return error_eof;
    }
    if (result < 0) {
        const auto error_code = get_call_error();
        if (error_code == error_again) {
            // while in non-blocking mode, socket operations may return eagain if
            // the operation will end up blocking, as such just return.
            read_out = 0;
            return error_success;
        }

        return get_call_error();
    }

    read_out = result;
    return error_success;
}

looper::error write(tcp* tcp, const uint8_t* buffer, size_t size, size_t& written_out) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    const auto result = ::write(tcp->fd, buffer, size);
    if (result < 0) {
        return get_call_error();
    }

    written_out = result;
    return error_success;
}

looper::error listen(tcp* tcp, size_t backlog_size) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    if (::listen(tcp->fd, static_cast<int>(backlog_size))) {
        return get_call_error();
    }

    return error_success;
}

looper::error accept(tcp* this_tcp, tcp** tcp_out) {
    if (this_tcp->closed) {
        return error_fd_closed;
    }
    if (this_tcp->disabled) {
        return error_operation_not_supported;
    }

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);

    const auto new_fd = ::accept(this_tcp->fd, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (new_fd < 0) {
        return get_call_error();
    }

    auto* _new_tcp = reinterpret_cast<tcp*>(malloc(sizeof(tcp)));
    if (_new_tcp == nullptr) {
        ::close(new_fd);
        return error_allocation;
    }

    auto status = configure_blocking(new_fd, false);
    if (status != error_success) {
        ::close(new_fd);
        return status;
    }

    _new_tcp->fd = new_fd;
    _new_tcp->disabled = false;
    _new_tcp->closed = false;

    *tcp_out = _new_tcp;
    return error_success;
}

}