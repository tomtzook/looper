#pragma once

#include <cstdint>
#include <cstddef>
#include <chrono>
#include <memory>

#include <looper_types.h>

#include "os/os.h"

namespace looper {

using event_types = uint32_t;

enum event_type : event_types {
    event_in = (0x1 << 0),
    event_out = (0x1 << 1),
    event_error = (0x1 << 2),
    event_hung = (0x1 << 3)
};

class event_data {
public:
    virtual ~event_data() = default;

    [[nodiscard]] virtual size_t count() const = 0;

    [[nodiscard]] virtual os::descriptor get_descriptor(size_t index) const = 0;
    [[nodiscard]] virtual event_types get_events(size_t index) const = 0;
};

class polled_events final {
public:
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::pair<os::descriptor, event_types>;

        iterator(event_data* data, size_t index)
                : m_data(data)
                , m_index(index)
        {}

        value_type operator*() const {
            auto descriptor = m_data->get_descriptor(m_index);
            auto events = m_data->get_events(m_index);
            return {descriptor, events};
        }

        iterator& operator++() {
            m_index++;
            return *this;
        }

        friend bool operator==(const iterator& a, const iterator& b) {
            return a.m_index == b.m_index;
        }
        friend bool operator!=(const iterator& a, const iterator& b) {
            return a.m_index != b.m_index;
        }

    private:
        event_data* m_data;
        size_t m_index;
    };

    explicit polled_events(event_data* data);

    [[nodiscard]] iterator begin() const;
    [[nodiscard]] iterator end() const;

private:
    event_data* m_data;
};

class poller {
public:
    virtual ~poller() = default;

    virtual void add(os::descriptor descriptor, event_types events) = 0;
    virtual void set(os::descriptor descriptor, event_types events) = 0;
    virtual void remove(os::descriptor descriptor) = 0;

    virtual polled_events poll(size_t max_events, std::chrono::milliseconds timeout) = 0;
};

}
