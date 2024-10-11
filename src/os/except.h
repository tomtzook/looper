#pragma once

#include <exception>

namespace looper::os {

class closed_fd_exception : public std::exception {
public:

    [[nodiscard]] const char * what() const noexcept override {
        return "fd has been closed";
    }
};

class eof_exception : public std::exception {
public:

    [[nodiscard]] const char * what() const noexcept override {
        return "eof reached";
    }
};

}
