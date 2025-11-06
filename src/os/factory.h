#pragma once

#include <looper_except.h>
#include "os/os.h"


#define OS_CHECK_THROW(...) \
    do {                    \
        auto _status = __VA_ARGS__; \
        if (_status != looper::error_success) { \
            throw looper::os_exception(_status); \
        }                                       \
    } while(0);

namespace looper::os {

struct event_deleter {
    void operator()(event::event* event) const {
        event::close(event);
    }
};

struct tcp_deleter {
    void operator()(tcp::tcp* tcp) const {
        tcp::close(tcp);
    }
};

struct udp_deleter {
    void operator()(udp::udp* udp) const {
        udp::close(udp);
    }
};

struct poller_deleter {
    void operator()(poll::poller* poller) const {
        poll::close(poller);
    }
};

using event_ptr = std::unique_ptr<event::event, event_deleter>;
using tcp_ptr = std::unique_ptr<tcp::tcp, tcp_deleter>;
using udp_ptr = std::unique_ptr<udp::udp, udp_deleter>;
using poller_ptr = std::unique_ptr<poll::poller, poller_deleter>;

inline event_ptr make_event() {
    os::event::event* event;
    const auto status = event::create(&event);
    if (status != error_success) {
        throw os_exception(status);
    }

    return event_ptr(event);
}

inline tcp_ptr make_tcp(tcp::tcp* tcp) {
    return tcp_ptr(tcp);
}

inline tcp_ptr make_tcp() {
    os::tcp::tcp* tcp;
    const auto status = tcp::create(&tcp);
    if (status != error_success) {
        throw os_exception(status);
    }

    return tcp_ptr(tcp);
}

inline udp_ptr make_udp() {
    os::udp::udp* udp;
    const auto status = udp::create(&udp);
    if (status != error_success) {
        throw os_exception(status);
    }

    return udp_ptr(udp);
}

inline poller_ptr make_poller() {
    os::poll::poller* poller;
    const auto status = poll::create(&poller);
    if (status != error_success) {
        throw os_exception(status);
    }

    return poller_ptr(poller);
}

}
