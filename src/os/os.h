#pragma once

#include <variant>

#include <looper_except.h>
#include "os/os_interface.h"


#define OS_CHECK_THROW(...) \
    do {                    \
        auto _status = __VA_ARGS__; \
        if (_status != looper::error_success) { \
            throw looper::os_exception(_status); \
        }                                       \
    } while(0);

namespace looper::os {

namespace detail {

struct event_creator {
    interface::event::event* operator()() const {
        interface::event::event* event;
        const auto status = interface::event::create(&event);
        if (status != error_success) {
            throw os_exception(status);
        }

        return event;
    }
};

struct event_deleter {
    void operator()(interface::event::event* event) const {
        interface::event::close(event);
    }
};

struct tcp_creator {
    interface::tcp::tcp* operator()() const {
        interface::tcp::tcp* tcp;
        const auto status = interface::tcp::create(&tcp);
        if (status != error_success) {
            throw os_exception(status);
        }

        return tcp;
    }
};

struct tcp_deleter {
    void operator()(interface::tcp::tcp* tcp) const {
        interface::tcp::close(tcp);
    }
};

struct udp_creator {
    interface::udp::udp* operator()() const {
        interface::udp::udp* udp;
        const auto status = interface::udp::create(&udp);
        if (status != error_success) {
            throw os_exception(status);
        }

        return udp;
    }
};

struct udp_deleter {
    void operator()(interface::udp::udp* udp) const {
        interface::udp::close(udp);
    }
};

struct poller_creator {
    interface::poll::poller* operator()() const {
        interface::poll::poller* poller;
        const auto status = interface::poll::create(&poller);
        if (status != error_success) {
            throw os_exception(status);
        }

        return poller;
    }
};

struct poller_deleter {
    void operator()(interface::poll::poller* poller) const {
        interface::poll::close(poller);
    }
};

#ifdef LOOPER_UNIX_SOCKETS

struct unix_socket_creator {
    interface::unix_sock::unix_socket* operator()() const {
        interface::unix_sock::unix_socket* unix_socket;
        const auto status = interface::unix_sock::create(&unix_socket);
        if (status != error_success) {
            throw os_exception(status);
        }

        return unix_socket;
    }
};

struct unix_socket_deleter {
    void operator()(interface::unix_sock::unix_socket* skt) const {
        interface::unix_sock::close(skt);
    }
};

#endif

}

template<typename t_, typename creator_, typename deleter_>
class os_object {
public:
    using underlying_type = t_;
    using pointer = t_*;
    using smart_ptr = std::unique_ptr<underlying_type, deleter_>;

    os_object() = delete;
    os_object(const os_object&) = delete;
    os_object(os_object&&) = default;
    os_object& operator=(const os_object&) = delete;
    os_object& operator=(os_object&&) = default;

    explicit os_object(smart_ptr&& ptr) : m_ptr(std::move(ptr)) {}

    // ReSharper disable once CppNonExplicitConversionOperator
    operator pointer() const { return m_ptr.get(); } // NOLINT(*-explicit-constructor)

    void reset() { m_ptr.reset(); }

    static os_object create() {
        const auto obj = creator_()();
        return os_object(smart_ptr(obj));
    }

private:
    smart_ptr m_ptr;
};

using event = os_object<interface::event::event, detail::event_creator, detail::event_deleter>;
using tcp = os_object<interface::tcp::tcp, detail::tcp_creator, detail::tcp_deleter>;
using udp = os_object<interface::udp::udp, detail::udp_creator, detail::udp_deleter>;
using poller = os_object<interface::poll::poller, detail::poller_creator, detail::poller_deleter>;

#ifdef LOOPER_UNIX_SOCKETS
using unix_socket = os_object<interface::unix_sock::unix_socket, detail::unix_socket_creator, detail::unix_socket_deleter>;
#endif



namespace detail {

template<typename t_>
concept os_object_type = std::is_same_v<t_, event> || std::is_same_v<t_, tcp> || std::is_same_v<t_, udp> || std::is_same_v<t_, poller> ||
#ifdef LOOPER_UNIX_SOCKETS
    std::is_same_v<t_, unix_socket>
#endif
    ;

using streamable_type = std::variant<tcp>;

template<os_object_type t_>
struct os_descriptor;
template<os_object_type t_>
struct os_socket;
template<os_object_type t_>
struct os_stream;

template<>
struct os_descriptor<event> {
    static os::descriptor get(const event& obj) {
        return interface::event::get_descriptor(obj);
    }
};

template<>
struct os_socket<event> {};
template<>
struct os_stream<event> {};

template<>
struct os_descriptor<tcp> {
    static os::descriptor get(const tcp& obj) {
        return interface::tcp::get_descriptor(obj);
    }
};

template<>
struct os_socket<tcp> {
    static looper::error ipv4_bind(const tcp& obj, const std::string_view ip, const uint16_t port) {
        return interface::tcp::bind(obj, ip, port);
    }
    static looper::error ipv4_bind(const tcp& obj, const uint16_t port) {
        return interface::tcp::bind(obj, port);
    }
    static looper::error ipv4_connect(const tcp& obj, const std::string_view ip, const uint16_t port) {
        return interface::tcp::connect(obj, ip, port);
    }
    static looper::error ipv4_finalize_connect(const tcp& obj) {
        return interface::tcp::finalize_connect(obj);
    }
};

template<>
struct os_stream<tcp> {
    static looper::error read(const tcp& obj, std::span<uint8_t> buffer, size_t& read_out) {
        return interface::tcp::read(obj, buffer.data(), buffer.size(), read_out);
    }
    static looper::error write(const tcp& obj, const std::span<const uint8_t> buffer, size_t& written_out) {
        return interface::tcp::write(obj, buffer.data(), buffer.size(), written_out);
    }
};

template<>
struct os_descriptor<udp> {
    static os::descriptor get(const udp& obj) {
        return interface::udp::get_descriptor(obj);
    }
};

template<>
struct os_socket<udp> {
    static looper::error ipv4_bind(const udp& obj, const std::string_view ip, const uint16_t port) {
        return interface::udp::bind(obj, ip, port);
    }
    static looper::error ipv4_bind(const udp& obj, const uint16_t port) {
        return interface::udp::bind(obj, port);
    }
};

template<>
struct os_stream<udp> {};

template<>
struct os_descriptor<poller> {};
template<>
struct os_socket<poller> {};
template<>
struct os_stream<poller> {};

#ifdef LOOPER_UNIX_SOCKETS

template<>
struct os_descriptor<unix_socket> {
    static os::descriptor get(const unix_socket& obj) {
        return interface::unix_sock::get_descriptor(obj);
    }
};

#endif

}

template<detail::os_object_type t_>
os::descriptor get_descriptor(const t_& t) {
    return detail::os_descriptor<t_>::get(t);
}

template<detail::os_object_type t_>
looper::error ipv4_bind(const t_& t, const std::string_view ip, const uint16_t port) {
    return detail::os_socket<t_>::ipv4_bind(t, ip, port);
}

template<detail::os_object_type t_>
looper::error ipv4_bind(const t_& t, const uint16_t port) {
    return detail::os_socket<t_>::ipv4_bind(t, port);
}

class streamable_object {
public:
    template<detail::os_object_type t_>
    explicit streamable_object(t_&& t) : m_t(t) {}

    [[nodiscard]] os::descriptor get_descriptor() const {
        return std::visit([]<typename T0>(const T0& val)->auto {
            using T = std::decay_t<T0>;
            return detail::os_descriptor<T>::get(val);
        }, m_t);
    }

    looper::error read(std::span<uint8_t> buffer, size_t& read_out) {
        return std::visit([&buffer, &read_out]<typename T0>(const T0& val)->looper::error {
            using T = std::decay_t<T0>;
            return detail::os_stream<T>::read(val, buffer, read_out);
        }, m_t);
    }

    looper::error write(const std::span<const uint8_t> buffer, size_t& written_out) {
        return std::visit([&buffer, &written_out]<typename T0>(const T0& val)->looper::error {
            using T = std::decay_t<T0>;
            return detail::os_stream<T>::write(val, buffer, written_out);
        }, m_t);
    }

private:
    detail::streamable_type m_t;
};

}
