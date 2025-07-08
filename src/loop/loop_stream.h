#pragma once

#include "loop_structs.h"

namespace looper::impl {

struct stream_data {
    struct write_request {
        std::unique_ptr<uint8_t[]> buffer;
        size_t pos;
        size_t size;
        looper::write_callback write_callback;

        looper::error error;
    };

    using loop_read_callback = std::function<void(stream_data*, std::span<const uint8_t>, looper::error)>;
    using loop_write_callback = std::function<void(stream_data*, write_request&)>;
    using read_function = std::function<looper::error(uint8_t* buffer, size_t buffer_size, size_t& read_out)>;
    using write_function = std::function<looper::error(const uint8_t* buffer, size_t size, size_t& written_out)>;

    explicit stream_data(const handle handle)
        : handle(handle)
        , is_errored(false)
        , can_read(false)
        , can_write(false)
        , read_func(nullptr)
        , write_func(nullptr)
        , user_read_callback(nullptr)
        , write_requests()
        , completed_write_requests()
        , resource(empty_handle)
        , reading(false)
        , write_pending(false)
        , from_loop_read_callback(nullptr)
        , from_loop_write_callback(nullptr)
    {}

    looper::handle handle;

    bool is_errored;
    bool can_read;
    bool can_write;

    read_function read_func;
    write_function write_func;

    read_callback user_read_callback;
    std::deque<write_request> write_requests;
    std::deque<write_request> completed_write_requests;

    // managed in loop
    looper::impl::resource resource;
    bool reading;
    bool write_pending;

    loop_read_callback from_loop_read_callback;
    loop_write_callback from_loop_write_callback;
};

void stream_handle_read(std::unique_lock<std::mutex>& lock, loop_context* context, stream_data* stream);
void stream_handle_write(std::unique_lock<std::mutex>& lock, loop_context* context, stream_data* stream);

void init_stream(
    stream_data* stream,
    handle handle,
    resource resource,
    stream_data::read_function&& read_func,
    stream_data::write_function&& write_func);

void start_stream_read(loop_context* context, stream_data* stream, read_callback&& callback);
void stop_stream_read(loop_context* context, stream_data* stream);
void write_stream(loop_context* context, stream_data* stream, stream_data::write_request&& request);

}
