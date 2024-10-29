#pragma once

#include <memory>

#include <looper_types.h>

namespace looper::os {

// todo: better os layer?
//  maybe with error codes instead
//  and structs instead of classes and oop
//  and platform-independent error codes

using descriptor = int;

class resource {
public:
    virtual ~resource() = default;

    [[nodiscard]] virtual descriptor get_descriptor() const = 0;
};

class event : public resource {
public:
    virtual ~event() override = default;

    virtual void set() = 0;
    virtual void clear() = 0;
};

class tcp_socket : public resource {
public:
    virtual ~tcp_socket() override = default;

    [[nodiscard]] virtual error get_internal_error() = 0;

    virtual void close() = 0;

    virtual void bind(uint16_t port) = 0;

    virtual bool connect(std::string_view ip, uint16_t port) = 0;
    virtual void finalize_connect() = 0;

    virtual size_t read(uint8_t* buffer, size_t buffer_size) = 0;
    virtual size_t write(const uint8_t* buffer, size_t size) = 0;
};

}
