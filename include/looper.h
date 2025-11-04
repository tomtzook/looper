#pragma once

#include <chrono>

#include <looper_types.h>
#include <looper_except.h>
#include <looper_trace.h>

namespace looper {

loop create();
void destroy(loop loop);
loop get_parent_loop(handle handle);

void run_once(loop loop);
void run_forever(loop loop);

void exec_in_thread(loop loop);

// futures
future create_future(loop loop, future_callback&& callback);
void destroy_future(future future);
void execute_once(future future, std::chrono::milliseconds delay = no_timeout);
bool wait_for(future future, std::chrono::milliseconds timeout = no_timeout);

void execute_later(loop loop, loop_callback&& callback);
bool execute_later_and_wait(loop loop, loop_callback&& callback, std::chrono::milliseconds timeout = no_timeout);

// events
event create_event(loop loop, event_callback&& callback);
void destroy_event(event event);
void set_event(event event);
void clear_event(event event);

// timers
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback);
void destroy_timer(timer timer);
void start_timer(timer timer);
void stop_timer(timer timer);
void reset_timer(timer timer);

// tcp
tcp create_tcp(loop loop);
void destroy_tcp(tcp tcp);

void bind_tcp(tcp tcp, uint16_t port);
void bind_tcp(tcp tcp, std::string_view address, uint16_t port);
void connect_tcp(tcp tcp, std::string_view address, uint16_t port, tcp_callback&& callback);

void start_tcp_read(tcp tcp, read_callback&& callback);
void stop_tcp_read(tcp tcp);
void write_tcp(tcp tcp, std::span<const uint8_t> buffer, write_callback&& callback);

tcp_server create_tcp_server(loop loop);
void destroy_tcp_server(tcp_server tcp);

void bind_tcp_server(tcp_server tcp, std::string_view address, uint16_t port);
void bind_tcp_server(tcp_server tcp, uint16_t port);
void listen_tcp(tcp_server tcp, size_t backlog, tcp_server_callback&& callback);
tcp accept_tcp(tcp_server tcp);

// udp
udp create_udp(loop loop);
void destroy_udp(udp udp);

void bind_udp(udp udp, uint16_t port);

void start_udp_read(udp udp, udp_read_callback&& callback);
void stop_udp_read(udp udp);
void write_udp(udp udp, inet_address_view destination, std::span<const uint8_t> buffer, udp_callback&& callback);

}
