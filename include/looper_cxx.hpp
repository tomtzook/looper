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
    explicit handle_holder(t_ handle)
        : m_handle(handle)
    {}
    ~handle_holder() {
        if (m_handle != empty_handle) {
            closer_()(m_handle);
            m_handle = empty_handle;
        }
    }

    explicit handle_holder(handle_holder&) = delete;
    handle_holder& operator=(handle_holder&) = delete;

    handle_holder(handle_holder&& other) noexcept
        : m_handle(other.m_handle) {
        other.m_handle = empty_handle;
    }
    handle_holder& operator=(handle_holder&& other) noexcept {
        m_handle = other.m_handle;
        other.m_handle = empty_handle;
        return *this;
    }

    [[nodiscard]] t_ handle() const {
        return m_handle;
    }

private:
    t_ m_handle;
};

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

}
