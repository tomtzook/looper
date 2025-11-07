
#include <sys/epoll.h>

#include "types_internal.h"
#include "linux.h"

namespace looper::os::poll {

static constexpr size_t default_events_buffer_size = 32;

static looper::error create_epoll(os::descriptor& descriptor_out) {
    const auto fd = ::epoll_create1(0);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

static uint32_t events_to_native(const event_types events) {
    uint32_t r_events = 0;
    if ((events & event_type::event_in) != 0) {
        r_events |= EPOLLIN;
    }
    if ((events & event_type::event_out) != 0) {
        r_events |= EPOLLOUT;
    }
    if ((events & event_type::event_error) != 0) {
        r_events |= EPOLLERR;
    }
    if ((events & event_type::event_hung) != 0) {
        r_events |= EPOLLHUP;
    }

    return r_events;
}

static event_types native_to_events(const uint32_t events) {
    event_types r_events = 0;
    if ((events & EPOLLIN) != 0) {
        r_events |= event_type::event_in;
    }
    if ((events & EPOLLOUT) != 0) {
        r_events |= event_type::event_out;
    }
    if ((events & EPOLLERR) != 0) {
        r_events |= event_type::event_error;
    }
    if ((events & EPOLLHUP) != 0) {
        r_events |= event_type::event_hung;
    }

    return r_events;
}

struct poller {
    os::descriptor fd;
    epoll_event* events;
    size_t events_buffer_size;
};

looper::error create(poller** poller_out) {
    auto* _poller = static_cast<poller*>(malloc(sizeof(poller)));
    if (_poller == nullptr) {
        return error_allocation;
    }

    _poller->events = static_cast<epoll_event*>(malloc(sizeof(epoll_event) * default_events_buffer_size));
    if (_poller->events == nullptr) {
        free(_poller);
        return error_allocation;
    }
    _poller->events_buffer_size = default_events_buffer_size;

    os::descriptor descriptor;
    const auto status = create_epoll(descriptor);
    if (status != error_success) {
        free(_poller->events);
        free(_poller);
        return status;
    }

    _poller->fd = descriptor;

    *poller_out = _poller;
    return error_success;
}

void close(poller* poller) {
    ::close(poller->fd);

    free(poller->events);
    free(poller);
}

looper::error add(const poller* poller, const os::descriptor descriptor, const event_types events) {
    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(poller->fd, EPOLL_CTL_ADD, descriptor, &event)) {
        return get_call_error();
    }

    return error_success;
}

looper::error set(const poller* poller, const os::descriptor descriptor, const event_types events) {
    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(poller->fd, EPOLL_CTL_MOD, descriptor, &event)) {
        return get_call_error();
    }

    return error_success;
}

looper::error remove(const poller* poller, const os::descriptor descriptor) {
    epoll_event event{};
    event.events = 0;
    event.data.fd = descriptor;

    if (::epoll_ctl(poller->fd, EPOLL_CTL_DEL, descriptor, &event)) {
        return get_call_error();
    }

    return error_success;
}

looper::error poll(poller* poller,
    const size_t max_events,
    const std::chrono::milliseconds timeout,
    event_data* events,
    size_t& event_count) {
    if (max_events > poller->events_buffer_size) {
        auto* _events = static_cast<epoll_event*>(malloc(sizeof(epoll_event) * max_events));
        if (_events == nullptr) {
            return error_allocation;
        }

        free(poller->events);

        poller->events_buffer_size = max_events;
        poller->events = _events;
    }

    const auto count = ::epoll_wait(
        poller->fd,
        poller->events,
        static_cast<int>(max_events),
        static_cast<int>(timeout.count()));
    if (count < 0) {
        return get_call_error();
    }

    for (int i = 0; i < count; i++) {
        auto& event = poller->events[i];
        auto& event_out = events[i];

        event_out.descriptor = event.data.fd;
        event_out.events = native_to_events(event.events);
    }

    event_count = count;
    return error_success;
}

}
