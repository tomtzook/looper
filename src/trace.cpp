
#include <cstdarg>

#include <looper_trace.h>

#define TRACE_LEVEL_ERROR 0
#define TRACE_LEVEL_INFO 1
#define TRACE_LEVEL_DEBUG 2

#define TRACE_SINK_STDOUT 0
#define TRACE_SINK_LINUX_SYSLOG 1

#ifdef TRACE_SINK
#if TRACE_SINK == TRACE_SINK_STDOUT
#include <cstdio>
#elif TRACE_SINK == TRACE_SINK_LINUX_SYSLOG
#include <sys/syslog.h>
#else
#error "unknown trace sink"
#endif
#endif

namespace looper::trace {

#ifndef TRACE_SINK
#error "trace sink not defined"
#endif

#ifndef TRACE_LEVEL
#error "trace level not defined"
#endif

#if TRACE_SINK == TRACE_SINK_LINUX_SYSLOG
static inline int level_to_pri(log_level level) {
    switch (level) {
        case log_level_debug:
            return LOG_DEBUG;
        case log_level_info:
            return LOG_INFO;
        case log_level_error:
            return LOG_ERR;
        default:
            return LOG_INFO;
    }
}
#endif

#if TRACE_LEVEL == TRACE_LEVEL_ERROR
log_level s_base_level = log_level::log_level_error;
#elif TRACE_LEVEL == TRACE_LEVEL_INFO
log_level s_base_level = log_level::log_level_info;
#elif TRACE_LEVEL == TRACE_LEVEL_DEBUG
log_level s_base_level = log_level::log_level_debug;
#else
#error "unknown trace level"
#endif

__attribute__((constructor)) void construct_trace()  {
#if TRACE_SINK == TRACE_SINK_LINUX_SYSLOG
    openlog("looper", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
#endif
}

__attribute__((destructor())) void destruct_trace()  {
#if TRACE_SINK == TRACE_SINK_LINUX_SYSLOG
    closelog();
#endif
}

bool can_log(const log_level level) {
    return level >= s_base_level;
}

void trace_impl(log_level level, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
#if TRACE_SINK == TRACE_SINK_STDOUT
    vprintf(format, ap);
#elif TRACE_SINK == TRACE_SINK_LINUX_SYSLOG
    vsyslog(level_to_pri(level), format, ap);
#endif
    va_end(ap);
}

}
