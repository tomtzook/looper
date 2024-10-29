#pragma once

#include <exception>

#include <looper_types.h>

namespace looper {

class loop_closing_exception : public std::exception {
public:
    explicit loop_closing_exception(loop handle)
        : m_handle(handle)
    {}

    [[nodiscard]] loop get_handle() const {
        return m_handle;
    }

    [[nodiscard]] const char * what() const noexcept override {
        return "loop is closing and can't be used";
    }

private:
    loop m_handle;
};

class no_space_exception : public std::exception {
    [[nodiscard]] const char * what() const noexcept override {
        return "no space more more data";
    }
};

class bad_handle_exception : public std::exception {
public:
    explicit bad_handle_exception(handle handle)
        : m_handle(handle)
    {}

    [[nodiscard]] handle get_handle() const {
        return m_handle;
    }

    [[nodiscard]] const char * what() const noexcept override {
        return "handle not compatible for specific use";
    }

private:
    handle m_handle;
};

class no_such_handle_exception : public std::exception {
public:
    explicit no_such_handle_exception(handle handle)
        : m_handle(handle)
    {}

    [[nodiscard]] handle get_handle() const {
        return m_handle;
    }

    [[nodiscard]] const char * what() const noexcept override {
        return "handle references nothing";
    }

private:
    handle m_handle;
};

class os_exception : public std::exception {
public:
    explicit os_exception(error code)
        : m_code(code)
    {}

    [[nodiscard]] error get_code() const {
        return m_code;
    }

    [[nodiscard]] const char * what() const noexcept override {
        return "error from os";
    }

private:
    error m_code;
};

}
