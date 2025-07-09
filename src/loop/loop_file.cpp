
#include "loop_file.h"

namespace looper::impl {

#define log_module loop_log_module "_file"

file::file(const looper::file handle, loop_context* context, const std::string_view path, const open_mode mode, const file_attributes attributes)
    : stream(handle, context)
    , m_file_obj(os::make_file(path, mode, attributes)) {
    std::unique_lock lock(m_context->mutex);
    attach_to_loop(os::file::get_descriptor(m_file_obj.get()), 0);

    set_read_enabled((mode & open_mode::read) != 0);
    set_write_enabled((mode & open_mode::write) != 0);
}

void file::seek(const size_t offset, const seek_whence whence) {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    OS_CHECK_THROW(os::file::seek(m_file_obj.get(), offset, whence));
}

size_t file::tell() const {
    std::unique_lock lock(m_context->mutex);

    verify_not_errored();

    size_t offset;
    OS_CHECK_THROW(os::file::tell(m_file_obj.get(), offset));
    return offset;
}

void file::close() {
    std::unique_lock lock(m_context->mutex);

    if (m_file_obj) {
        m_file_obj.reset();
    }

    set_read_enabled(false);
    set_write_enabled(false);

    detach_from_loop();
}

looper::error file::read_from_obj(uint8_t* buffer, const size_t buffer_size, size_t& read_out) {
    return os::file::read(m_file_obj.get(), buffer, buffer_size, read_out);
}

looper::error file::write_to_obj(const uint8_t* buffer, const size_t size, size_t& written_out) {
    return os::file::write(m_file_obj.get(), buffer, size, written_out);
}

}
