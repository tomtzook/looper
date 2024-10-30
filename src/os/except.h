#pragma once

#include <exception>

#include <looper_except.h>

namespace looper::os {

class closed_fd_exception : public os_exception {
public:
    closed_fd_exception()
        : os_exception(error_fd_closed)
    {}

    [[nodiscard]] const char * what() const noexcept override {
        return "fd has been closed";
    }
};

class eof_exception : public os_exception {
public:
    eof_exception()
        : os_exception(error_eof)
    {}

    [[nodiscard]] const char * what() const noexcept override {
        return "eof reached";
    }
};

}
