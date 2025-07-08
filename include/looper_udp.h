#pragma once

#include <looper_types.h>
#include <looper_except.h>

namespace looper {

udp create_udp(loop loop);
void destroy_udp(udp udp);

void bind_udp(udp udp, uint16_t port);

void start_udp_read(udp udp, udp_read_callback&& callback);
void stop_udp_read(udp udp);
void write_udp(udp udp, inet_address destination, std::span<const uint8_t> buffer, udp_callback&& callback);

}
