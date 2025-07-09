
#include "looper_base.h"

namespace looper {

#define log_module looper_log_module

file create_file(const loop loop, const std::string_view path, const open_mode mode, const file_attributes attributes) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop(loop);

    auto [handle, file_impl] = data.m_files.allocate_new(data.m_context, path, mode, attributes);
    looper_trace_info(log_module, "created new file: loop=%lu, handle=%lu", data.m_handle, handle);
    data.m_files.assign(handle, std::move(file_impl));

    return handle;
}

void destroy_file(const file file) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "destroying file: loop=%lu, handle=%lu", data.m_handle, file);

    const auto file_impl = data.m_files.release(file);
    file_impl->close();
}

void seek_file(const file file, const size_t offset, const seek_whence whence) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "seeking file: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_impl = data.m_files[file];
    file_impl.seek(offset, whence);
}

size_t tell_file(const file file) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "telling file: loop=%lu, handle=%lu", data.m_handle, file);

    const auto& file_impl = data.m_files[file];
    return file_impl.tell();
}

void start_file_read(const file file, read_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "starting file read: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_impl = data.m_files[file];
    file_impl.start_read(std::move(callback));
}

void stop_file_read(const file file) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "stopping file read: loop=%lu, handle=%lu", data.m_handle, file);

    auto& file_impl = data.m_files[file];
    file_impl.stop_read();
}

void write_file(const file file, const std::span<const uint8_t> buffer, write_callback&& callback) {
    std::unique_lock lock(get_global_loop_data().m_mutex);

    auto& data = get_loop_from_handle(file);

    looper_trace_info(log_module, "writing to file: loop=%lu, handle=%lu, data_size=%lu", data.m_handle, file, buffer.size_bytes());

    auto& file_impl = data.m_files[file];

    const auto buffer_size = buffer.size_bytes();
    impl::file::write_request request;
    request.buffer = std::unique_ptr<uint8_t[]>(new uint8_t[buffer_size]);
    request.size = buffer_size;
    request.write_callback = std::move(callback);

    memcpy(request.buffer.get(), buffer.data(), buffer_size);

    file_impl.write(std::move(request));
}

}
