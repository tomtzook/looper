
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <new>

#ifdef LOOPER_UNIX_SOCKETS
#include <sys/un.h>
#endif

#include "os/linux/linux.h"
#include "os/os_interface.h"

namespace looper::os::interface {

namespace detail {


looper::error configure_blocking(const os::descriptor descriptor, const bool blocking) {
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

looper::error setoption(
    const os::descriptor descriptor,
    const int level,
    const int opt,
    const void* value,
    const size_t size) {
    if (::setsockopt(descriptor, level, opt, value, size)) {
        return get_call_error();
    }

    return error_success;
}

looper::error set_default_options(const os::descriptor descriptor) {
    const int value = 1;
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

looper::error get_socket_error(const os::descriptor descriptor, looper::error& error_out) {
    int code;
    socklen_t len = sizeof(code);
    if (::getsockopt(descriptor, SOL_SOCKET, SO_ERROR, &code, &len)) {
        return get_call_error();
    }

    error_out = os_error_to_looper(code);
    return error_success;
}

looper::error bind_socket_ipv4(const os::descriptor descriptor, const std::string_view ip, const uint16_t port) {
    const std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::bind(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

looper::error bind_socket_ipv4(const os::descriptor descriptor, const uint16_t port) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

looper::error connect_socket_ipv4(const os::descriptor descriptor, const std::string_view ip, const uint16_t port) {
    const std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::connect(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        const auto error_code = get_call_error();
        if (error_code == error_in_progress) {
            return error_in_progress;
        }

        return get_call_error();
    }

    return error_success;
}

looper::error finalize_connect_socket(const os::descriptor descriptor) {
    // for non-blocking connect, we need to make sure it actually succeeded in the end.
    looper::error error;
    const auto status = get_socket_error(descriptor, error);
    if (status != error_success) {
        return status;
    }
    if (error != error_success) {
        return error;
    }

    return error_success;
}

looper::error read_socket_stream(const os::descriptor descriptor, uint8_t* buffer, const size_t buffer_size, size_t& read_out) {
    size_t read_count;
    const auto status = io_read(descriptor, buffer, buffer_size, read_count);
    if (status != error_success) {
        return status;
    }

    read_out = read_count;

    return error_success;
}

looper::error write_socket_stream(const os::descriptor descriptor, const uint8_t* buffer, const size_t buffer_size, size_t& written_out) {
    size_t written;
    const auto status = io_write(descriptor, buffer, buffer_size, written);
    if (status != error_success) {
        return status;
    }

    written_out = written;
    return error_success;
}

looper::error readfrom_socket_dgram(
    const os::descriptor descriptor,
    uint8_t* buffer,
    const size_t buffer_size,
    size_t& read_out,
    char* sender_ip_buff,
    const size_t sender_ip_buff_size,
    uint16_t& sender_port_out) {
    if (buffer_size == 0) {
        read_out = 0;
        return error_success;
    }

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);
    const auto result = ::recvfrom(
        descriptor,
        buffer,
        buffer_size,
        0,
        reinterpret_cast<sockaddr*>(&addr),
        &addr_len);
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

looper::error writeto_socket_dgram(
    const os::descriptor descriptor,
    const std::string_view dest_ip,
    const uint16_t dest_port,
    const uint8_t* buffer,
    const size_t size,
    size_t& written_out) {
    const std::string ip_c(dest_ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(dest_port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    const auto result = ::sendto(
        descriptor,
        buffer,
        size,
        0,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr));
    if (result < 0) {
        return get_call_error();
    }

    written_out = result;
    return error_success;
}

looper::error listen_socket(const os::descriptor descriptor, const size_t backlog_size) {
    if (::listen(descriptor, static_cast<int>(backlog_size))) {
        return get_call_error();
    }

    return error_success;
}

looper::error accept_socket(const os::descriptor descriptor, os::descriptor& descriptor_out) {
    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);

    const auto new_fd = ::accept(descriptor, reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (new_fd < 0) {
        return get_call_error();
    }

    const auto status = detail::configure_blocking(new_fd, false);
    if (status != error_success) {
        ::close(new_fd);
        return status;
    }

    descriptor_out = new_fd;
    return error_success;
}

struct base_socket {
    os::descriptor fd;
    bool closed;
};

template<typename T>
looper::error accept_socket_strt(const os::descriptor descriptor, T** skt_out) {
    auto* _new_skt = new (std::nothrow) T;
    if (_new_skt == nullptr) {
        return error_allocation;
    }

    os::descriptor new_fd;
    const auto status = detail::accept_socket(descriptor, new_fd);
    if (status != error_success) {
        delete _new_skt;
        return status;
    }

    reinterpret_cast<base_socket*>(_new_skt)->fd = new_fd;
    reinterpret_cast<base_socket*>(_new_skt)->closed = false;

    *skt_out = _new_skt;
    return error_success;
}

template<typename T>
looper::error create_new_socket(const int domain, const int type, const int protocol, const bool set_options, T** skt_out) {
    const int fd = ::socket(domain, type, protocol);
    if (fd < 0) {
        return get_call_error();
    }

    const os::descriptor descriptor = fd;

    auto status = detail::configure_blocking(descriptor, false);
    if (status != error_success) {
        ::close(descriptor);
        return status;
    }

    if (set_options) {
        status = detail::set_default_options(descriptor);
        if (status != error_success) {
            ::close(descriptor);
            return status;
        }
    }

    auto* _strt = new (std::nothrow) T;
    if (_strt == nullptr) {
        ::close(descriptor);
        return error_allocation;
    }

    reinterpret_cast<base_socket*>(_strt)->fd = descriptor;
    reinterpret_cast<base_socket*>(_strt)->closed = false;

    *skt_out = _strt;
    return error_success;
}

void close_socket(base_socket* skt) {
    skt->closed = true;
    ::close(skt->fd);
    skt->fd = -1;

    delete skt;
}

#ifdef LOOPER_UNIX_SOCKETS

looper::error bind_socket_unix(const os::descriptor descriptor, const std::string_view path) {
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.data());

    if (::bind(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        return get_call_error();
    }

    return error_success;
}

looper::error connect_socket_unix(const os::descriptor descriptor, const std::string_view path) {
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path.data());

    if (::connect(descriptor, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        const auto error_code = get_call_error();
        if (error_code == error_in_progress) {
            return error_in_progress;
        }

        return get_call_error();
    }

    return error_success;
}

#endif

}

namespace tcp {

struct tcp : public detail::base_socket {
    bool disabled;
};

looper::error create(tcp** tcp_out) noexcept {
    tcp* _tcp;
    const auto status = detail::create_new_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, true, &_tcp);
    if (status != error_success) {
        return status;
    }

    _tcp->disabled = false;

    *tcp_out = _tcp;
    return error_success;
}

void close(tcp* tcp) noexcept {
    detail::close_socket(tcp);
}

descriptor get_descriptor(const tcp* tcp) noexcept {
    return tcp->fd;
}

looper::error get_internal_error(const tcp* tcp, looper::error& error_out) noexcept {
    return detail::get_socket_error(tcp->fd, error_out);
}

looper::error bind(const tcp* tcp, const uint16_t port) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return detail::bind_socket_ipv4(tcp->fd, port);
}

looper::error bind(const tcp* tcp, const std::string_view ip, const uint16_t port) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return detail::bind_socket_ipv4(tcp->fd, ip, port);
}

looper::error connect(tcp* tcp, const std::string_view ip, const uint16_t port) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    const auto status = detail::connect_socket_ipv4(tcp->fd, ip, port);
    if (status == error_in_progress) {
        // while in non-blocking mode, socket operations may return inprogress as a result
        // to operations they have not yet finished. this is fine.
        tcp->disabled = true;
    }

    return status;
}

looper::error finalize_connect(tcp* tcp) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }

    tcp->disabled = false;
    return detail::finalize_connect_socket(tcp->fd);
}

looper::error read(const tcp* tcp, uint8_t* buffer, const size_t buffer_size, size_t& read_out) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return detail::read_socket_stream(tcp->fd, buffer, buffer_size, read_out);
}

looper::error write(const tcp* tcp, const uint8_t* buffer, const size_t size, size_t& written_out) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return detail::write_socket_stream(tcp->fd, buffer, size, written_out);
}

looper::error listen(const tcp* tcp, const size_t backlog_size) noexcept {
    if (tcp->closed) {
        return error_fd_closed;
    }
    if (tcp->disabled) {
        return error_operation_not_supported;
    }

    return detail::listen_socket(tcp->fd, backlog_size);
}

looper::error accept(const tcp* this_tcp, tcp** tcp_out) noexcept {
    if (this_tcp->closed) {
        return error_fd_closed;
    }
    if (this_tcp->disabled) {
        return error_operation_not_supported;
    }

    tcp* _new_tcp;
    const auto status = detail::accept_socket_strt(this_tcp->fd, &_new_tcp);
    if (status != error_success) {
        return status;
    }

    _new_tcp->disabled = false;

    *tcp_out = _new_tcp;
    return error_success;
}

}

namespace udp {

struct udp : public detail::base_socket {
};

looper::error create(udp** udp_out) noexcept {
    udp* _udp;
    const auto status = detail::create_new_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP, true, &_udp);
    if (status != error_success) {
        return status;
    }

    *udp_out = _udp;
    return error_success;
}

void close(udp* udp) noexcept {
    detail::close_socket(udp);
}

descriptor get_descriptor(const udp* udp) noexcept {
    return udp->fd;
}

looper::error get_internal_error(const udp* udp, looper::error& error_out) noexcept {
    return detail::get_socket_error(udp->fd, error_out);
}

looper::error bind(const udp* udp, const uint16_t port) noexcept {
    if (udp->closed) {
        return error_fd_closed;
    }

    return detail::bind_socket_ipv4(udp->fd, port);
}

looper::error bind(const udp* udp, const std::string_view ip, const uint16_t port) noexcept {
    if (udp->closed) {
        return error_fd_closed;
    }

    return detail::bind_socket_ipv4(udp->fd, ip, port);
}

looper::error read(
    const udp* udp,
    uint8_t* buffer,
    const size_t buffer_size,
    size_t& read_out,
    char* sender_ip_buff,
    const size_t sender_ip_buff_size,
    uint16_t& sender_port_out) noexcept {
    if (udp->closed) {
        return error_fd_closed;
    }

    return detail::readfrom_socket_dgram(udp->fd, buffer, buffer_size, read_out, sender_ip_buff, sender_ip_buff_size, sender_port_out);
}

looper::error write(
    const udp* udp,
    const std::string_view dest_ip,
    const uint16_t dest_port,
    const uint8_t* buffer,
    const size_t size,
    size_t& written_out) noexcept {
    if (udp->closed) {
        return error_fd_closed;
    }

    return detail::writeto_socket_dgram(udp->fd, dest_ip, dest_port, buffer, size, written_out);
}

}

#ifdef LOOPER_UNIX_SOCKETS

namespace unix_sock {

struct unix_socket : public detail::base_socket {
    bool disabled;
};

looper::error create(unix_socket** skt_out) noexcept {
    unix_socket* _skt;
    const auto status = detail::create_new_socket(AF_UNIX, SOCK_STREAM, 0, false, &_skt);
    if (status != error_success) {
        return status;
    }

    _skt->disabled = false;

    *skt_out = _skt;
    return error_success;
}

void close(unix_socket* skt) noexcept {
    detail::close_socket(skt);
}

descriptor get_descriptor(const unix_socket* skt) noexcept {
    return skt->fd;
}

looper::error bind(const unix_socket* skt, const std::string_view path) noexcept {
    if (skt->closed) {
        return error_fd_closed;
    }
    if (skt->disabled) {
        return error_operation_not_supported;
    }

    return detail::bind_socket_unix(skt->fd, path);
}

looper::error connect(unix_socket* skt, const std::string_view path) noexcept {
    if (skt->closed) {
        return error_fd_closed;
    }
    if (skt->disabled) {
        return error_operation_not_supported;
    }

    const auto status = detail::connect_socket_unix(skt->fd, path);
    if (status == error_in_progress) {
        // while in non-blocking mode, socket operations may return inprogress as a result
        // to operations they have not yet finished. this is fine.
        skt->disabled = true;
    }

    return status;
}

looper::error finalize_connect(unix_socket* skt) noexcept {
    if (skt->closed) {
        return error_fd_closed;
    }

    skt->disabled = false;
    return detail::finalize_connect_socket(skt->fd);
}

looper::error read(const unix_socket* skt, uint8_t* buffer, const size_t buffer_size, size_t& read_out) noexcept {
    if (skt->closed) {
        return error_fd_closed;
    }
    if (skt->disabled) {
        return error_operation_not_supported;
    }

    return detail::read_socket_stream(skt->fd, buffer, buffer_size, read_out);
}

looper::error write(const unix_socket* skt, const uint8_t* buffer, const size_t size, size_t& written_out) noexcept {
    if (skt->closed) {
        return error_fd_closed;
    }
    if (skt->disabled) {
        return error_operation_not_supported;
    }

    return detail::write_socket_stream(skt->fd, buffer, size, written_out);
}

looper::error listen(const unix_socket* skt, const size_t backlog_size) noexcept {
    if (skt->closed) {
        return error_fd_closed;
    }
    if (skt->disabled) {
        return error_operation_not_supported;
    }

    return detail::listen_socket(skt->fd, backlog_size);
}

looper::error accept(const unix_socket* this_skt, unix_socket** skt_out) noexcept {
    if (this_skt->closed) {
        return error_fd_closed;
    }
    if (this_skt->disabled) {
        return error_operation_not_supported;
    }

    unix_socket* _new_skt;
    const auto status = detail::accept_socket_strt(this_skt->fd, &_new_skt);
    if (status != error_success) {
        return status;
    }

    _new_skt->disabled = false;

    *skt_out = _new_skt;
    return error_success;
}

}

#endif

}
