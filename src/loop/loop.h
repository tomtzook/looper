#pragma once

#include <memory>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <chrono>
#include <list>

#include "looper_types.h"

#include "os/os.h"
#include "util/handles.h"
#include "util/util.h"
#include "types_internal.h"

namespace looper::impl {

#define loop_log_module "loop"

class loop;

using resource = handle;
using resource_callback = std::function<void(resource, void*, event_type)>;
using loop_callback = std::function<void()>;
using loop_timer_callback = std::function<void()>;
using loop_future_callback = std::function<void()>;
using loop_ptr = std::shared_ptr<loop>;

static constexpr size_t max_events_for_process = 20;
static constexpr size_t initial_reserve_size = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);
static constexpr size_t resource_table_size = 256;

enum class events_update_type {
    override,
    append,
    remove
};

struct timer_data {
    timer_data()
        : timeout(0)
        , next_timestamp(0)
        , hit(true)
        , callback(nullptr)
    {}

    std::chrono::milliseconds timeout;
    std::chrono::milliseconds next_timestamp;
    bool hit;
    loop_timer_callback callback;
};

struct future_data {
    future_data()
        : finished(true)
        , execute_time(0)
        , callback(nullptr)
    {}

    bool finished;
    std::chrono::milliseconds execute_time;
    loop_future_callback callback;
};

struct resource_data {
    explicit resource_data(const resource handle)
        : our_handle(handle)
        , user_ptr(nullptr)
        , descriptor(-1)
        , events(event_type::none)
        , callback(nullptr)
    {}

    resource our_handle;
    void* user_ptr;
    os::descriptor descriptor;
    event_type events;
    resource_callback callback;
};

struct update {
    enum update_type {
        type_add,
        type_new_events,
        type_new_events_add,
        type_new_events_remove,
    };

    resource handle;
    update_type type;
    event_type events;
};

class loop {
public:
    explicit loop(looper::loop handle) noexcept;
    ~loop() noexcept;

    loop(loop&) = delete;
    loop(loop&&) = delete;
    loop& operator=(loop&) = delete;
    loop& operator=(loop&&) = delete;

    [[nodiscard]] looper::loop handle() const;

    std::unique_lock<std::mutex> lock_loop() noexcept;

    resource add_resource(os::descriptor descriptor,
                          event_type events,
                          resource_callback&& callback,
                          void* user_ptr = nullptr) noexcept;
    void remove_resource(resource resource) noexcept;
    void request_resource_events(resource resource, event_type events, events_update_type type) noexcept;

    void add_future(future_data* data) noexcept;
    void remove_future(future_data* data) noexcept;
    void add_timer(timer_data* data) noexcept;
    void remove_timer(timer_data* data) noexcept;

    void invoke_from_loop(loop_callback&& callback) noexcept;

    void set_timeout_if_smaller(std::chrono::milliseconds timeout) noexcept;
    void reset_smallest_timeout() noexcept;
    void signal_run() noexcept;

    // loop cannot be locked by current thread when this is called
    bool run_once() noexcept;

private:
    void process_timers(std::unique_lock<std::mutex>& lock) const noexcept;
    void process_futures(std::unique_lock<std::mutex>& lock) const noexcept;
    void process_update(const update& update) noexcept;
    void process_updates() noexcept;
    void process_invokes(std::unique_lock<std::mutex>& lock) noexcept;
    void process_events(std::unique_lock<std::mutex>& lock, size_t event_count) noexcept;

    std::pair<std::unique_lock<std::mutex>, bool> lock_if_needed() noexcept;

    looper::loop m_handle;
    std::mutex m_mutex;
    os::poller m_poller;
    std::chrono::milliseconds m_timeout;
    os::event m_run_loop_event;
    os::interface::poll::event_data m_event_data[max_events_for_process];

    bool m_stop;
    bool m_executing;
    std::condition_variable m_run_finished;

    handles::handle_table<resource_data, resource_table_size> m_resource_table;
    std::unordered_map<os::descriptor, resource_data*> m_descriptor_map;
    std::list<future_data*> m_futures;
    std::list<timer_data*> m_timers;
    std::deque<update> m_updates;
    std::deque<loop_callback> m_invoke_callbacks;
};

std::chrono::milliseconds time_now();

}
