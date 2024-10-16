#pragma once

#include <span>

#include <looper_types.h>

namespace looper::impl {

// todo: modify so that handles are only in looper.cpp layers and here we just work with structs

struct loop_context;

loop_context* create_loop(loop handle);
void destroy_loop(loop_context* context);

// event
event create_event(loop_context* context, event_callback&& callback);
void destroy_event(loop_context* context, event event);
void set_event(loop_context* context, event event);
void clear_event(loop_context* context, event event);

// timer
timer create_timer(loop_context* context, std::chrono::milliseconds timeout, timer_callback&& callback);
void destroy_timer(loop_context* context, timer timer);
void start_timer(loop_context* context, timer timer);
void stop_timer(loop_context* context, timer timer);
void reset_timer(loop_context* context, timer timer);

// execute
future create_future(loop_context* context, future_callback&& callback);
void destroy_future(loop_context* context, future future);
void execute_later(loop_context* context, future future, std::chrono::milliseconds delay);
bool wait_for(loop_context* context, future future, std::chrono::milliseconds timeout);

// tcp
tcp create_tcp(loop_context* context);
void destroy_tcp(loop_context* context, tcp tcp);
void bind_tcp(loop_context* context, tcp tcp, uint16_t port);
void connect_tcp(loop_context* context, tcp tcp, std::string_view server_address, uint16_t server_port, tcp_callback&& callback);
void start_tcp_read(loop_context* context, tcp tcp, tcp_read_callback&& callback);
void stop_tcp_read(loop_context* context, tcp tcp);
void write_tcp(loop_context* context, tcp tcp, std::span<const uint8_t> buffer, tcp_callback&& callback);

// run
bool run_once(loop_context* context);
void run_forever(loop_context* context);

}
