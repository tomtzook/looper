
#include "looper_trace.h"

#include "loop.h"

namespace looper::impl {

#define log_module loop_log_module

constexpr event_types must_have_events = event_error | event_hung;

loop::loop(const looper::loop handle)
    : m_handle(handle)
    , m_mutex()
    , m_poller(os::poller::create())
    , m_timeout(initial_poll_timeout)
    , m_run_loop_event(os::event::create())
    , m_event_data()
    , m_stop(false)
    , m_executing(false)
    , m_run_finished()
    , m_resource_table(0, handles::type_resource)
    , m_descriptor_map()
    , m_futures()
    , m_timers()
    , m_updates() {
    m_updates.resize(initial_reserve_size);

    looper_trace_info(log_module, "creating loop: handle=%lu", m_handle);

    add_resource(os::get_descriptor(m_run_loop_event),
                 event_in,
                 [this](resource, void*, event_types)->void {
                     os::event_clear(m_run_loop_event);
                 });
}

loop::~loop() {
    std::unique_lock lock(m_mutex);

    looper_trace_info(log_module, "stopping looper");

    m_stop = true;
    signal_run();

    if (m_executing) {
        m_run_finished.wait(lock, [this]()->bool {
            return !m_executing;
        });
    }

    lock.unlock();
}

[[nodiscard]] looper::loop loop::handle() const {
    return m_handle;
}

std::unique_lock<std::mutex> loop::lock_loop() {
    return std::unique_lock(m_mutex);
}

resource loop::add_resource(
    os::descriptor descriptor,
    const event_types events,
    resource_callback&& callback,
    void* user_ptr) {
    auto [lock, _1] = lock_if_needed();

    const auto it = m_descriptor_map.find(descriptor);
    if (it != m_descriptor_map.end()) {
        throw std::runtime_error("resource already added");
    }

    auto [handle, data] = m_resource_table.allocate_new();
    data->user_ptr = user_ptr;
    data->descriptor = descriptor;
    data->events = 0;
    data->callback = std::move(callback);

    looper_trace_debug(log_module, "adding resource: loop=%lu, handle=%lu, fd=%lu", m_handle, handle, descriptor);

    auto [_2, data_ptr] = m_resource_table.assign(handle, std::move(data));

    m_descriptor_map.emplace(descriptor, &data_ptr);
    m_updates.push_back({handle, update::type_add, events});

    signal_run();

    return handle;
}

void loop::remove_resource(const resource resource) {
    auto [lock, _] = lock_if_needed();

    const auto data = m_resource_table.release(resource);

    looper_trace_debug(log_module, "removing resource: loop=%lu, handle=%lu", m_handle, resource);

    m_descriptor_map.erase(data->descriptor);
    os::poller_remove(m_poller, data->descriptor);

    signal_run();
}

void loop::request_resource_events(
    const resource resource,
    const event_types events,
    const events_update_type type) {
    auto [lock, _] = lock_if_needed();

    const auto& data = m_resource_table[resource];

    update::update_type update_type;
    switch (type) {
        case events_update_type::override:
            update_type = update::type_new_events;
            break;
        case events_update_type::append:
            update_type = update::type_new_events_add;
            break;
        case events_update_type::remove:
            update_type = update::type_new_events_remove;
            break;
        default:
            throw std::runtime_error("unsupported event type");
    }

    looper_trace_debug(log_module, "modifying resource events: loop=%lu, handle=%lu, type=%d, events=0x%x",
                       m_handle, resource, static_cast<uint8_t>(update_type), events);

    m_updates.push_back({data.our_handle, update_type, events});
    signal_run();
}

void loop::add_future(future_data* data) {
    auto [lock, _] = lock_if_needed();

    m_futures.push_back(data);
}

void loop::remove_future(future_data* data) {
    auto [lock, _] = lock_if_needed();

    m_futures.remove(data);
}

void loop::add_timer(timer_data* data) {
    auto [lock, _] = lock_if_needed();

    m_timers.push_back(data);
}

void loop::remove_timer(timer_data* data) {
    auto [lock, _] = lock_if_needed();

    m_timers.remove(data);
}

void loop::set_timeout_if_smaller(const std::chrono::milliseconds timeout) {
    auto [lock, _] = lock_if_needed();

    if (m_timeout > timeout) {
        m_timeout = timeout;
    }
}

void loop::reset_smallest_timeout() {
    auto [lock, _] = lock_if_needed();

    std::chrono::milliseconds timeout = initial_poll_timeout;
    for (const auto* timer : m_timers) {
        if (timer->timeout < timeout) {
            timeout = timer->timeout;
        }
    }

    m_timeout = timeout;
}

void loop::signal_run() {
    auto [lock, _] = lock_if_needed();

    looper_trace_debug(log_module, "signalling loop run: loop=%lu", m_handle);
    os::event_set(m_run_loop_event);
}

bool loop::run_once() {
    std::unique_lock lock(m_mutex);

    if (m_stop) {
        looper_trace_debug(log_module, "looper marked stop, not running");
        return true;
    }

    m_executing = true;
    looper_trace_debug(log_module, "start looper run");

    process_updates();

    size_t event_count;
    lock.unlock();
    {
        const auto status = os::poller_poll(
            m_poller,
            max_events_for_process,
            m_timeout,
            m_event_data,
            event_count);
        if (status == error_interrupted) {
            // timeout
            event_count = 0;
        } else if (status != error_success) {
            looper_trace_error(log_module, "failed to poll: code=%lu", status);
            std::abort();
        }
    }
    lock.lock();

    if (event_count != 0) {
        process_events(lock, event_count);
    }

    process_timers(lock);
    process_futures(lock);

    looper_trace_debug(log_module, "finish looper run");
    m_executing = false;
    m_run_finished.notify_all();

    return m_stop;
}

void loop::process_timers(std::unique_lock<std::mutex>& lock) const {
    std::vector<loop_timer_callback> to_call;

    const auto now = time_now();
    for (auto* timer : m_timers) {
        if (timer->hit || timer->next_timestamp > now) {
            continue;
        }

        looper_trace_debug(log_module, "timer hit: ptr=0x%x", timer);

        timer->hit = true;
        to_call.push_back(timer->callback);
    }

    lock.unlock();
    for (const auto& callback : to_call) {
        invoke_func_nolock("timer_callback", callback);
    }
    lock.lock();
}

void loop::process_futures(std::unique_lock<std::mutex>& lock) const {
    std::vector<loop_future_callback> to_call;

    const auto now = time_now();
    for (auto* future : m_futures) {
        if (future->finished || future->execute_time > now) {
            continue;
        }

        looper_trace_debug(log_module, "future finished: ptr=0x%x", future);

        future->finished = true;
        to_call.push_back(future->callback);
    }

    lock.unlock();
    for (const auto& callback : to_call) {
        invoke_func_nolock("future_callback", callback);
    }
    lock.lock();
}

void loop::process_update(const update& update) {
    if (!m_resource_table.has(update.handle)) {
        return;
    }

    auto& data = m_resource_table[update.handle];

    switch (update.type) {
        case update::type_add: {
            data.events = update.events | must_have_events;
            const auto status = os::poller_add(m_poller, data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
        case update::type_new_events: {
            data.events = update.events | must_have_events;
            const auto status = os::poller_set(m_poller, data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
        case update::type_new_events_add: {
            data.events |= update.events | must_have_events;
            const auto status = os::poller_set(m_poller, data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
        case update::type_new_events_remove: {
            data.events &= ~update.events;
            data.events |= must_have_events;
            const auto status = os::poller_set(m_poller, data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
    }
}

void loop::process_updates() {
    while (!m_updates.empty()) {
        auto& update = m_updates.front();
        process_update(update);

        m_updates.pop_front();
    }
}

void loop::process_events(std::unique_lock<std::mutex>& lock, const size_t event_count) {
    for (int i = 0; i < event_count; i++) {
        auto& current_event_data = m_event_data[i];

        auto it = m_descriptor_map.find(current_event_data.descriptor);
        if (it == m_descriptor_map.end()) {
            // make sure to remove this fd, guess it somehow was left over
            looper_trace_debug(log_module, "resource received events, but isn't attached to anything: fd=%lu", current_event_data.descriptor);

            const auto status = os::poller_remove(m_poller, current_event_data.descriptor);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }

            continue;
        }

        const auto* resource_data = it->second;

        if ((current_event_data.events & (event_error | event_hung)) != 0) {
            // we got an error on the resource, push it into the resource handler by marking
            // other flags as active and let the syscalls handle it then
            current_event_data.events |= resource_data->events & (event_out | event_in);
        }

        const auto adjusted_flags = (resource_data->events & current_event_data.events);
        if (adjusted_flags == 0) {
            continue;
        }

        looper_trace_debug(log_module, "resource has events: loop=%lu, handle=%lu, events=0x%x",
                           m_handle, resource_data->our_handle, adjusted_flags);

        invoke_func(lock, "resource_callback",
                    resource_data->callback, resource_data->our_handle, resource_data->user_ptr, adjusted_flags);
    }
}

std::pair<std::unique_lock<std::mutex>, bool> loop::lock_if_needed() {
    std::unique_lock lock(m_mutex, std::defer_lock);
    const auto locked = lock.try_lock();
    return {std::move(lock), locked};
}

std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
}

}
