
#include "loop_internal.h"

namespace looper::impl {

#define log_module loop_log_module

loop_context::loop_context()
        : m_mutex()
        , m_poller(os::create_poller())
        , m_timeout(initial_poll_timeout)
        , m_run_loop_event(os::create_event())
        , stop(false)
        , executing(false)
        , m_run_finished()
        , m_resource_table(0, handles::type_resource)
        , m_descriptor_map()
        , m_futures()
        , m_timers()
        , m_updates()
        , m_timer_call_holder()
        , m_future_call_holder()
        , m_read_buffer() {
    m_updates.resize(initial_reserve_size);
    m_timer_call_holder.reserve(initial_reserve_size);
    m_future_call_holder.reserve(initial_reserve_size);
}

std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
}

void signal_run(loop_context* context) {
    looper_trace_debug(log_module, "signalling loop run: context=0x%x", context);

    context->m_run_loop_event->set();
}

resource add_resource(loop_context* context, std::shared_ptr<os::resource> resource,
                      event_types events, resource_callback&& callback,
                      void* user_ptr) {
    const auto descriptor = resource->get_descriptor();

    auto it = context->m_descriptor_map.find(descriptor);
    if (it != context->m_descriptor_map.end()) {
        throw std::runtime_error("resource already added");
    }

    auto [handle, data] = context->m_resource_table.allocate_new();
    data->user_ptr = user_ptr;
    data->resource_obj = std::move(resource);
    data->descriptor = descriptor;
    data->events = 0;
    data->callback = std::move(callback);

    looper_trace_debug(log_module, "adding resource: context=0x%x, handle=%lu, fd=%lu", context, handle, descriptor);

    auto [_, data_ptr] = context->m_resource_table.assign(handle, std::move(data));

    context->m_descriptor_map.emplace(descriptor, &data_ptr);
    context->m_updates.push_back({handle, update::type_add, events});

    signal_run(context);

    return handle;
}

void remove_resource(loop_context* context, resource resource) {
    auto data = context->m_resource_table.release(resource);

    looper_trace_debug(log_module, "removing resource: context=0x%x, handle=%lu", context, resource);

    context->m_descriptor_map.erase(data->descriptor);
    context->m_poller->remove(data->descriptor);

    signal_run(context);
}

void request_resource_events(loop_context* context, resource resource, event_types events, events_update_type type) {
    auto& data = context->m_resource_table[resource];

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

    context->m_updates.push_back({data.our_handle, update_type, events});
    signal_run(context);
}


}
