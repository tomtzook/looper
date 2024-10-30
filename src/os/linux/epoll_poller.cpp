
#include <looper_except.h>
#include "epoll_poller.h"


namespace looper::os {

static constexpr size_t default_events_buffer_size = 32;

static descriptor create() {
    const auto fd = ::epoll_create1(0);
    if (fd < 0) {
        throw_call_error();
    }

    return fd;
}

static uint32_t events_to_native(event_types events) {
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

static event_types native_to_events(uint32_t events) {
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

epoll_poller::epoll_poller()
    : m_descriptor(create())
    , m_events(new epoll_event[default_events_buffer_size])
    , m_events_buffer_size(default_events_buffer_size)
    , m_data(m_events)
{}

epoll_poller::~epoll_poller() {
    ::close(m_descriptor);
}

void epoll_poller::add(os::descriptor descriptor, event_types events) {
    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(m_descriptor, EPOLL_CTL_ADD, descriptor, &event)) {
        throw_call_error();
    }
}

void epoll_poller::set(os::descriptor descriptor, event_types events) {
    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(m_descriptor, EPOLL_CTL_MOD, descriptor, &event)) {
        throw_call_error();
    }
}

void epoll_poller::remove(os::descriptor descriptor) {
    epoll_event event{};
    event.events = 0;
    event.data.fd = descriptor;

    if (::epoll_ctl(m_descriptor, EPOLL_CTL_DEL, descriptor, &event)) {
        throw_call_error();
    }
}

polled_events epoll_poller::poll(size_t max_events, std::chrono::milliseconds timeout) {
    if (max_events > m_events_buffer_size) {
        m_events.reset(new epoll_event[max_events]);
        m_events_buffer_size = max_events;
        m_data.reset_events(m_events);
    }

    auto* events = reinterpret_cast<epoll_event*>(m_events.get());
    const auto count = ::epoll_wait(m_descriptor, events, static_cast<int>(max_events), static_cast<int>(timeout.count()));
    if (count < 0) {
        const auto error = get_call_error();
        if (error == error_interrupted) {
            // timeout has occurred
            m_data.set_count(0);
            return polled_events{&m_data};
        }

        throw_call_error();
    }

    m_data.set_count(count);
    return polled_events{&m_data};
}

epoll_poller::epoll_event_data::epoll_event_data(std::shared_ptr<epoll_event[]> events)
    : m_events(std::move(events))
    , m_count(0)
{}

void epoll_poller::epoll_event_data::reset_events(std::shared_ptr<epoll_event[]> events) {
    m_events.swap(events);
}

size_t epoll_poller::epoll_event_data::count() const {
    return m_count;
}

void epoll_poller::epoll_event_data::set_count(size_t count) {
    m_count = count;
}

descriptor epoll_poller::epoll_event_data::get_descriptor(size_t index) const {
    const auto* events = reinterpret_cast<epoll_event*>(m_events.get());
    return static_cast<descriptor>(events[index].data.fd);
}

event_types epoll_poller::epoll_event_data::get_events(size_t index) const {
    const auto* events = reinterpret_cast<epoll_event*>(m_events.get());
    return native_to_events(events[index].events);
}

}
