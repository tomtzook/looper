
#include "instance.h"

namespace looper {

#define log_module "looper_instance"

looper_data g_instance;

static void event_resource_callback(loop loop, resource resource, event_types events) {
    auto data = g_instance.m_loops[loop];
    auto opt = data->get_handle_from_resource(resource, handles::type_event);
    if (!opt) {
        return;
    }

    auto event_handle = opt.value();
    auto event_data = data->get_event(event_handle);

    event_data->callback(loop, event_handle);
}

loop_data::loop_data(handle handle)
    : m_loop(handle)
    , m_events(handles::handle{handle}.index(), handles::type_event)
    , m_resource_ptrs()
    , m_resource_callbacks() {

    m_resource_callbacks[handles::type_event] = event_resource_callback;
}

event_handle_data* loop_data::get_event(event event) {
    return m_events[event];
}

std::optional<handle> loop_data::get_handle_from_resource(resource resource, handles::handle_types type) {
    auto it = m_resource_ptrs.find(resource);
    if (it == m_resource_ptrs.end()) {
        return std::nullopt;
    }

    if (it->second.type != type) {
        throw std::runtime_error("requested resource is of a different handle than expected");
    }

    return it->second.handle;
}

event loop_data::add_event(std::shared_ptr<os::event> event, event_callback&& callback) {
    auto [handle, event_data] = m_events.allocate_new();
    event_data->event = std::move(event);
    event_data->callback = std::move(callback);

    event_data->resource = add_resource(handle, event_data->event, event_in);

    return handle;
}

void loop_data::remove_event(event event) {
    auto data = m_events[event];
    remove_resource(data->resource);

    m_events.release(event);
}

void loop_data::execute_later(execute_callback&& callback, bool wait) {
    m_loop.execute_later(std::move(callback), wait);
}

void loop_data::run() {
    m_loop.run_once();
}

resource loop_data::add_resource(looper::handle handle, std::shared_ptr<os::resource> resource, looper::event_types events) {
    handles::handle handle_full(handle);
    const auto type = handle_full.type();

    auto callback = m_resource_callbacks[type];
    if (callback == nullptr) {
        looper_trace_error(log_module, "resource callback for type %u is null", type);
        std::abort();
    }

    const auto resource_handle = m_loop.add_resource(std::move(resource), events, resource_callback(callback));

    resource_ptr ptr{static_cast<handles::handle_types>(type), handle};
    m_resource_ptrs.emplace(resource_handle, ptr);

    return resource_handle;
}

void loop_data::remove_resource(resource resource) {
    m_resource_ptrs.erase(resource);
    m_loop.remove_resource(resource);
}

}
