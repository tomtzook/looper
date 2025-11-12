#pragma once

#include <looper.h>

namespace looper {

template<typename t_>
concept handle_type = std::same_as<t_, handle>;

template<typename t_, typename handle_t_>
concept handle_closer = requires(t_ t, handle_t_ handle) {
    { t(handle) } -> std::same_as<void>;
};

template<handle_type t_, handle_closer<t_> closer_>
struct handle_holder {
    handle_holder();
    ~handle_holder();

    explicit handle_holder(handle_holder&) = delete;
    handle_holder& operator=(handle_holder&) = delete;

    explicit handle_holder(t_ handle);
    handle_holder& operator=(t_ handle) noexcept;
    handle_holder(handle_holder&& other) noexcept;
    handle_holder& operator=(handle_holder&& other) noexcept;

    // ReSharper disable once CppNonExplicitConversionOperator
    operator t_() const; // NOLINT(*-explicit-constructor)
    [[nodiscard]] t_ handle() const;

    void reset();

private:
    t_ m_handle;
};

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>::handle_holder()
    : m_handle(empty_handle)
{}

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>::~handle_holder() {
    reset();
}

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>::handle_holder(t_ handle)
    : m_handle(handle)
{}

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>& handle_holder<t_, closer_>::operator=(t_ handle) noexcept {
    reset();
    m_handle = handle;
    return *this;
}

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>::handle_holder(handle_holder&& other) noexcept
    : m_handle(other.m_handle) {
    other.m_handle = empty_handle;
}

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>& handle_holder<t_, closer_>::operator=(handle_holder&& other) noexcept {
    m_handle = other.m_handle;
    other.m_handle = empty_handle;
    return *this;
}

template<handle_type t_, handle_closer<t_> closer_>
handle_holder<t_, closer_>::operator t_() const {
    return m_handle;
}

template<handle_type t_, handle_closer<t_> closer_>
t_ handle_holder<t_, closer_>::handle() const {
    return m_handle;
}

template<handle_type t_, handle_closer<t_> closer_>
void handle_holder<t_, closer_>::reset() {
    if (m_handle != empty_handle) {
        closer_()(m_handle);
        m_handle = empty_handle;
    }
}

struct loop_closer {
    void operator()(const loop loop) const {
        destroy(loop);
    }
};

struct future_closer {
    void operator()(const future future) const {
        destroy_future(future);
    }
};

struct event_closer {
    void operator()(const event event) const {
        destroy_event(event);
    }
};

struct timer_closer {
    void operator()(const timer timer) const {
        destroy_timer(timer);
    }
};

struct tcp_closer {
    void operator()(const tcp tcp) const {
        destroy_tcp(tcp);
    }
};

struct tcp_server_closer {
    void operator()(const tcp_server tcp) const {
        destroy_tcp_server(tcp);
    }
};

struct udp_closer {
    void operator()(const udp udp) const {
        destroy_udp(udp);
    }
};

using loop_holder = handle_holder<loop, loop_closer>;
using future_holder = handle_holder<future, future_closer>;
using event_holder = handle_holder<event, event_closer>;
using timer_holder = handle_holder<timer, timer_closer>;
using tcp_holder = handle_holder<tcp, tcp_closer>;
using tcp_server_holder = handle_holder<tcp_server, tcp_server_closer>;
using udp_holder = handle_holder<udp, udp_closer>;

#ifdef LOOPER_UNIX_SOCKETS

struct unix_socket_closer {
    void operator()(const handle handle) const {
        destroy_unix_socket(handle);
    }
};

struct unix_socket_server_closer {
    void operator()(const handle handle) const {
        destroy_unix_socket_server(handle);
    }
};

using unix_socket_holder = handle_holder<unix_socket, unix_socket_closer>;
using unix_socket_server_holder = handle_holder<unix_socket_server, unix_socket_server_closer>;

#endif

/**
 * Calls looper::create to create a new loop and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new loop handle
 */
inline loop_holder make_loop() {
    return loop_holder(create());
}

/**
 * Calls looper::create_future to create a new future and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new future handle
 */
inline future_holder make_future(const loop loop, future_callback&& callback) {
    return future_holder(create_future(loop, std::move(callback)));
}

/**
 * Calls looper::create_event to create a new event and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new event handle
 */
inline event_holder make_event(const loop loop, event_callback&& callback) {
    return event_holder(create_event(loop, std::move(callback)));
}

/**
 * Calls looper::create_timer to create a new timer and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new timer handle
 */
inline timer_holder make_timer(const loop loop, const std::chrono::milliseconds timeout, timer_callback&& callback) {
    return timer_holder(create_timer(loop, timeout, std::move(callback)));
}

/**
 * Calls looper::create_tcp to create a new tcp and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new tcp handle
 */
inline tcp_holder make_tcp(const loop loop) {
    return tcp_holder(create_tcp(loop));
}

/**
 * Calls looper::create_tcp_server to create a new tcp_server and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new tcp_server handle
 */
inline tcp_server_holder make_tcp_server(const loop loop) {
    return tcp_server_holder(create_tcp_server(loop));
}

/**
 * Calls looper::create_udp to create a new udp and returns it in a holder.
 * See the used function for more documentation.
 *
 * @return handle holder with new udp handle
 */
inline udp_holder make_udp(const loop loop) {
    return udp_holder(create_udp(loop));
}

#ifdef LOOPER_UNIX_SOCKETS

inline unix_socket_holder make_unix_socket(const loop loop) {
    return unix_socket_holder(create_unix_socket(loop));
}

inline unix_socket_server_holder make_unix_socket_server(const loop loop) {
    return unix_socket_server_holder(create_unix_socket_server(loop));
}

#endif

}
