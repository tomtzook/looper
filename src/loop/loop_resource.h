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
    [[nodiscard]] looper::error verify_not_errored() const;

    [[nodiscard]] bool is_reading() const;
    [[nodiscard]] looper::error verify_not_reading() const;

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
        using handle_events_func = std::function<void(std::unique_lock<std::mutex>&, control&, event_type events)>;

        control(loop_ptr loop, looper::impl::resource& resource);

        [[nodiscard]] looper::impl::resource handle() const;

        void attach_to_loop(os::descriptor descriptor, event_type events, handle_events_func&& handle_events) noexcept;
        void detach_from_loop() noexcept;
        void request_events(event_type events, events_update_type type) const noexcept;
        void invoke_in_loop(loop_callback&& callback) const noexcept;

        template<typename... args_>
        void invoke_in_loop(const std::function<void(args_...)>& ref, args_... args) const noexcept {
            auto ref_val = ref;
            if (ref_val != nullptr) {
                invoke_in_loop([ref_val, args...]()->void {
                    ref_val(args...);
                });
            }
        }

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

    [[nodiscard]] std::pair<std::unique_lock<std::mutex>, control> lock_loop() noexcept;

private:
    loop_ptr m_loop;
    looper::impl::resource m_resource;
};

}
