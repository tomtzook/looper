
#include "os/except.h"

#include "loop_internal.h"


namespace looper::impl {

#define log_module loop_log_module "_tcp"

static void tcp_resource_handle_connecting(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp_data, event_types events) {
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

        request_resource_events(context, tcp_data->resource, event_out, events_update_type::remove);

        auto callback = std::move(tcp_data->connect_callback);
        tcp_data->connect_callback = nullptr;

        invoke_func<std::mutex, looper::loop, looper::tcp, looper::error>(
                lock,
                "tcp_connect_callback",
                callback,
                context->m_handle,
                tcp_data->handle,
                error);
    }
}

static void tcp_resource_handle_connected_read(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp_data) {
    if (!tcp_data->reading) {
        request_resource_events(context, tcp_data->resource, event_in, events_update_type::remove);
        return;
    }

    std::span<const uint8_t> data{};
    error_t error = 0;
    try {
        auto read = tcp_data->socket_obj->read(context->m_read_buffer, sizeof(context->m_read_buffer));
        data = std::span<const uint8_t>{context->m_read_buffer, read};
    } catch (const os_exception& e) {
        error = e.get_code();
    } catch (const os::eof_exception& e) {
        // todo: pass EOF ERROR CODE
        error = -1;
    }

    invoke_func<std::mutex, looper::loop, looper::tcp, std::span<const uint8_t>, looper::error>(
            lock,
            "tcp_read_callback",
            tcp_data->read_callback,
            context->m_handle,
            tcp_data->handle,
            data,
            error);
}

static void tcp_resource_handle_connected_write(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp_data) {
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

static void tcp_resource_handle_connected(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp_data, event_types events) {
    // todo: handle hung/error events
    if ((events & event_in) != 0) {
        // new data
        tcp_resource_handle_connected_read(lock, context, tcp_data);
    }

    if ((events & event_out) != 0) {
        tcp_resource_handle_connected_write(lock, context, tcp_data);
    }
}

static void tcp_resource_handler(loop_context* context, looper::impl::resource resource, handle handle, event_types events) {
    std::unique_lock lock(context->m_mutex);
    if (!context->m_tcp_table.has(handle)) {
        return;
    }

    auto tcp_data = context->m_tcp_table[handle];
    if (tcp_data->resource == empty_handle) {
        return;
    }

    switch (tcp_data->state) {
        case tcp_data::state::connecting:
            tcp_resource_handle_connecting(lock, context, tcp_data, events);
            break;
        case tcp_data::state::connected:
            tcp_resource_handle_connected(lock, context, tcp_data, events);
            break;
        default:
            break;
    }
}

tcp create_tcp(loop_context* context) {
    std::unique_lock lock(context->m_mutex);

    auto socket = os::create_tcp_socket();

    auto [handle, data] = context->m_tcp_table.allocate_new();
    data->socket_obj = socket;
    data->state = tcp_data::state::init;

    looper_trace_info(log_module, "creating tcp: handle=%lu", handle);

    // todo: on failure of add_resource we will have the event in the table
    auto resource = add_resource(context, socket, 0, tcp_resource_handler, handle);
    data->resource = resource;
    data->state = tcp_data::state::open;

    context->m_tcp_table.assign(handle, std::move(data));

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
    if (data->reading) {
        throw std::runtime_error("tcp already reading");
    }

    data->read_callback = std::move(callback);
    request_resource_events(context, data->resource, event_in, events_update_type::append);
    data->reading = true;
}

void stop_tcp_read(loop_context* context, tcp tcp) {
    std::unique_lock lock(context->m_mutex);

    auto data = context->m_tcp_table[tcp];
    if (data->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for read");
    }
    if (!data->reading) {
        return;
    }

    data->reading = false;
    request_resource_events(context, data->resource, event_in, events_update_type::remove);
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

}
