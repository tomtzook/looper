#pragma once

#include <chrono>

#include <looper_types.h>
#include <looper_except.h>
#include <looper_trace.h>

namespace looper {

loop create();
void destroy(loop loop);

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

}
