#pragma once

#include <assert.h>
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
    void operator()(const interface::event::event* event) const {
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
    void operator()(const interface::poll::poller* poller) const {
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

    os_object(const os_object&) = delete;
    os_object(os_object&&) = default;
    os_object& operator=(const os_object&) = delete;
    os_object& operator=(os_object&&) = default;

    explicit os_object(smart_ptr&& ptr) : m_ptr(std::move(ptr)) {}

    // ReSharper disable once CppNonExplicitConversionOperator
    operator pointer() const { assert(m_ptr.get() != nullptr); return m_ptr.get(); } // NOLINT(*-explicit-constructor)

    void close() { m_ptr.reset(); }

    static os_object create() {
        const auto obj = creator_()();
        return os_object(smart_ptr(obj));
    }

    static os_object empty() {
        return os_object();
    }

private:
    os_object() : m_ptr() {}

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

template<os_object_type t_>
struct os_descriptor;
template<os_object_type t_>
struct os_socket;
template<os_object_type t_>
struct os_socket_server;
template<os_object_type t_>
struct os_stream;

template<typename t_>
concept connectable_socket_type = requires(t_ t, const t_& f1_type) {
    { os_socket<t_>::finalize_connect(f1_type) } -> std::same_as<looper::error>;
};

template<>
struct os_descriptor<event> {
    static os::descriptor get(const event& obj) {
        return interface::event::get_descriptor(obj);
    }
};

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
    static looper::error finalize_connect(const tcp& obj) {
        return interface::tcp::finalize_connect(obj);
    }
};

template<>
struct os_socket_server<tcp> {
    static looper::error socket_accept(const tcp& obj, const size_t backlog) {
        return interface::tcp::listen(obj, backlog);
    }
    static std::pair<looper::error, tcp> socket_accept(const tcp& obj) {
        interface::tcp::tcp* new_tcp;
        const auto status = interface::tcp::accept(obj, &new_tcp);
        if (status != error_success) {
            return { status, tcp::empty() };
        }

        return { error_success, tcp(tcp::smart_ptr(new_tcp)) };
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

#ifdef LOOPER_UNIX_SOCKETS

template<>
struct os_descriptor<unix_socket> {
    static os::descriptor get(const unix_socket& obj) {
        return interface::unix_sock::get_descriptor(obj);
    }
};

template<>
struct os_socket<unix_socket> {
    static looper::error unix_bind(const unix_socket& obj, const std::string_view path) {
        return interface::unix_sock::bind(obj, path);
    }
    static looper::error unix_connect(const unix_socket& obj, const std::string_view path) {
        return interface::unix_sock::connect(obj, path);
    }
    static looper::error finalize_connect(const unix_socket& obj) {
        return interface::unix_sock::finalize_connect(obj);
    }
};

template<>
struct os_socket_server<unix_socket> {
    static looper::error socket_accept(const unix_socket& obj, const size_t backlog) {
        return interface::unix_sock::listen(obj, backlog);
    }
    static std::pair<looper::error, unix_socket> socket_accept(const unix_socket& obj) {
        interface::unix_sock::unix_socket* new_obj;
        const auto status = interface::unix_sock::accept(obj, &new_obj);
        if (status != error_success) {
            return { status, unix_socket::empty() };
        }

        return { error_success, unix_socket(unix_socket::smart_ptr(new_obj)) };
    }
};

template<>
struct os_stream<unix_socket> {
    static looper::error read(const unix_socket& obj, std::span<uint8_t> buffer, size_t& read_out) {
        return interface::unix_sock::read(obj, buffer.data(), buffer.size(), read_out);
    }
    static looper::error write(const unix_socket& obj, const std::span<const uint8_t> buffer, size_t& written_out) {
        return interface::unix_sock::write(obj, buffer.data(), buffer.size(), written_out);
    }
};

#endif

}

template<typename t_>
concept os_object_type = detail::os_object_type<t_>;
template<typename t_>
concept os_stream_type = requires(t_ t) {
    { detail::os_stream<t_>::read };
    { detail::os_stream<t_>::write };
};


// todo: shrink scopes here with concepts

template<os_object_type t_>
os::descriptor get_descriptor(const t_& t) {
    return detail::os_descriptor<t_>::get(t);
}

inline looper::error event_set(const event& obj) {
    return interface::event::set(obj);
}

inline looper::error event_clear(const event& obj) {
    return interface::event::clear(obj);
}

inline looper::error poller_add(const poller& obj, const os::descriptor descriptor, const event_type events) {
    return interface::poll::add(obj, descriptor, events);
}

inline looper::error poller_remove(const poller& obj, const os::descriptor descriptor) {
    return interface::poll::remove(obj, descriptor);
}

inline looper::error poller_set(const poller& obj, const os::descriptor descriptor, const event_type events) {
    return interface::poll::set(obj, descriptor, events);
}

inline looper::error poller_poll(
    const poller& obj,
    const size_t max_events,
    const std::chrono::milliseconds timeout,
    interface::poll::event_data* events,
    size_t& event_count) {
    return interface::poll::poll(obj, max_events, timeout, events, event_count);
}

template<os_object_type t_>
looper::error ipv4_bind(const t_& t, const std::string_view ip, const uint16_t port) {
    return detail::os_socket<t_>::ipv4_bind(t, ip, port);
}

template<os_object_type t_>
looper::error ipv4_bind(const t_& t, const uint16_t port) {
    return detail::os_socket<t_>::ipv4_bind(t, port);
}

template<os_object_type t_>
looper::error ipv4_connect(const t_& t, const std::string_view ip, const uint16_t port) {
    return detail::os_socket<t_>::ipv4_connect(t, ip, port);
}

template<os_object_type t_>
looper::error finalize_connect(const t_& t) {
    return detail::os_socket<t_>::finalize_connect(t);
}

template<os_object_type t_>
looper::error socket_listen(const t_& t, const size_t backlog) {
    return detail::os_socket_server<t_>::socket_accept(t, backlog);
}

template<os_object_type t_>
std::pair<looper::error, t_> socket_accept(const t_& t) {
    return detail::os_socket_server<t_>::socket_accept(t);
}

template<os_stream_type t_>
looper::error stream_read(const t_& t, std::span<uint8_t> buffer, size_t& read_out) {
    return detail::os_stream<t_>::read(t, buffer, read_out);
}

template<os_stream_type t_>
looper::error stream_write(const t_& t, const std::span<const uint8_t> buffer, size_t& written_out) {
    return detail::os_stream<t_>::write(t, buffer, written_out);
}

#ifdef LOOPER_UNIX_SOCKETS

template<os_object_type t_>
looper::error unix_bind(const t_& t, const std::string_view path) {
    return detail::os_socket<t_>::unix_bind(t, path);
}

template<os_object_type t_>
looper::error unix_connect(const t_& t, const std::string_view path) {
    return detail::os_socket<t_>::unix_connect(t, path);
}

#endif

}
