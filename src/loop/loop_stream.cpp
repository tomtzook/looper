
#include "loop_internal.h"
#include "loop_stream.h"

namespace looper::impl {

#define log_module loop_log_module "_stream"

static bool do_write(stream_data* stream) {
    // todo: better use of queues
    // todo: use iovec

    static constexpr size_t max_writes_to_do_in_one_iteration = 16;

    // only write up to 16 requests, so as not to starve the loop with writes
    size_t write_count = max_writes_to_do_in_one_iteration;

    while (!stream->write_requests.empty() && (write_count--) > 0) {
        auto& request = stream->write_requests.front();

        size_t written;
        auto error = stream->write_func(request.buffer.get() + request.pos, request.size, written);
        if (error == error_success) {
            request.pos += written;
            if (request.pos < request.size) {
                // didn't finish write
                return true;
            }

            looper_trace_debug(log_module, "stream write request finished: ptr=0x%x", stream);
            request.error = error_success;

            stream->completed_write_requests.push_back(std::move(request));
            stream->write_requests.pop_front();
        } else if (error == error_in_progress || error == error_again) {
            // didn't finish write, but need to try again later
            return true;
        } else {
            looper_trace_error(log_module, "stream write request failed: ptr=0x%x, code=%lu", stream, error);
            request.error = error;

            stream->completed_write_requests.push_back(std::move(request));
            stream->write_requests.pop_front();

            return false;
        }
    }

    return true;
}

static void report_write_requests_finished(std::unique_lock<std::mutex>& lock, stream_data* stream) {
    while (!stream->completed_write_requests.empty()) {
        auto& request = stream->completed_write_requests.front();
        invoke_func<std::mutex, stream_data*, stream_data::write_request&>
                (lock, "stream_loop_callback", stream->from_loop_write_callback, stream, request);

        stream->completed_write_requests.pop_front();
    }
}

void stream_handle_read(std::unique_lock<std::mutex>& lock, loop_context* context, stream_data* stream) {
    if (!stream->reading || stream->is_errored || !stream->can_read) {
        request_resource_events(context, stream->resource, event_in, events_update_type::remove);
        return;
    }

    std::span<const uint8_t> data{};
    size_t read;
    auto error = stream->read_func(context->read_buffer, sizeof(context->read_buffer), read);
    if (error == error_success) {
        data = std::span<const uint8_t>{context->read_buffer, read};
        looper_trace_debug(log_module, "stream read new data: ptr=0x%x, data_size=%lu", stream, data.size());
    } else {
        stream->is_errored = true;
        looper_trace_error(log_module, "stream read error: ptr=0x%x, code=%lu", stream, error);
    }

    invoke_func<std::mutex, stream_data*, std::span<const uint8_t>, looper::error>
            (lock, "stream_loop_callback", stream->from_loop_read_callback, stream, data, error);
}

void stream_handle_write(std::unique_lock<std::mutex>& lock, loop_context* context, stream_data* stream) {
    if (!stream->write_pending || stream->is_errored || !stream->can_write) {
        request_resource_events(context, stream->resource, event_out, events_update_type::remove);
        return;
    }

    if (!do_write(stream)) {
        stream->is_errored = true;
        stream->write_pending = false;
        request_resource_events(context, stream->resource, event_out, events_update_type::remove);
    } else {
        if (stream->write_requests.empty()) {
            stream->write_pending = false;
            request_resource_events(context, stream->resource, event_out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock, stream);
}

void init_stream(
    stream_data* stream,
    const handle handle,
    const resource resource,
    stream_data::read_function&& read_func,
    stream_data::write_function&& write_func) {
    stream->handle = handle;
    stream->resource = resource;
    stream->read_func = read_func;
    stream->write_func = write_func;
    stream->is_errored = false;
    stream->can_read = false;
    stream->can_write = false;
}

void start_stream_read(loop_context* context, stream_data* stream, read_callback&& callback) {
    std::unique_lock lock(context->mutex);

    if (stream->resource == empty_handle) {
        throw std::runtime_error("stream has bad resource handle");
    }
    if (stream->read_func == nullptr) {
        throw std::runtime_error("stream does not support read");
    }
    if (!stream->can_read) {
        throw std::runtime_error("stream cannot read at the current state");
    }
    if (stream->reading) {
        throw std::runtime_error("stream already reading");
    }
    if (stream->is_errored) {
        throw std::runtime_error("stream cannot read because it is errored");
    }

    looper_trace_info(log_module, "stream starting read: ptr=0x%x", stream);

    stream->user_read_callback = callback;
    request_resource_events(context, stream->resource, event_in, events_update_type::append);
    stream->reading = true;
}

void stop_stream_read(loop_context* context, stream_data* stream) {
    std::unique_lock lock(context->mutex);

    if (stream->resource == empty_handle) {
        throw std::runtime_error("stream has bad resource handle");
    }
    if (!stream->reading) {
        return;
    }

    looper_trace_info(log_module, "stream stopping read: ptr=0x%x", stream);

    stream->reading = false;
    request_resource_events(context, stream->resource, event_in, events_update_type::remove);
}

void write_stream(loop_context* context, stream_data* stream, stream_data::write_request&& request) {
    std::unique_lock lock(context->mutex);

    if (stream->resource == empty_handle) {
        throw std::runtime_error("stream has bad resource handle");
    }
    if (stream->write_func == nullptr) {
        throw std::runtime_error("stream does not support write");
    }
    if (!stream->can_write) {
        throw std::runtime_error("stream cannot write at the current state");
    }
    if (stream->is_errored) {
        throw std::runtime_error("stream cannot write because it is errored");
    }

    looper_trace_info(log_module, "stream writing: ptr=0x%x, buffer_size=%lu", stream, request.size);

    stream->write_requests.push_back(std::move(request));

    if (!stream->write_pending) {
        request_resource_events(context, stream->resource, event_out, events_update_type::append);
        stream->write_pending = true;
    }
}

}
