#pragma once

#include "os/os.h"
#include "loop_io.h"

namespace looper::impl {

struct udp_write_request {
    size_t pos;
    size_t size;
    looper::error error;
    looper::write_callback write_callback;

    std::unique_ptr<uint8_t[]> buffer;
    inet_address destination;
};

struct udp_read_data {
    std::span<uint8_t> buffer;
    size_t read_count;
    inet_address sender;
    looper::error error;
};

struct udp_io {
    udp_io();

    [[nodiscard]] os::descriptor get_descriptor() const;
    looper::error read(udp_read_data& data) const;
    looper::error write(const udp_write_request& request, size_t& written) const;

    void close();

    os::udp m_obj;
};

class udp final {
public:
    udp(looper::udp handle, const loop_ptr& loop);

    void bind(uint16_t port);
    void bind(std::string_view address, uint16_t port);

    void start_read(udp_read_callback&& callback);
    void stop_read();
    void write(udp_write_request&& request);

    void close();

private:
    io<udp_write_request, udp_read_data, udp_io> m_io;
};

}
