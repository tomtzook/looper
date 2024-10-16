
#include "os/except.h"

#include "loop_internal.h"


namespace looper::impl {

#define log_module loop_log_module "_tcp"

static void tcp_resource_handle_connecting(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp, event_types events) {
    if ((events & event_out) != 0) {
        // finish connect
        looper::error error = 0;
        try {
            tcp->socket_obj->finalize_connect();
            tcp->state = tcp_data::state::connected;
        } catch (const os_exception& e) {
            tcp->state = tcp_data::state::errored;
            error = e.get_code();
        }

        request_resource_events(context, tcp->resource, event_out, events_update_type::remove);

        tcp_data::cause_data cause_data{};
        invoke_func<std::mutex, tcp_data*, tcp_data::cause, tcp_data::cause_data&, looper::error>
                (lock, "tcp_loop_callback", tcp->callback, tcp, tcp_data::cause::connect, cause_data, error);
    }
}

static void tcp_resource_handle_connected_read(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp) {
    if (!tcp->reading) {
        request_resource_events(context, tcp->resource, event_in, events_update_type::remove);
        return;
    }

    std::span<const uint8_t> data{};
    error_t error = 0;
    try {
        auto read = tcp->socket_obj->read(context->m_read_buffer, sizeof(context->m_read_buffer));
        data = std::span<const uint8_t>{context->m_read_buffer, read};
    } catch (const os_exception& e) {
        error = e.get_code();
    } catch (const os::eof_exception& e) {
        // todo: pass EOF ERROR CODE
        error = -1;
    }

    tcp_data::cause_data cause_data{};
    cause_data.read.data = data;
    invoke_func<std::mutex, tcp_data*, tcp_data::cause, tcp_data::cause_data&, looper::error>
            (lock, "tcp_loop_callback", tcp->callback, tcp, tcp_data::cause::read, cause_data, error);
}

static void tcp_resource_handle_connected_write(std::unique_lock<std::mutex>& lock, loop_context* context, tcp_data* tcp) {
    if (tcp->write_pending) {
        tcp->write_pending = false;

        tcp_data::cause_data cause_data{};
        invoke_func<std::mutex, tcp_data*, tcp_data::cause, tcp_data::cause_data&, looper::error>
                (lock, "tcp_loop_callback", tcp->callback, tcp, tcp_data::cause::write_finished, cause_data, 0);
    }

    request_resource_events(context, tcp->resource, event_out, events_update_type::remove);
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

void add_tcp(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    auto resource = add_resource(context, tcp->socket_obj, 0, tcp_resource_handler, tcp);
    tcp->resource = resource;
}

void remove_tcp(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->resource != empty_handle) {
        remove_resource(context, tcp->resource);
        tcp->resource = empty_handle;
    }
}

void connect_tcp(loop_context* context, tcp_data* tcp, std::string_view server_address, uint16_t server_port) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->state != tcp_data::state::open) {
        throw std::runtime_error("tcp state invalid for connect");
    }

    tcp->state = tcp_data::state::connecting;
    try {
        if (tcp->socket_obj->connect(server_address, server_port)) {
            // connection finished
            tcp->state = tcp_data::state::connected;

            tcp_data::cause_data cause_data{};
            invoke_func<std::mutex, tcp_data*, tcp_data::cause, tcp_data::cause_data&, looper::error>
                    (lock, "tcp_loop_callback", tcp->callback, tcp, tcp_data::cause::connect, cause_data, 0);
        } else {
            // wait for connection finish
            request_resource_events(context, tcp->resource, event_out, events_update_type::append);
        }
    } catch (const os_exception& e) {
        tcp->state = tcp_data::state::errored;

        tcp_data::cause_data cause_data{};
        invoke_func<std::mutex, tcp_data*, tcp_data::cause, tcp_data::cause_data&, looper::error>
                (lock, "tcp_loop_callback", tcp->callback, tcp, tcp_data::cause::connect, cause_data, e.get_code());
    }
}

void start_tcp_read(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for read");
    }
    if (tcp->reading) {
        throw std::runtime_error("tcp already reading");
    }

    request_resource_events(context, tcp->resource, event_in, events_update_type::append);
    tcp->reading = true;
}

void stop_tcp_read(loop_context* context, tcp_data* tcp) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for read");
    }
    if (!tcp->reading) {
        return;
    }

    tcp->reading = false;
    request_resource_events(context, tcp->resource, event_in, events_update_type::remove);
}

void write_tcp(loop_context* context, tcp_data* tcp, std::span<const uint8_t> buffer) {
    std::unique_lock lock(context->m_mutex);

    if (tcp->state != tcp_data::state::connected) {
        throw std::runtime_error("tcp state invalid for write");
    }
    if (tcp->write_pending) {
        throw std::runtime_error("write already in progress");
    }

    try {
        auto written = tcp->socket_obj->write(buffer.data(), buffer.size());
        if (written != buffer.size()) {
            // todo: handle internally
            throw std::runtime_error("unable to write all buffer! implement solution");
        }

        request_resource_events(context, tcp->resource, event_out, events_update_type::append);
        tcp->write_pending = true;
    } catch (const os_exception& e) {
        tcp_data::cause_data cause_data{};
        invoke_func<std::mutex, tcp_data*, tcp_data::cause, tcp_data::cause_data&, looper::error>
            (lock, "tcp_loop_callback", tcp->callback, tcp, tcp_data::cause::write_finished, cause_data, e.get_code());
    }
}

}
