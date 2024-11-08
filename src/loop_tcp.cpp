
#include "os/except.h"

#include "loop_internal.h"


namespace looper::impl {

#define log_module loop_log_module "_tcp"

// todo: we want os layer to be with errors and not exceptions to make sure we catch all errors.

static void tcp_resource_handle_connecting(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp, event_types events) {
    if ((events & event_out) != 0) {
        // finish connect
        looper::error error = 0;
        try {
            tcp->socket_obj->finalize_connect();
            tcp->state = tcp_data::state::connected;

            looper_trace_debug(log_module, "tcp connect finish: ptr=0x%x", tcp);
        } catch (const os_exception& e) {
            tcp->state = tcp_data::state::errored;
            error = e.get_code();

            looper_trace_error(log_module, "tcp connect error: ptr=0x%x, code=%lu", tcp, e.get_code());
        }

        request_resource_events(context, tcp->resource, event_out, events_update_type::remove);
        invoke_func<std::mutex, tcp_data*, looper::error>
                (lock, "tcp_loop_callback", tcp->l_connect_callback, tcp, error);
    }
}

static void tcp_resource_handle_connected_read(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp) {
    if (!tcp->reading) {
        request_resource_events(context, tcp->resource, event_in, events_update_type::remove);
        return;
    }

    std::span<const uint8_t> data{};
    looper::error error = 0;
    try {
        auto read = tcp->socket_obj->read(context->m_read_buffer, sizeof(context->m_read_buffer));
        data = std::span<const uint8_t>{context->m_read_buffer, read};

        looper_trace_debug(log_module, "tcp read new data: ptr=0x%x, data_size=%lu", tcp, data.size());
    } catch (const os_exception& e) {
        error = e.get_code();
        looper_trace_error(log_module, "tcp read error: ptr=0x%x, code=%lu", tcp, e.get_code());
    }

    invoke_func<std::mutex, tcp_data*, std::span<const uint8_t>, looper::error>
            (lock, "tcp_loop_callback", tcp->l_read_callback, tcp, data, error);
}

static void tcp_resource_handle_connected_write(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp) {
    if (!tcp->write_pending) {
        request_resource_events(context, tcp->resource, event_out, events_update_type::remove);
        return;
    }

    auto& request = tcp->write_requests.front();

    try {
        // todo: better use of queues
        // todo: use iovec
        // todo: catch certain errors to try again: WOULDBLOCK, INTERRUPT
        auto written = tcp->socket_obj->write(request.buffer.get() + request.pos, request.size);
        request.pos += written;

        if (request.pos < request.size) {
            // didn't finish write
            return;
        }

        tcp->write_requests.pop_front();
        if (tcp->write_requests.empty()) {
            tcp->write_pending = false;
            request_resource_events(context, tcp->resource, event_out, events_update_type::remove);
        }

        looper_trace_debug(log_module, "tcp write request finished: ptr=0x%x", tcp);
        invoke_func<std::mutex, tcp_data*, tcp_data::write_request&, looper::error>
                (lock, "tcp_loop_callback", tcp->l_write_callback, tcp, request, 0);
    } catch (const os_exception& e) {
        looper_trace_error(log_module, "tcp write request failed: ptr=0x%x, code=%lu", tcp, e.get_code());
        invoke_func<std::mutex, tcp_data*, tcp_data::write_request&, looper::error>
                (lock, "tcp_loop_callback", tcp->l_write_callback, tcp, request, e.get_code());
    }
}

static void tcp_resource_handle_connected(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp, event_types events) {
    if ((events & (event_error | event_hung)) != 0) {
        // hung/error
        tcp->state = tcp_data::state::errored;

        const auto code = tcp->socket_obj->get_internal_error();
        looper_trace_error(log_module, "tcp hung/error: ptr=0x%x, code=%lu", tcp, code);
        invoke_func<>(lock, "tcp_loop_callback", tcp->l_error_callback, tcp, code);

        return;
    }

    if ((events & event_in) != 0) {
        // new data
        tcp_resource_handle_connected_read(lock, context, tcp);
    }

    if ((events & event_out) != 0) {
        tcp_resource_handle_connected_write(lock, context, tcp);
    }
}

static void tcp_resource_handler(loop_context* context, void* ptr, event_types events) {
    std::unique_lock lock(context->m_mutex);

    auto* data = reinterpret_cast<tcp_data*>(ptr);
    if (data->resource == empty_handle) {
        return;
    }

    switch (data->state) {
        case tcp_data::state::connecting:
            tcp_resource_handle_connecting(lock, context, data, events);
            break;
        case tcp_data::state::connected:
            tcp_resource_handle_connected(lock, context, data, events);
            break;
        default:
            break;
    }
}

static void tcp_server_resource_handler(loop_context* context, void* ptr, event_types events) {
    std::unique_lock lock(context->m_mutex);

    auto* tcp = reinterpret_cast<tcp_server_data*>(ptr);
    if (tcp->resource == empty_handle) {
        return;
    }

    if ((events & (event_error | event_hung)) != 0) {
        // hung/error
        const auto code = tcp->socket_obj->get_internal_error();
        looper_trace_error(log_module, "tcp hung/error: ptr=0x%x, code=%lu", tcp, code);

        return;
    }

    if ((events & event_in) != 0) {
        // new data
        invoke_func(lock, "tcp_server_callback", tcp->callback, tcp);
    }
}

void add_tcp(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    auto resource = add_resource(context, tcp->socket_obj, event_error | event_hung, tcp_resource_handler, tcp);
    tcp->resource = resource;

    looper_trace_info(log_module, "adding tcp: ptr=0x%x, resource_handle=%lu", tcp, resource);
}

void remove_tcp(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "removing tcp: ptr=0x%x, resource_handle=%lu", tcp, tcp->resource);

    if (tcp->resource != empty_handle) {
        remove_resource(context, tcp->resource);
        tcp->resource = empty_handle;
    }
}

void connect_tcp(loop_context* context, tcp_data* tcp, std::string_view server_address, uint16_t server_port) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->resource == empty_handle) {
        throw std::runtime_error("tcp has bad resource handle");
    }
    if (tcp->state != tcp_data::state::open) {
        throw std::runtime_error("tcp state invalid for connect");
    }

    looper_trace_info(log_module, "connecting tcp: ptr=0x%x", tcp);

    tcp->state = tcp_data::state::connecting;
    try {
        if (tcp->socket_obj->connect(server_address, server_port)) {
            // connection finished
            tcp->state = tcp_data::state::connected;

            looper_trace_info(log_module, "connected tcp inplace: ptr=0x%x", tcp);
            invoke_func<>(lock, "tcp_loop_callback", tcp->l_connect_callback, tcp, 0);
        } else {
            // wait for connection finish
            request_resource_events(context, tcp->resource, event_out, events_update_type::append);

            looper_trace_info(log_module, "tcp connection not finished: ptr=0x%x", tcp);
        }
    } catch (const os_exception& e) {
        tcp->state = tcp_data::state::errored;

        looper_trace_error(log_module, "tcp connection failed: ptr=0x%x, code=%s", tcp, e.get_code());
        invoke_func<>(lock, "tcp_loop_callback", tcp->l_connect_callback, tcp, e.get_code());
    }
}

void start_tcp_read(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->resource == empty_handle) {
        throw std::runtime_error("tcp has bad resource handle");
    }
    if (tcp->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for read");
    }
    if (tcp->reading) {
        throw std::runtime_error("tcp already reading");
    }

    looper_trace_info(log_module, "tcp starting read: ptr=0x%x", tcp);

    request_resource_events(context, tcp->resource, event_in, events_update_type::append);
    tcp->reading = true;
}

void stop_tcp_read(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->resource == empty_handle) {
        throw std::runtime_error("tcp has bad resource handle");
    }
    if (tcp->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for read");
    }
    if (!tcp->reading) {
        return;
    }

    looper_trace_info(log_module, "tcp stopping read: ptr=0x%x", tcp);

    tcp->reading = false;
    request_resource_events(context, tcp->resource, event_in, events_update_type::remove);
}

void write_tcp(loop_context* context, tcp_data* tcp, tcp_data::write_request&& request) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->resource == empty_handle) {
        throw std::runtime_error("tcp has bad resource handle");
    }
    if (tcp->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for write");
    }

    looper_trace_info(log_module, "tcp writing: ptr=0x%x, buffer_size=%lu", tcp, request.size);

    tcp->write_requests.push_back(std::move(request));

    if (!tcp->write_pending) {
        request_resource_events(context, tcp->resource, event_out, events_update_type::append);
        tcp->write_pending = true;
    }
}

void add_tcp_server(loop_context* context, tcp_server_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    auto resource = add_resource(context, tcp->socket_obj, event_in | event_error | event_hung, tcp_server_resource_handler, tcp);
    tcp->resource = resource;

    looper_trace_info(log_module, "adding tcp server: ptr=0x%x, resource_handle=%lu", tcp, resource);
}

void remove_tcp_server(loop_context* context, tcp_server_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    looper_trace_info(log_module, "removing tcp server: ptr=0x%x, resource_handle=%lu", tcp, tcp->resource);

    if (tcp->resource != empty_handle) {
        remove_resource(context, tcp->resource);
        tcp->resource = empty_handle;
    }
}

}
