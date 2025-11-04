
#include "looper_trace.h"
#include "loop_internal.h"

namespace looper::impl {

#define log_module loop_log_module

constexpr event_types must_have_events = event_error | event_hung;

static void process_update(loop_context* context, const update& update) {
    if (!context->resource_table.has(update.handle)) {
        return;
    }

    auto& data = context->resource_table[update.handle];

    switch (update.type) {
        case update::type_add: {
            data.events = update.events | must_have_events;
            const auto status = os::poll::add(context->poller.get(), data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
        case update::type_new_events: {
            data.events = update.events | must_have_events;
            const auto status = os::poll::set(context->poller.get(), data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
        case update::type_new_events_add: {
            data.events |= update.events | must_have_events;
            const auto status = os::poll::set(context->poller.get(), data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
        case update::type_new_events_remove: {
            data.events &= ~update.events;
            data.events |= must_have_events;
            const auto status = os::poll::set(context->poller.get(), data.descriptor, data.events);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }
            break;
        }
    }
}

loop_context::loop_context(const looper::loop handle)
        : handle(handle)
        , mutex()
        , poller(os::make_poller())
        , timeout(initial_poll_timeout)
        , run_loop_event(os::make_event())
        , event_data{}
        , stop(false)
        , executing(false)
        , run_finished()
        , resource_table(0, handles::type_resource)
        , descriptor_map()
        , futures()
        , timers()
        , updates() {
    updates.resize(initial_reserve_size);
}

std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
}

void signal_run(loop_context* context) {
    looper_trace_debug(log_module, "signalling loop run: context=0x%x", context);
    os::event::set(context->run_loop_event.get());
}

void reset_smallest_timeout(loop_context* context) {
    std::chrono::milliseconds timeout = initial_poll_timeout;
    for (const auto* timer : context->timers) {
        if (timer->timeout < timeout) {
            timeout = timer->timeout;
        }
    }

    context->timeout = timeout;
}

resource add_resource(
    loop_context* context,
    os::descriptor descriptor,
    const event_types events,
    resource_callback&& callback,
    void* user_ptr) {
    const auto it = context->descriptor_map.find(descriptor);
    if (it != context->descriptor_map.end()) {
        throw std::runtime_error("resource already added");
    }

    auto [handle, data] = context->resource_table.allocate_new();
    data->user_ptr = user_ptr;
    data->descriptor = descriptor;
    data->events = 0;
    data->callback = std::move(callback);

    looper_trace_debug(log_module, "adding resource: context=0x%x, handle=%lu, fd=%lu", context, handle, descriptor);

    auto [_, data_ptr] = context->resource_table.assign(handle, std::move(data));

    context->descriptor_map.emplace(descriptor, &data_ptr);
    context->updates.push_back({handle, update::type_add, events});

    signal_run(context);

    return handle;
}

void remove_resource(loop_context* context, const resource resource) {
    const auto data = context->resource_table.release(resource);

    looper_trace_debug(log_module, "removing resource: context=0x%x, handle=%lu", context, resource);

    context->descriptor_map.erase(data->descriptor);
    os::poll::remove(context->poller.get(), data->descriptor);

    signal_run(context);
}

void request_resource_events(
    loop_context* context,
    const resource resource,
    const event_types events,
    const events_update_type type) {
    const auto& data = context->resource_table[resource];

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

    looper_trace_debug(log_module, "modifying resource events: context=0x%x, handle=%lu, type=%d, events=%lu",
                       context, resource, static_cast<uint8_t>(update_type), events);

    context->updates.push_back({data.our_handle, update_type, events});
    signal_run(context);
}

void process_updates(loop_context* context) {
    while (!context->updates.empty()) {
        auto& update = context->updates.front();
        process_update(context, update);

        context->updates.pop_front();
    }
}

void process_events(loop_context* context, std::unique_lock<std::mutex>& lock, const size_t event_count) {
    for (int i = 0; i < event_count; i++) {
        auto& event_data = context->event_data[i];

        auto it = context->descriptor_map.find(event_data.descriptor);
        if (it == context->descriptor_map.end()) {
            // make sure to remove this fd, guess it somehow was left over
            looper_trace_debug(log_module, "resource received events, but isn't attached to anything: fd=%lu", event_data.descriptor);

            const auto status = os::poll::remove(context->poller.get(), event_data.descriptor);
            if (status != error_success) {
                looper_trace_error(log_module, "failed to modify poller: code=%lu", status);
                std::abort();
            }

            continue;
        }

        const auto* resource_data = it->second;

        if ((event_data.events & (event_error | event_hung)) != 0) {
            // we got an error on the resource, push it into the resource handler by marking
            // other flags as active and let the syscalls handle it then
            event_data.events |= resource_data->events & (event_out | event_in);
        }

        const auto adjusted_flags = (resource_data->events & event_data.events);
        if (adjusted_flags == 0) {
            continue;
        }

        looper_trace_debug(log_module, "resource has events: context=0x%x, handle=%lu, events=%lu",
                           context, resource_data->our_handle, adjusted_flags);

        invoke_func(lock, "resource_callback",
                    resource_data->callback, context, resource_data->our_handle, resource_data->user_ptr, adjusted_flags);
    }
}

}
