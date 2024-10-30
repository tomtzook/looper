
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>

#include <looper_except.h>
#include "linux.h"


namespace looper::os {

static os::descriptor create_eventfd() {
    auto fd = eventfd(0, EFD_NONBLOCK);
    if (fd < 0) {
        throw_call_error();
    }

    return fd;
}

static int create_tcp_socket() {
    int m_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_fd < 0) {
        throw_call_error();
    }

    return m_fd;
}

linux_event::linux_event()
    : m_fd(create_eventfd())
{}

linux_event::~linux_event() {
    ::close(m_fd);
}

descriptor linux_event::get_descriptor() const {
    return m_fd;
}

void linux_event::set() {
    if (::eventfd_write(get_descriptor(), 1)) {
        throw_call_error();
    }
}

void linux_event::clear() {
    eventfd_t value;
    if (::eventfd_read(get_descriptor(), &value)) {
        throw_call_error();
    }
}

linux_tcp_socket::linux_tcp_socket()
    : linux_tcp_socket(create_tcp_socket()) {

}

linux_tcp_socket::linux_tcp_socket(os::descriptor fd)
    : m_fd(fd)
    , m_socket(m_fd) {
    m_socket.configure_blocking(false);
    m_socket.setoption<sockopt_reuseport>(true);
    m_socket.setoption<sockopt_keepalive>(true);
}

linux_tcp_socket::~linux_tcp_socket() {
    m_socket.close();
}

descriptor linux_tcp_socket::get_descriptor() const {
    return m_fd;
}

error linux_tcp_socket::get_internal_error() {
    return m_socket.get_internal_error();
}

void linux_tcp_socket::close() {
    m_socket.close();
}

void linux_tcp_socket::bind(uint16_t port) {
    m_socket.bind(port);
}

bool linux_tcp_socket::connect(std::string_view ip, uint16_t port) {
    return m_socket.connect(ip, port);
}

void linux_tcp_socket::finalize_connect() {
    m_socket.finalize_connect();
}

size_t linux_tcp_socket::read(uint8_t* buffer, size_t buffer_size) {
    return m_socket.read(buffer, buffer_size);
}

size_t linux_tcp_socket::write(const uint8_t* buffer, size_t size) {
    return m_socket.write(buffer, size);
}


linux_tcp_server_socket::linux_tcp_server_socket()
    : m_fd(create_tcp_socket())
    , m_socket(m_fd) {
    m_socket.configure_blocking(false);
    m_socket.setoption<sockopt_reuseport>(true);
    m_socket.setoption<sockopt_keepalive>(true);
}

linux_tcp_server_socket::~linux_tcp_server_socket() {
    m_socket.close();
}

descriptor linux_tcp_server_socket::get_descriptor() const {
    return m_fd;
}

error linux_tcp_server_socket::get_internal_error() {
    return m_socket.get_internal_error();
}

void linux_tcp_server_socket::close() {
    m_socket.close();
}

void linux_tcp_server_socket::bind(uint16_t port) {
    m_socket.bind(port);
}

void linux_tcp_server_socket::bind(std::string_view ip, uint16_t port) {
    m_socket.bind(ip, port);
}

void linux_tcp_server_socket::listen(size_t backlog_size) {
    m_socket.listen(backlog_size);
}

std::shared_ptr<tcp_socket> linux_tcp_server_socket::accept() {
    const auto fd = m_socket.accept();
    return std::make_shared<linux_tcp_socket>(fd);
}

}
