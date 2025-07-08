
#include "loop_internal.h"


namespace looper::impl {

#define log_module loop_log_module "_udp"

static void udp_resource_handle_read(std::unique_lock<std::mutex>& lock, loop_context* context, udp_data* udp) {
    if (!udp->reading) {
        request_resource_events(context, udp->resource, event_in, events_update_type::remove);
        return;
    }

    char ip_buff[64]{};
    uint16_t port;
    std::span<const uint8_t> data{};
    size_t read;
    auto error = os::udp::read(udp->socket_obj.get(), context->read_buffer, sizeof(context->read_buffer), read, ip_buff, sizeof(ip_buff), port);
    if (error == error_success) {
        data = std::span<const uint8_t>{context->read_buffer, read};
        looper_trace_debug(log_module, "udp read new data: ptr=0x%x, data_size=%lu", udp, data.size());
    } else {
        udp->state = udp_data::state::errored;
        looper_trace_error(log_module, "udp read error: ptr=0x%x, code=%lu", udp, error);
    }

    invoke_func<std::mutex, udp_data*, const inet_address&, std::span<const uint8_t>, looper::error>
            (lock, "udp_loop_callback", udp->from_loop_read_callback, udp, {ip_buff, port}, data, error);
}

static bool do_udp_write(udp_data* udp) {
    // todo: better use of queues
    // todo: use iovec

    static constexpr size_t max_writes_to_do_in_one_iteration = 16;

    // only write up to 16 requests, so as not to starve the loop with writes
    size_t write_count = max_writes_to_do_in_one_iteration;

    while (!udp->write_requests.empty() && (write_count--) > 0) {
        auto& request = udp->write_requests.front();

        bool did_fail = false;
        size_t written;
        auto error = os::udp::write(
            udp->socket_obj.get(), request.destination.ip, request.destination.port,
            request.buffer.get(), request.size, written);
        if (error == error_success) {
            if (written < request.size) {
                looper_trace_error(log_module, "udp write did not write entire request ptr=0x%x", udp);
                did_fail = true;
                error = error_interrupted;
            } else {
                looper_trace_debug(log_module, "udp write request finished: ptr=0x%x", udp);
                request.error = error_success;

                udp->completed_write_requests.push_back(std::move(request));
                udp->write_requests.pop_front();
            }
        } else if (error == error_in_progress || error == error_again) {
            // didn't finish write, but need to try again later
            return true;
        } else {
            did_fail = true;
        }

        if (did_fail) {
            looper_trace_error(log_module, "udp write request failed: ptr=0x%x, code=%lu", udp, error);
            request.error = error;

            udp->completed_write_requests.push_back(std::move(request));
            udp->write_requests.pop_front();

            return false;
        }
    }

    return true;
}

static void report_write_requests_finished(std::unique_lock<std::mutex>& lock, udp_data* udp) {
    while (!udp->completed_write_requests.empty()) {
        auto& request = udp->completed_write_requests.front();
        invoke_func<std::mutex, udp_data*, udp_data::write_request&>
                (lock, "udp_loop_callback", udp->from_loop_write_callback, udp, request);

        udp->completed_write_requests.pop_front();
    }
}

static void udp_resource_handle_write(std::unique_lock<std::mutex>& lock, loop_context* context, udp_data* udp) {
    if (!udp->write_pending) {
        request_resource_events(context, udp->resource, event_out, events_update_type::remove);
        return;
    }

    if (!do_udp_write(udp)) {
        udp->state = udp_data::state::errored;
        udp->write_pending = false;
        request_resource_events(context, udp->resource, event_out, events_update_type::remove);
    } else {
        if (udp->write_requests.empty()) {
            udp->write_pending = false;
            request_resource_events(context, udp->resource, event_out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock, udp);
}

static void udp_resource_handler(loop_context* context, void* ptr, event_types events) {
    std::unique_lock lock(context->mutex);

    auto* data = static_cast<udp_data*>(ptr);
    if (data->resource == empty_handle) {
        return;
    }

    switch (data->state) {
        case udp_data::state::open: {
            if ((events & event_in) != 0) {
                // new data
                udp_resource_handle_read(lock, context, data);
            }

            if ((events & event_out) != 0) {
                udp_resource_handle_write(lock, context, data);
            }

            break;
        }
        default:
            break;
    }
}

void add_udp(loop_context* context, udp_data* udp) {
    std::unique_lock lock(context->mutex);

    auto resource = add_resource(context,
                                 os::udp::get_descriptor(udp->socket_obj.get()),
                                 0,
                                 udp_resource_handler, udp);
    udp->resource = resource;

    looper_trace_info(log_module, "adding udp: ptr=0x%x, resource_handle=%lu", udp, resource);
}

void remove_udp(loop_context* context, udp_data* udp) {
    std::unique_lock lock(context->mutex);

    looper_trace_info(log_module, "removing udp: ptr=0x%x, resource_handle=%lu", udp, udp->resource);

    if (udp->resource != empty_handle) {
        remove_resource(context, udp->resource);
        udp->resource = empty_handle;
    }
}

void start_udp_read(loop_context* context, udp_data* udp) {
    std::unique_lock lock(context->mutex);

    if (udp->resource == empty_handle) {
        throw std::runtime_error("udp has bad resource handle");
    }
    if (udp->state != udp_data::state::open) {
        throw std::runtime_error("udp state invalid for read");
    }
    if (udp->reading) {
        throw std::runtime_error("udp already reading");
    }

    looper_trace_info(log_module, "udp starting read: ptr=0x%x", udp);

    request_resource_events(context, udp->resource, event_in, events_update_type::append);
    udp->reading = true;
}

void stop_udp_read(loop_context* context, udp_data* udp) {
    std::unique_lock lock(context->mutex);

    if (udp->resource == empty_handle) {
        throw std::runtime_error("udp has bad resource handle");
    }
    if (udp->state != udp_data::state::open) {
        throw std::runtime_error("udp state invalid for read");
    }
    if (!udp->reading) {
        return;
    }

    looper_trace_info(log_module, "udp stopping read: ptr=0x%x", udp);

    udp->reading = false;
    request_resource_events(context, udp->resource, event_in, events_update_type::remove);
}

void write_udp(loop_context* context, udp_data* udp, udp_data::write_request&& request) {
    std::unique_lock lock(context->mutex);

    if (udp->resource == empty_handle) {
        throw std::runtime_error("udp has bad resource handle");
    }
    if (udp->state != udp_data::state::open) {
        throw std::runtime_error("udp state invalid for write");
    }

    looper_trace_info(log_module, "udp writing: ptr=0x%x, buffer_size=%lu", udp, request.size);

    udp->write_requests.push_back(std::move(request));

    if (!udp->write_pending) {
        request_resource_events(context, udp->resource, event_out, events_update_type::append);
        udp->write_pending = true;
    }
}

}
