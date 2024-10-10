
#include <looper_trace.h>

#include "factory.h"
#include "loop.h"

namespace looper {

#define log_module "loop"

static constexpr size_t max_events_for_process = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);

loop_impl::loop_impl(loop handle)
    : m_handle(handle)
    , m_mutex()
    , m_poller(create_poller())
    , m_timeout(initial_poll_timeout)
    , m_run_loop_event(create_event())
    , m_resource_table(handles::handle{handle}.index(), handles::type_resource)
    , m_descriptor_map()
    , m_updates()
    , m_execute_requests() {
    add_resource(m_run_loop_event, event_in, [this](loop loop, resource resource, event_types events)->void {
        m_run_loop_event->clear();
    });
}

resource loop_impl::add_resource(std::shared_ptr<os::resource> resource, event_types events, resource_callback&& callback) {
    std::unique_lock lock(m_mutex);

    const auto descriptor = resource->get_descriptor();

    auto it = m_descriptor_map.find(descriptor);
    if (it != m_descriptor_map.end()) {
        throw std::runtime_error("resource already added");
    }

    auto [handle, data] = m_resource_table.allocate_new();
    data->resource_obj = std::move(resource);
    data->descriptor = descriptor;
    data->events = 0;
    data->callback = std::move(callback);

    m_descriptor_map.emplace(descriptor, data);
    m_updates.push_back({handle, update::type_add, events});

    signal_run();

    return handle;
}

void loop_impl::remove_resource(resource resource) {
    std::unique_lock lock(m_mutex);

    auto data = m_resource_table.release(resource);

    m_descriptor_map.erase(data->descriptor);
    m_poller->remove(data->descriptor);

    signal_run();
}

void loop_impl::request_resource_events(resource resource, event_types events, events_update_type type) {
    std::unique_lock lock(m_mutex);

    auto data = m_resource_table[resource];

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

    m_updates.push_back({data->handle, update_type, events});
    signal_run();
}

void loop_impl::execute_later(execute_callback&& callback, bool wait) {
    std::unique_lock lock(m_mutex);

    execute_request request{};
    request.callback = std::move(callback);
    m_execute_requests.push_back(request);

    signal_run();

    if (wait) {
        // todo: wait for run
    }
}

void loop_impl::run_once() {
    std::unique_lock lock(m_mutex);

    process_updates();

    lock.unlock();
    auto result = m_poller->poll(max_events_for_process, m_timeout);
    lock.lock();

    process_events(lock, result);
    execute_requests(lock);
}

void loop_impl::signal_run() {
    m_run_loop_event->set();
}

void loop_impl::process_updates() {
    while (!m_updates.empty()) {
        auto& update = m_updates.front();
        process_update(update);

        m_updates.pop_front();
    }
}

void loop_impl::process_update(looper::loop_impl::update& update) {
    if (!m_resource_table.has(update.handle)) {
        return;
    }

    auto data = m_resource_table[update.handle];

    switch (update.type) {
        case update::type_add:
            data->events = update.events;
            m_poller->add(data->descriptor, data->events);
            break;
        case update::type_new_events:
            data->events = update.events;
            m_poller->set(data->descriptor, data->events);
            break;
        case update::type_new_events_add:
            data->events |= update.events;
            m_poller->set(data->descriptor, data->events);
            break;
        case update::type_new_events_remove:
            data->events &= ~update.events;
            m_poller->set(data->descriptor, data->events);
            break;
    }
}

void loop_impl::process_events(std::unique_lock<std::mutex>& lock, polled_events& events) {
    for (auto [descriptor, revents] : events) {
        auto it = m_descriptor_map.find(descriptor);
        if (it == m_descriptor_map.end()) {
            continue;
        }

        auto data = it->second;

        auto adjusted_flags = (data->events & revents);
        if (adjusted_flags == 0) {
            continue;
        }

        lock.unlock();
        try {
            data->callback(m_handle, data->handle, adjusted_flags);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in io callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in io callback: unknown");
        }
        lock.lock();
    }
}

void loop_impl::execute_requests(std::unique_lock<std::mutex>& lock) {
    while (!m_execute_requests.empty()) {
        auto& request = m_execute_requests.front();

        lock.unlock();
        try {
            request.callback(m_handle);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error in request callback: what=%s", e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error in request callback: unknown");
        }
        lock.lock();

        m_execute_requests.pop_front();
    }
}

}
