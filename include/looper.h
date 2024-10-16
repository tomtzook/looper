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

// futures
future create_future(loop loop, future_callback&& callback);
void destroy_future(future future);
void execute_once(future future, std::chrono::milliseconds delay);
bool wait_for(future future, std::chrono::milliseconds timeout);

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
