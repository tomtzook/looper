

#include "loop_internal.h"


namespace looper::impl {

#define log_module loop_log_module "_file"

static void file_resource_handle_read(std::unique_lock<std::mutex>& lock, loop_context* context, file_data* file) {
    if (!file->reading) {
        request_resource_events(context, file->resource, event_in, events_update_type::remove);
        return;
    }

    std::span<const uint8_t> data{};
    size_t read;
    auto error = os::file::read(file->file_obj.get(), context->read_buffer, sizeof(context->read_buffer), read);
    if (error == error_success) {
        data = std::span<const uint8_t>{context->read_buffer, read};
        looper_trace_debug(log_module, "file read new data: ptr=0x%x, data_size=%lu", file, data.size());
    } else {
        file->state = file_data::state::errored;
        looper_trace_error(log_module, "file read error: ptr=0x%x, code=%lu", file, error);
    }

    invoke_func<std::mutex, file_data*, std::span<const uint8_t>, looper::error>
            (lock, "file_loop_callback", file->from_loop_read_callback, file, data, error);
}

static bool do_file_write(file_data* file) {
    // todo: better use of queues
    // todo: use iovec

    static constexpr size_t max_writes_to_do_in_one_iteration = 16;

    // only write up to 16 requests, so as not to starve the loop with writes
    size_t write_count = max_writes_to_do_in_one_iteration;

    while (!file->write_requests.empty() && (write_count--) > 0) {
        auto& request = file->write_requests.front();

        size_t written;
        auto error = os::file::write(file->file_obj.get(), request.buffer.get() + request.pos, request.size, written);
        if (error == error_success) {
            request.pos += written;
            if (request.pos < request.size) {
                // didn't finish write
                return true;
            }

            looper_trace_debug(log_module, "file write request finished: ptr=0x%x", file);
            request.error = error_success;

            file->completed_write_requests.push_back(std::move(request));
            file->write_requests.pop_front();
        } else if (error == error_in_progress || error == error_again) {
            // didn't finish write, but need to try again later
            return true;
        } else {
            looper_trace_error(log_module, "file write request failed: ptr=0x%x, code=%lu", file, error);
            request.error = error;

            file->completed_write_requests.push_back(std::move(request));
            file->write_requests.pop_front();

            return false;
        }
    }

    return true;
}

static void report_write_requests_finished(std::unique_lock<std::mutex>& lock, file_data* file) {
    while (!file->completed_write_requests.empty()) {
        auto& request = file->completed_write_requests.front();
        invoke_func<std::mutex, file_data*, file_data::write_request&>
                (lock, "file_loop_callback", file->from_loop_write_callback, file, request);

        file->completed_write_requests.pop_front();
    }
}

static void file_resource_handle_write(std::unique_lock<std::mutex>& lock, loop_context* context, file_data* file) {
    if (!file->write_pending) {
        request_resource_events(context, file->resource, event_out, events_update_type::remove);
        return;
    }

    if (!do_file_write(file)) {
        file->state = file_data::state::errored;
        file->write_pending = false;
        request_resource_events(context, file->resource, event_out, events_update_type::remove);
    } else {
        if (file->write_requests.empty()) {
            file->write_pending = false;
            request_resource_events(context, file->resource, event_out, events_update_type::remove);
        }
    }

    report_write_requests_finished(lock, file);
}

static void file_resource_handler(loop_context* context, void* ptr, event_types events) {
    std::unique_lock lock(context->mutex);

    auto* data = static_cast<file_data*>(ptr);
    if (data->resource == empty_handle) {
        return;
    }

    switch (data->state) {
        case file_data::state::open: {
            if ((events & event_in) != 0) {
                // new data
                file_resource_handle_read(lock, context, data);
            }

            if ((events & event_out) != 0) {
                file_resource_handle_write(lock, context, data);
            }

            break;
        }
        default:
            break;
    }
}

void add_file(loop_context* context, file_data* file) {
    std::unique_lock lock(context->mutex);

    auto resource = add_resource(context,
                                 os::file::get_descriptor(file->file_obj.get()),
                                 0,
                                 file_resource_handler, file);
    file->resource = resource;

    looper_trace_info(log_module, "adding file: ptr=0x%x, resource_handle=%lu", file, resource);
}

void remove_file(loop_context* context, file_data* file) {
    std::unique_lock lock(context->mutex);

    looper_trace_info(log_module, "removing file: ptr=0x%x, resource_handle=%lu", file, file->resource);

    if (file->resource != empty_handle) {
        remove_resource(context, file->resource);
        file->resource = empty_handle;
    }
}

void start_file_read(loop_context* context, file_data* file) {
    std::unique_lock lock(context->mutex);

    if (file->resource == empty_handle) {
        throw std::runtime_error("file has bad resource handle");
    }
    if (file->state != file_data::state::open) {
        throw std::runtime_error("file state invalid for read");
    }
    if (file->reading) {
        throw std::runtime_error("file already reading");
    }

    looper_trace_info(log_module, "file starting read: ptr=0x%x", file);

    request_resource_events(context, file->resource, event_in, events_update_type::append);
    file->reading = true;
}

void stop_file_read(loop_context* context, file_data* file) {
    std::unique_lock lock(context->mutex);

    if (file->resource == empty_handle) {
        throw std::runtime_error("file has bad resource handle");
    }
    if (file->state != file_data::state::open) {
        throw std::runtime_error("file state invalid for read");
    }
    if (!file->reading) {
        return;
    }

    looper_trace_info(log_module, "file stopping read: ptr=0x%x", file);

    file->reading = false;
    request_resource_events(context, file->resource, event_in, events_update_type::remove);
}

void write_file(loop_context* context, file_data* file, file_data::write_request&& request) {
    std::unique_lock lock(context->mutex);

    if (file->resource == empty_handle) {
        throw std::runtime_error("file has bad resource handle");
    }
    if (file->state != file_data::state::open) {
        throw std::runtime_error("file state invalid for write");
    }

    looper_trace_info(log_module, "file writing: ptr=0x%x, buffer_size=%lu", file, request.size);

    file->write_requests.push_back(std::move(request));

    if (!file->write_pending) {
        request_resource_events(context, file->resource, event_out, events_update_type::append);
        file->write_pending = true;
    }
}

}
