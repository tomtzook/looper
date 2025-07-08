
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

static void _file_read_callback(impl::file_data* file, std::span<const uint8_t> buffer, looper::error error) {
    auto loop = get_loop_handle(file->handle);

    looper_trace_debug(log_module, "file read new data: loop=%lu, handle=%lu, error=%lu", loop, file->handle, error);
    invoke_func_nolock("file_read_callback", file->user_read_callback, loop, file->handle, buffer, error);
}

static void _file_write_callback(impl::file_data* file, impl::file_data::write_request& request) {
    auto loop = get_loop_handle(file->handle);

    looper_trace_debug(log_module, "file writing finished: loop=%lu, handle=%lu, error=%lu", loop, file->handle, request.error);
    invoke_func_nolock("file_write_callback", request.write_callback, loop, file->handle, request.error);
}

file create_file(loop loop, std::string_view path, open_mode mode, file_attributes attributes) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, file_data] = data.m_files.allocate_new();
    file_data->file_obj = os::make_file(path, mode, attributes);
    file_data->from_loop_read_callback = _file_read_callback;
    file_data->from_loop_write_callback = _file_write_callback;
    file_data->state = impl::file_data::state::open;

    looper_trace_info(log_module, "creating new file: loop=%lu, handle=%lu", data.m_handle, handle);

    impl::add_file(data.m_context, file_data.get());

    data.m_files.assign(handle, std::move(file_data));

    return handle;
}

void destroy_file(file file) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "destroying file: loop=%lu, handle=%lu", data.m_handle, file);

    auto file_data = data.m_files.release(file);
    impl::remove_file(data.m_context, file_data.get());

    if (file_data->file_obj) {
        file_data->file_obj.reset();
    }
}

void seek_file(file file, size_t offset, seek_whence whence) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "seeking file: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_data = data.m_files[file];
    OS_CHECK_THROW(os::file::seek(file_data.file_obj.get(), offset, whence));
}

size_t tell_file(file file) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "telling file: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_data = data.m_files[file];

    size_t offset;
    OS_CHECK_THROW(os::file::tell(file_data.file_obj.get(), offset));
    return offset;
}

void start_file_read(file file, file_read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "starting file read: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_data = data.m_files[file];
    file_data.user_read_callback = std::move(callback);
    impl::start_file_read(data.m_context, &file_data);
}

void stop_file_read(file file) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "stopping file read: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_data = data.m_files[file];
    impl::stop_file_read(data.m_context, &file_data);
}

void write_file(file file, std::span<const uint8_t> buffer, file_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "writing to file: loop=%lu, handle=%lu, data_size=%lu", data.m_handle, file, buffer.size_bytes());

    auto& file_data = data.m_files[file];

    const auto buffer_size = buffer.size_bytes();
    impl::file_data::write_request request;
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.size = buffer_size;
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    impl::write_file(data.m_context, &file_data, std::move(request));
}

}
