
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "os/linux/linux.h"
#include "os/os.h"

namespace looper::os {

static looper::error create_tcp_socket(os::descriptor& descriptor_out) {
    const int fd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

static looper::error create_udp_socket(os::descriptor& descriptor_out) {
    const int fd = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

static looper::error configure_blocking(const os::descriptor descriptor, const bool blocking) {
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

static looper::error setoption(const os::descriptor descriptor, const int level, const int opt, const void* value, const size_t size) {
    if (::setsockopt(descriptor, level, opt, value, size)) {
        return get_call_error();
    }

    return error_success;
}

static looper::error set_default_options(const os::descriptor descriptor) {
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

static looper::error get_socket_error(const os::descriptor descriptor, looper::error& error_out) {
    int code;
    socklen_t len = sizeof(code);
    if (::getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &code, &len)) {
        return get_call_error();
    }

    error_out = os_error_to_looper(code);
    return error_success;
}

looper::error bind_socket_ipv4(os::descriptor descriptor, std::string_view ip, uint16_t port) {
    std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::bind(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

looper::error bind_socket_ipv4(os::descriptor descriptor, uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

namespace tcp {

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

    auto* _tcp = static_cast<tcp*>(malloc(sizeof(tcp)));
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
    return get_socket_error(tcp->fd, error_out);
}

looper::error bind(tcp* tcp, uint16_t port) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return bind_socket_ipv4(tcp->fd, port);
}

looper::error bind(tcp* tcp, std::string_view ip, uint16_t port) {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return bind_socket_ipv4(tcp->fd, ip, port);
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

namespace udp {

struct udp {
    os::descriptor fd;
    bool closed;
};

looper::error create(udp** udp_out) {
    os::descriptor descriptor;
    auto status = create_udp_socket(descriptor);
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

    auto* _udp = static_cast<udp*>(malloc(sizeof(udp)));
    if (_udp == nullptr) {
        ::close(descriptor);
        return error_allocation;
    }

    _udp->fd = descriptor;
    _udp->closed = false;

    *udp_out = _udp;
    return error_success;
}

void close(udp* udp) {
    ::close(udp->fd);

    free(udp);
}

descriptor get_descriptor(udp* udp) {
    return udp->fd;
}

looper::error get_internal_error(udp* udp, looper::error& error_out) {
    return get_socket_error(udp->fd, error_out);
}

looper::error bind(udp* udp, uint16_t port) {
    if (udp->closed) {
        return error_fd_closed;
    }

    return bind_socket_ipv4(udp->fd, port);
}

looper::error bind(udp* udp, std::string_view ip, uint16_t port) {
    if (udp->closed) {
        return error_fd_closed;
    }

    return bind_socket_ipv4(udp->fd, ip, port);
}

looper::error read(udp* udp, uint8_t* buffer, size_t buffer_size, size_t& read_out, char* sender_ip_buff, size_t sender_ip_buff_size, uint16_t& sender_port_out) {
    if (udp->closed) {
        return error_fd_closed;
    }

    if (buffer_size == 0) {
        read_out = 0;
        return error_success;
    }

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    const auto result = ::recvfrom(udp->fd, buffer, buffer_size, 0, reinterpret_cast<sockaddr*>(&addr), &addr_len);
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
    ::inet_ntop(AF_INET, &addr.sin_addr, sender_ip_buff, sender_ip_buff_size);
    sender_port_out = ntohs(addr.sin_port);

    return error_success;
}

looper::error write(udp* udp, std::string_view dest_ip, uint16_t dest_port, const uint8_t* buffer, size_t size, size_t& written_out) {
    if (udp->closed) {
        return error_fd_closed;
    }

    std::string ip_c(dest_ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(dest_port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    const auto result = ::sendto(udp->fd, buffer, size, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result < 0) {
        return get_call_error();
    }

    written_out = result;
    return error_success;
}

}

}