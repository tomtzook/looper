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

using event_ptr = std::unique_ptr<event::event, decltype(&event::close)>;
using tcp_ptr = std::unique_ptr<tcp::tcp, decltype(&tcp::close)>;
using udp_ptr = std::unique_ptr<udp::udp, decltype(&udp::close)>;
using poller_ptr = std::unique_ptr<poll::poller, decltype(&poll::close)>;

static inline event_ptr make_event() {
    event::event* event;
    auto status = event::create(&event);
    if (status != error_success) {
        throw os_exception(status);
    }

    return event_ptr(event, &event::close);
}

static inline tcp_ptr make_tcp(tcp::tcp* tcp) {
    return tcp_ptr(tcp, &tcp::close);
}

static inline tcp_ptr make_tcp() {
    tcp::tcp* tcp;
    auto status = tcp::create(&tcp);
    if (status != error_success) {
        throw os_exception(status);
    }

    return tcp_ptr(tcp, &tcp::close);
}

static inline udp_ptr make_udp() {
    udp::udp* udp;
    auto status = udp::create(&udp);
    if (status != error_success) {
        throw os_exception(status);
    }

    return udp_ptr(udp, &udp::close);
}

static inline poller_ptr make_poller() {
    poll::poller* poller;
    auto status = poll::create(&poller);
    if (status != error_success) {
        throw os_exception(status);
    }

    return poller_ptr(poller, &poll::close);
}

}
