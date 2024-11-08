#pragma once

#include "os/os.h"

namespace looper::os {

static inline looper::error os_error_to_looper(int error) {
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

static inline looper::error get_call_error() {
    int code = errno;
    return os_error_to_looper(code);
}

}