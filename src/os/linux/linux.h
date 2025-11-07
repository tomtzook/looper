#pragma once

#include "os/os.h"

namespace looper::os {

static looper::error os_error_to_looper(const int error) {
    switch (error) {
        case 0:
            return error_success;
        case EAGAIN:
            return error_again;
        case EINPROGRESS:
            return error_in_progress;
        case EINTR:
            return error_interrupted;
        default:
            return -error;
    }
}

static looper::error get_call_error() {
    const int code = errno;
    return os_error_to_looper(code);
}

looper::error io_read(os::descriptor descriptor, uint8_t* buffer, size_t buffer_size, size_t& read_out);
looper::error io_write(os::descriptor descriptor, const uint8_t* buffer, size_t size, size_t& written_out);

}