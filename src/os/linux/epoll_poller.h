#pragma once

#include <sys/epoll.h>

#include "poll.h"
#include "linux.h"

namespace looper::os {

class epoll_poller : public poller {
public:
    epoll_poller();
    ~epoll_poller() override;

    void add(os::descriptor descriptor, event_types events) override;
    void set(os::descriptor descriptor, event_types events) override;
    void remove(os::descriptor descriptor) override;

    polled_events poll(size_t max_events, std::chrono::milliseconds timeout) override;

private:
    class epoll_event_data : public event_data {
    public:
        explicit epoll_event_data(std::shared_ptr<epoll_event[]> events);

        void reset_events(std::shared_ptr<epoll_event[]> events);

        [[nodiscard]] size_t count() const override;
        void set_count(size_t count);

        [[nodiscard]] descriptor get_descriptor(size_t index) const override;
        [[nodiscard]] event_types get_events(size_t index) const override;

    private:
        std::shared_ptr<epoll_event[]> m_events;
        size_t m_count;
    };

    os::descriptor m_descriptor;
    std::shared_ptr<epoll_event[]> m_events;
    size_t m_events_buffer_size;
    epoll_event_data m_data;
};

}
