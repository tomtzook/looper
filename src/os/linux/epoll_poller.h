#pragma once

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
        explicit epoll_event_data(void* events);

        [[nodiscard]] size_t count() const override;
        void set_count(size_t count);

        [[nodiscard]] descriptor get_descriptor(size_t index) const override;
        [[nodiscard]] event_types get_events(size_t index) const override;

    private:
        void* m_events;
        size_t m_count;
    };

    void handle_error();

    os::descriptor m_descriptor;
    void* m_events;
    epoll_event_data m_data;
};

}
