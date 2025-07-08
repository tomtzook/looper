#include "linux.h"

namespace looper::os {

looper::error io_read(const os::descriptor descriptor, uint8_t* buffer, const size_t buffer_size, size_t& read_out) {
    if (buffer_size == 0) {
        read_out = 0;
        return error_success;
    }

    const auto result = ::read(descriptor, buffer, buffer_size);
    if (result == 0) {
        return error_eof;
    }
    if (result < 0) {
        const auto error_code = get_call_error();
        if (error_code == error_again) {
            // while in non-blocking mode, socket operations may return eagain if
            // the operation will end up blocking, as such just return.
            read_out = 0;
            return error_success;
        }

        return get_call_error();
    }

    read_out = result;
    return error_success;
}

looper::error io_write(const os::descriptor descriptor, const uint8_t* buffer, const size_t size, size_t& written_out) {
    const auto result = ::write(descriptor, buffer, size);
    if (result < 0) {
        return get_call_error();
    }

    written_out = result;
    return error_success;
}

}
