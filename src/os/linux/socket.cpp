
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

#include <looper_except.h>

#include "os/except.h"
#include "linux.h"
#include "socket.h"

namespace looper::os {

struct {
    int level;
    int opt;
} sockopt_natives[] = {
        {SOL_SOCKET, SO_REUSEPORT},
        {SOL_SOCKET, SO_KEEPALIVE}
};

linux_base_socket::linux_base_socket(int fd)
    : m_fd(fd)
    , m_disabled(false)
    , m_is_blocking(true) {
}

linux_base_socket::~linux_base_socket() {
    close();
}

int linux_base_socket::get_fd() const {
    return m_fd;
}

looper::error linux_base_socket::get_internal_error() const {
    int code;
    socklen_t len = sizeof(code);
    if (::getsockopt(get_fd(), SOL_SOCKET, SO_ERROR, &code, &len)) {
        throw_call_error();
    }

    return os_error_to_looper(code);
}

void linux_base_socket::setoption(sockopt_type opt, void* value, size_t size) {
    throw_if_closed();
    throw_if_disabled();

    const auto sockopt = sockopt_natives[static_cast<size_t>(opt)];

    if (::setsockopt(get_fd(), sockopt.level, sockopt.opt, value, size)) {
        throw_call_error();
    }
}

void linux_base_socket::configure_blocking(bool blocking) {
    throw_if_disabled();

    auto fd = this->get_fd();
    auto flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw_call_error();
    }

    flags = blocking ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK);
    if (fcntl(fd, F_SETFL, flags)) {
        throw_call_error();
    }

    m_is_blocking = blocking;
}

void linux_base_socket::bind(const std::string_view& ip, uint16_t port) {
    throw_if_closed();
    throw_if_disabled();

    std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::bind(get_fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        throw_call_error();
    }
}

void linux_base_socket::bind(uint16_t port) {
    throw_if_closed();
    throw_if_disabled();

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(get_fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        throw_call_error();
    }
}

void linux_base_socket::listen(size_t backlog_size) {
    throw_if_closed();
    throw_if_disabled();

    if (::listen(get_fd(), static_cast<int>(backlog_size))) {
        throw_call_error();
    }
}

int linux_base_socket::accept() {
    throw_if_closed();
    throw_if_disabled();

    sockaddr_in addr{};
    socklen_t addr_len = sizeof(addr);

    const auto new_fd = ::accept(get_fd(), reinterpret_cast<sockaddr*>(&addr), &addr_len);
    if (new_fd < 0) {
        throw_call_error();
    }

    return new_fd;
}

bool linux_base_socket::connect(std::string_view ip, uint16_t port) {
    throw_if_closed();
    throw_if_disabled();

    std::string ip_c(ip);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ::htons(port);
    ::inet_pton(AF_INET, ip_c.c_str(), &addr.sin_addr);

    if (::connect(get_fd(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        const auto error_code = get_call_error();
        if (error_code == error_in_progress && !is_blocking()) {
            // while in non-blocking mode, socket operations may return inprogress as a result
            // to operations they have not yet finished. this is fine.
            disable();
            return false;
        }

        throw_call_error();
    }

    return true;
}

void linux_base_socket::finalize_connect() {
    throw_if_closed();

    enable();

    // for non-blocking connect, we need to make sure it actually succeeded in the end.
    const auto error = get_internal_error();
    if (error != error_success) {
        throw os_exception(error);
    }
}

size_t linux_base_socket::read(uint8_t* buffer, size_t buffer_size) {
    throw_if_closed();
    throw_if_disabled();

    if (buffer_size == 0) {
        return 0;
    }

    const auto result = ::read(get_fd(), buffer, buffer_size);
    if (result == 0) {
        throw eof_exception();
    } else if (result < 0) {
        const auto error_code = get_call_error();
        if (error_code == error_again && !is_blocking()) {
            // while in non-blocking mode, socket operations may return eagain if
            // the operation will end up blocking, as such just return.
            return 0;
        }

        throw_call_error();
    }

    return result;
}

size_t linux_base_socket::write(const uint8_t* buffer, size_t size) {
    throw_if_closed();
    throw_if_disabled();

    const auto result = ::write(get_fd(), buffer, size);
    if (result < 0) {
        throw_call_error();
    }

    return result;
}

void linux_base_socket::close() {
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void linux_base_socket::throw_if_disabled() const {
    if (m_disabled) {
        throw std::runtime_error("socket disabled");
    }
}

void linux_base_socket::throw_if_closed() const {
    if (m_fd < 0) {
        throw closed_fd_exception();
    }
}

}
