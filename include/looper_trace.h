#pragma once


#define looper_trace(level, module, format, ...) \
    do {                                  \
        if (looper::trace::can_log(level)) {      \
            looper::trace::trace_impl(level, "(%s:%d) " module ": " format "\n", __FILE__, __LINE__, ##__VA_ARGS__);\
        }                                  \
    } while(0)

#define looper_trace_debug(module, format, ...) looper_trace(looper::trace::log_level_debug, module, format, ##__VA_ARGS__)
#define looper_trace_info(module, format, ...) looper_trace(looper::trace::log_level_info, module, format, ##__VA_ARGS__)
#define looper_trace_error(module, format, ...) looper_trace(looper::trace::log_level_error, module, format, ##__VA_ARGS__)

namespace looper::trace {

enum log_level {
    log_level_debug,
    log_level_info,
    log_level_error
};

bool can_log(log_level level);
void trace_impl(log_level level, const char* format, ...);

}
