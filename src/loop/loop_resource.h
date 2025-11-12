#pragma once

#include "loop.h"

namespace looper::impl {

template<typename T>
concept readable_resource = requires(T t, read_callback&& func1_callback) {
    { t.start_read(func1_callback) } -> std::same_as<void>;
    { t.stop_read() } -> std::same_as<void>;
};

struct resource_state {
public:
    resource_state();

    [[nodiscard]] bool is_errored() const;
    void verify_not_errored() const;

    [[nodiscard]] bool is_reading() const;
    void verify_not_reading() const;

    [[nodiscard]] bool can_read() const;
    [[nodiscard]] bool can_write() const;

    void mark_errored();
    void set_reading(bool reading);
    void set_read_enabled(bool enabled);
    void set_write_enabled(bool enabled);

private:
    bool m_is_errored;
    bool m_is_reading;
    bool m_can_read;
    bool m_can_write;
};

class loop_resource {
public:
    class control final {
    public:
        using handle_events_func = std::function<void(std::unique_lock<std::mutex>&, control&, event_types events)>;

        control(loop_ptr loop, looper::impl::resource& resource);

        [[nodiscard]] looper::impl::resource handle() const;

        void attach_to_loop(os::descriptor descriptor, event_types events, handle_events_func&& handle_events);
        void detach_from_loop();
        void request_events(event_types events, events_update_type type) const;
        void invoke_in_loop(loop_callback&& callback) const;

    private:
        loop_ptr m_loop;
        looper::impl::resource& m_resource;
    };

    explicit loop_resource(loop_ptr loop);
    ~loop_resource();

    loop_resource(const loop_resource&) = delete;
    loop_resource& operator=(const loop_resource&) = delete;

    loop_resource(loop_resource&& other) noexcept;
    loop_resource& operator=(loop_resource&& other) noexcept;

    [[nodiscard]] looper::impl::resource handle() const;

    std::pair<std::unique_lock<std::mutex>, control> lock_loop();

private:
    loop_ptr m_loop;
    looper::impl::resource m_resource;
};

}
