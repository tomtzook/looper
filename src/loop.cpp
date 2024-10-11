
#include <looper_trace.h>

#include "os/except.h"
#include "os/factory.h"
#include "loop.h"

namespace looper::impl {

#define log_module "loop"

static constexpr size_t max_events_for_process = 20;
static constexpr auto initial_poll_timeout = std::chrono::milliseconds(1000);
static constexpr auto min_poll_timeout = std::chrono::milliseconds(100);

template<typename _mutex, typename... args_>
static void invoke_func(std::unique_lock<_mutex>& lock, const char* name, const std::function<void(args_...)>& ref, args_... args) {
    if (ref != nullptr) {
        lock.unlock();
        try {
            ref(args...);
        } catch (const std::exception& e) {
            looper_trace_error(log_module, "Error while invoking func %s: what=%s", name, e.what());
        } catch (...) {
            looper_trace_error(log_module, "Error while invoking func %s: unknown", name);
        }
        lock.lock();
    }
}

static std::chrono::milliseconds time_now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch());
}

static void signal_run(loop_context* context) {
    context->m_run_loop_event->set();
}

static resource add_resource(loop_context* context, std::shared_ptr<os::resource> resource, event_types events, resource_callback&& callback) {
    const auto descriptor = resource->get_descriptor();

    auto it = context->m_descriptor_map.find(descriptor);
    if (it != context->m_descriptor_map.end()) {
        throw std::runtime_error("resource already added");
    }

    auto [handle, data] = context->m_resource_table.allocate_new();
    data->resource_obj = std::move(resource);
    data->descriptor = descriptor;
    data->events = 0;
    data->callback = std::move(callback);

    context->m_descriptor_map.emplace(descriptor, data);
    context->m_updates.push_back({handle, update::type_add, events});

    signal_run(context);

    return handle;
}

static void remove_resource(loop_context* context, resource resource) {
    auto data = context->m_resource_table.release(resource);

    context->m_descriptor_map.erase(data->descriptor);
    context->m_poller->remove(data->descriptor);

    signal_run(context);
}

static void request_resource_events(loop_context* context, resource resource, event_types events, events_update_type type = events_update_type::override) {
    auto data = context->m_resource_table[resource];

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

    context->m_updates.push_back({data->handle, update_type, events});
    signal_run(context);
}

static void process_update(loop_context* context, update& update) {
    if (!context->m_resource_table.has(update.handle)) {
        return;
    }

    auto data = context->m_resource_table[update.handle];

    switch (update.type) {
        case update::type_add:
            data->events = update.events;
            context->m_poller->add(data->descriptor, data->events);
            break;
        case update::type_new_events:
            data->events = update.events;
            context->m_poller->set(data->descriptor, data->events);
            break;
        case update::type_new_events_add:
            data->events |= update.events;
            context->m_poller->set(data->descriptor, data->events);
            break;
        case update::type_new_events_remove:
            data->events &= ~update.events;
            context->m_poller->set(data->descriptor, data->events);
            break;
    }
}

static void process_updates(loop_context* context) {
    while (!context->m_updates.empty()) {
        auto& update = context->m_updates.front();
        process_update(context, update);

        context->m_updates.pop_front();
    }
}

static void process_events(loop_context* context, std::unique_lock<std::mutex>& lock, polled_events& events) {
    for (auto [descriptor, revents] : events) {
        auto it = context->m_descriptor_map.find(descriptor);
        if (it == context->m_descriptor_map.end()) {
            continue;
        }

        auto data = it->second;

        auto adjusted_flags = (data->events & revents);
        if (adjusted_flags == 0) {
            continue;
        }

        invoke_func(lock, "resource_callback", data->callback, context->m_handle, data->handle, adjusted_flags);
    }
}

static void process_timers(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<timer> to_remove;
    const auto now = time_now();

    for (auto [handle, data] : context->m_timer_table) {
        if (data.timeout.count() == 0) {
            to_remove.push_back(handle);
            continue;
        }

        if (!data.running || data.hit || data.next_timestamp > now) {
            continue;
        }

        data.hit = true;

        invoke_func(lock, "timer_callback", data.callback, context->m_handle, data.handle);
    }

    for (auto handle : to_remove) {
        context->m_timer_table.release(handle);
    }
}

static void process_futures(loop_context* context, std::unique_lock<std::mutex>& lock) {
    std::vector<future> to_remove;
    const auto now = time_now();

    for (auto [handle, data] : context->m_future_table) {
        if (data.remove) {
            to_remove.push_back(handle);
            continue;
        }

        if (data.finished || data.execute_time > now) {
            continue;
        }

        data.finished = true;

        invoke_func(lock, "future_callback", data.callback, context->m_handle, data.handle);
    }

    context->m_future_executed.notify_all();

    for (auto handle : to_remove) {
        context->m_future_table.release(handle);
    }
}

loop_context::loop_context(loop handle)
    : m_handle(handle)
    , m_mutex()
    , m_poller(os::create_poller())
    , m_timeout(initial_poll_timeout)
    , m_run_loop_event(os::create_event())
    , m_future_executed()
    , stop(false)
    , executing(false)
    , m_run_finished()
    , m_future_table(handles::handle{handle}.index(), handles::type_future)
    , m_event_table(handles::handle{handle}.index(), handles::type_event)
    , m_timer_table(handles::handle{handle}.index(), handles::type_timer)
    , m_tcp_table(handles::handle{handle}.index(), handles::type_tcp)
    , m_resource_table(handles::handle{handle}.index(), handles::type_resource)
    , m_descriptor_map()
    , m_updates()
    , m_read_buffer()
{}

loop_context* create_loop(loop handle) {
    looper_trace_info(log_module, "creating looper");

    auto context = new loop_context(handle);
    add_resource(context, context->m_run_loop_event, event_in, [context](loop loop, resource resource, event_types events)->void {
        context->m_run_loop_event->clear();
    });
    // todo: handle release on failure

    return context;
}

void destroy_loop(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "stopping looper");

    context->stop = true;
    signal_run(context);

    if (context->executing) {
        context->m_run_finished.wait(lock, [context]()->bool {
            return !context->executing;
        });
    }

    lock.unlock();
    // todo: there might still be a race here with someone trying to take the lock
    delete context;
}

// events
event create_event(loop_context* context, event_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto event = os::create_event();

    auto [handle, data] = context->m_event_table.allocate_new();
    data->event_obj = event;
    data->callback = std::move(callback);

    looper_trace_info(log_module, "creating new event: handle=%lu, fd=%d", handle, event->get_descriptor());

    // todo: on failure of add_resource we will have the event in the table
    auto resource_callback = [context, handle](looper::loop loop, looper::impl::resource resource, event_types events)->void {
        std::unique_lock lock(context->m_mutex);
        if (!context->m_event_table.has(handle)) {
            return;
        }

        auto event_data = context->m_event_table[handle];
        if (event_data->resource == empty_handle) {
            return;
        }

        invoke_func(lock, "event_callback", event_data->callback, context->m_handle, event_data->handle);
    };

    auto resource = add_resource(context, event, event_in, resource_callback);
    data->resource = resource;

    return handle;
}

void destroy_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table.release(event);
    remove_resource(context, data->resource);

    looper_trace_info(log_module, "destroying event: handle=%lu", event);
}

void set_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table[event];
    if (data->event_obj) {
        looper_trace_info(log_module, "setting event: handle=%lu", event);
        data->event_obj->set();
    }
}

void clear_event(loop_context* context, event event) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_event_table[event];
    if (data->event_obj) {
        looper_trace_info(log_module, "clearing event: handle=%lu", event);
        data->event_obj->clear();
    }
}

// timer
timer create_timer(loop_context* context, std::chrono::milliseconds timeout, timer_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    if (timeout < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    auto [handle, data] = context->m_timer_table.allocate_new();
    data->running = false;
    data->timeout = timeout;
    data->next_timestamp = std::chrono::milliseconds(0);
    data->callback = std::move(callback);

    looper_trace_info(log_module, "creating new timer: handle=%lu, timeout=%ul", handle, timeout.count());

    if (context->m_timeout > timeout) {
        context->m_timeout = timeout;
    }

    signal_run(context);

    return handle;
}

void destroy_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    // todo: find new smallest timeout

    // let the loop delete this to prevent sync problems with callbacks
    auto data = context->m_timer_table[timer];
    data->timeout = std::chrono::milliseconds(0);
    data->running = false;

    looper_trace_info(log_module, "destroying timer: handle=%lu", timer);
}

void start_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->next_timestamp = time_now() + data->timeout;
    data->hit = false;
    data->running = true;

    looper_trace_info(log_module, "starting timer: handle=%lu", timer);
}

void stop_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->running = false;

    looper_trace_info(log_module, "stopping timer: handle=%lu", timer);
}

void reset_timer(loop_context* context, timer timer) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_timer_table[timer];
    data->next_timestamp = time_now() + data->timeout;
    data->hit = false;

    looper_trace_info(log_module, "resetting timer: handle=%lu", timer);
}

// execute
future create_future(loop_context* context, future_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto [handle, data] = context->m_future_table.allocate_new();
    data->finished = true;
    data->remove = false;
    data->execute_time = std::chrono::milliseconds(0);
    data->callback = std::move(callback);

    looper_trace_info(log_module, "creating future: handle=%lu", handle);

    return handle;
}

void destroy_future(loop_context* context, future future) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_future_table[future];
    data->remove = true;

    looper_trace_info(log_module, "destroying future: handle=%lu", future);
}

void execute_later(loop_context* context, future future, std::chrono::milliseconds delay) {
    std::unique_lock lock(context->m_mutex);

    if (delay.count() != 0 && delay < min_poll_timeout) {
        throw std::runtime_error("timer timeout too small");
    }

    // todo: find new smallest timeout when finished
    auto data = context->m_future_table[future];
    data->execute_time = time_now() + delay;
    data->finished = false;

    if (delay.count() < 1) {
        signal_run(context);
    }

    looper_trace_info(log_module, "executing for future: handle=%lu, delay=%ul", future, delay.count());
}

bool wait_for(loop_context* context, future future, std::chrono::milliseconds timeout) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "waiting on future: handle=%lu, timeout=%ul", future, timeout.count());

    if (!context->m_future_table.has(future)) {
        // in case someone is waiting on a removed future because of the race with the callback which removed it
        looper_trace_debug(log_module, "future does not exist, returning");
        return false;
    }

    auto data = context->m_future_table[future];
    if (data->finished) {
        looper_trace_debug(log_module, "future finished, returning");
        return false;
    }

    return !context->m_future_executed.wait_for(lock, timeout, [context, future]()->bool {
        if (!context->m_future_table.has(future)) {
            return true;
        }

        auto data = context->m_future_table[future];
        if (data->finished || data->remove) {
            return true;
        }

        return false;
    });
}

// tcp
tcp create_tcp(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    auto socket = os::create_tcp_socket();

    auto [handle, data] = context->m_tcp_table.allocate_new();
    data->socket_obj = socket;
    data->state = tcp_data::state::init;

    looper_trace_info(log_module, "creating tcp: handle=%lu", handle);

    // todo: on failure of add_resource we will have the event in the table
    auto resource_callback = [context, handle](looper::loop loop, looper::impl::resource resource, event_types events)->void {
        std::unique_lock lock(context->m_mutex);
        if (!context->m_tcp_table.has(handle)) {
            return;
        }

        auto tcp_data = context->m_tcp_table[handle];
        if (tcp_data->resource == empty_handle) {
            return;
        }

        switch (tcp_data->state) {
            case tcp_data::state::connecting: {
                if ((events & event_out) != 0) {
                    // finish connect
                    looper::error error = 0;
                    try {
                        tcp_data->socket_obj->finalize_connect();
                        tcp_data->state = tcp_data::state::connected;
                    } catch (const os_exception& e) {
                        tcp_data->state = tcp_data::state::errored;
                        error = e.get_code();
                    }

                    // todo: clear callbacks and then use
                    request_resource_events(context, tcp_data->resource, 0, events_update_type::remove);
                    invoke_func<std::mutex, looper::loop, looper::tcp, looper::error>(
                            lock,
                            "tcp_connect_callback",
                            tcp_data->connect_callback,
                            context->m_handle,
                            tcp_data->handle,
                            error);
                }
            }
            case tcp_data::state::connected: {
                // todo: handle hung/error events
                if ((events & event_in) != 0) {
                    // new data
                    try {
                        auto read = tcp_data->socket_obj->read(context->m_read_buffer, sizeof(context->m_read_buffer));
                        auto span = std::span<const uint8_t>{context->m_read_buffer, read};
                        invoke_func<std::mutex, looper::loop, looper::tcp, std::span<const uint8_t>, looper::error>(
                                lock,
                                "tcp_read_callback",
                                tcp_data->read_callback,
                                context->m_handle,
                                tcp_data->handle,
                                span,
                                0);
                    } catch (const os_exception& e) {
                        invoke_func<std::mutex, looper::loop, looper::tcp, std::span<const uint8_t>, looper::error>(
                                lock,
                                "tcp_read_callback",
                                tcp_data->read_callback,
                                context->m_handle,
                                tcp_data->handle,
                                std::span<const uint8_t>{},
                                e.get_code());
                    } catch (const os::eof_exception& e) {
                        // todo: pass EOF ERROR CODE
                        invoke_func<std::mutex, looper::loop, looper::tcp, std::span<const uint8_t>, looper::error>(
                                lock,
                                "tcp_read_callback",
                                tcp_data->read_callback,
                                context->m_handle,
                                tcp_data->handle,
                                std::span<const uint8_t>{},
                                -1);
                    }
                }

                if ((events & event_out) != 0) {
                     if (tcp_data->write_pending) {
                         tcp_data->write_pending = false;
                         invoke_func<std::mutex, looper::loop, looper::tcp, looper::error>(
                                 lock,
                                 "tcp_write_callback",
                                 tcp_data->write_callback,
                                 context->m_handle,
                                 tcp_data->handle,
                                 0);
                     }

                    request_resource_events(context, tcp_data->resource, event_out, events_update_type::remove);
                }
            }
            default:
                break;
        }
    };

    auto resource = add_resource(context, socket, 0, resource_callback);
    data->resource = resource;
    data->state = tcp_data::state::open;

    return handle;
}

void destroy_tcp(loop_context* context, tcp tcp) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_tcp_table.release(tcp);
    if (data->resource != empty_handle) {
        remove_resource(context, data->resource);
    }

    // todo: race!
    if (data->socket_obj) {
        data->socket_obj->close();
        data->socket_obj.reset();
    }
}

void bind_tcp(loop_context* context, tcp tcp, uint16_t port) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_tcp_table[tcp];
    if (data->state != tcp_data::state::open) {
        throw std::runtime_error("tcp state invalid for bind");
    }

    data->socket_obj->bind(port);
}

void connect_tcp(loop_context* context, tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_tcp_table[tcp];
    if (data->state != tcp_data::state::open) {
        throw std::runtime_error("tcp state invalid for connect");
    }

    data->state = tcp_data::state::connecting;
    try {
        if (data->socket_obj->connect(server_address, server_port)) {
            // connection finished
            data->state = tcp_data::state::connected;
            invoke_func<std::mutex, looper::loop, looper::tcp, looper::error>(
                    lock,
                    "tcp_connect_callback",
                    callback,
                    context->m_handle,
                    data->handle,
                    0);
        } else {
            // wait for connection finish
            request_resource_events(context, data->resource, event_out, events_update_type::append);
            data->connect_callback = std::move(callback);
        }
    } catch (const os_exception& e) {
        data->state = tcp_data::state::errored;
        invoke_func<std::mutex, looper::loop, looper::tcp, looper::error>(
                lock,
                "tcp_connect_callback",
                callback,
                context->m_handle,
                data->handle,
                e.get_code());
    }
}

void start_tcp_read(loop_context* context, tcp tcp, tcp_read_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_tcp_table[tcp];
    if (data->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for read");
    }

    data->read_callback = std::move(callback);
    request_resource_events(context, data->resource, event_in, events_update_type::append);
}

void write_tcp(loop_context* context, tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_tcp_table[tcp];
    if (data->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for write");
    }

    if (data->write_pending) {
        throw std::runtime_error("write already in progress");
    }

    try {
        auto written = data->socket_obj->write(buffer.data(), buffer.size());
        if (written != buffer.size()) {
            // todo: handle internally
            throw std::runtime_error("unable to write all buffer! implement solution");
        }

        request_resource_events(context, data->resource, event_out, events_update_type::append);

        data->write_pending = true;
        data->write_callback = std::move(callback);
    } catch (const os_exception& e) {
        invoke_func<std::mutex, looper::loop, looper::tcp, looper::error>(
                lock,
                "tcp_write_callback",
                data->write_callback,
                context->m_handle,
                data->handle,
                e.get_code());
    }
}

// run
bool run_once(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    if (context->stop) {
        looper_trace_debug(log_module, "looper marked stop, not running");
        return true;
    }

    context->executing = true;
    looper_trace_debug(log_module, "start looper run");

    process_updates(context);

    lock.unlock();
    auto result = context->m_poller->poll(max_events_for_process, context->m_timeout);
    lock.lock();

    process_events(context, lock, result);
    process_timers(context, lock);
    process_futures(context, lock);

    looper_trace_debug(log_module, "finish looper run");
    context->executing = false;
    context->m_run_finished.notify_all();

    return context->stop;
}

void run_forever(loop_context* context) {
    while (true) {
        bool finished = run_once(context);
        if (finished) {
            break;
        }
    }
}

}
