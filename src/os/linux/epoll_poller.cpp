
#include <sys/epoll.h>

#include <looper_except.h>
#include "epoll_poller.h"


namespace looper::os {

static constexpr size_t events_buffer_size = 20;

static descriptor create() {
    const auto fd = ::epoll_create1(0);
    if (fd < 0) {
        throw os_exception(errno);
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
    , m_events(new epoll_event[events_buffer_size])
    , m_data(m_events)
{}

epoll_poller::~epoll_poller() {
    delete[] reinterpret_cast<epoll_event*>(m_events);
    ::close(m_descriptor);
}

void epoll_poller::add(os::descriptor descriptor, event_types events) {
    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(m_descriptor, EPOLL_CTL_ADD, descriptor, &event)) {
        handle_error();
    }
}

void epoll_poller::set(os::descriptor descriptor, event_types events) {
    epoll_event event{};
    event.events = events_to_native(events);
    event.data.fd = descriptor;

    if (::epoll_ctl(m_descriptor, EPOLL_CTL_MOD, descriptor, &event)) {
        handle_error();
    }
}

void epoll_poller::remove(os::descriptor descriptor) {
    epoll_event event{};
    event.events = 0;
    event.data.fd = descriptor;

    if (::epoll_ctl(m_descriptor, EPOLL_CTL_DEL, descriptor, &event)) {
        handle_error();
    }
}

polled_events epoll_poller::poll(size_t max_events, std::chrono::milliseconds timeout) {
    if (max_events > events_buffer_size) {
        // todo: dynamically change the circular_buffer size
        throw std::runtime_error("max events requested exceed support size");
    }

    auto events = reinterpret_cast<epoll_event*>(m_events);
    const auto count = ::epoll_wait(m_descriptor, events, static_cast<int>(max_events), static_cast<int>(timeout.count()));
    if (count < 0) {
        int error = errno;
        if (error == EINTR) {
            // timeout has occurred
            m_data.set_count(0);
            return polled_events{&m_data};
        } else {
            throw os_exception(error);
        }
    }

    m_data.set_count(count);
    return polled_events{&m_data};
}

void epoll_poller::handle_error() {
    int error = errno;
    throw os_exception(error);
}

epoll_poller::epoll_event_data::epoll_event_data(void* events)
    : m_events(events)
    , m_count(0)
{}

size_t epoll_poller::epoll_event_data::count() const {
    return m_count;
}

void epoll_poller::epoll_event_data::set_count(size_t count) {
    m_count = count;
}

descriptor epoll_poller::epoll_event_data::get_descriptor(size_t index) const {
    const auto events = reinterpret_cast<epoll_event*>(m_events);
    return static_cast<descriptor>(events[index].data.fd);
}

event_types epoll_poller::epoll_event_data::get_events(size_t index) const {
    const auto events = reinterpret_cast<epoll_event*>(m_events);
    return native_to_events(events[index].events);
}

}
