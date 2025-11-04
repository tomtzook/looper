#pragma once

#include "loop_internal.h"

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

class looper_resource {
public:
    class control final {
    public:
        using handle_events_func = std::function<void(std::unique_lock<std::mutex>&, control&, event_types events)>;

        control(loop_context* context, looper::impl::resource& resource);

        [[nodiscard]] looper::loop loop_handle() const;
        [[nodiscard]] looper::impl::resource handle() const;

        void attach_to_loop(os::descriptor descriptor, event_types events, handle_events_func&& handle_events);
        void detach_from_loop();
        void request_events(event_types events, events_update_type type) const;

    private:
        loop_context* m_context;
        looper::impl::resource& m_resource;
    };

    explicit looper_resource(loop_context* context);
    ~looper_resource();

    [[nodiscard]] looper::loop loop_handle() const;
    [[nodiscard]] looper::impl::resource handle() const;

    std::pair<std::unique_lock<std::mutex>, control> lock_loop();

private:
    loop_context* m_context;
    looper::impl::resource m_resource;
};

}
