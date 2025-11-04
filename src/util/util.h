#pragma once

#include <mutex>
#include <functional>

#include "looper_trace.h"

namespace looper {

#define cbinvoke_log_module "callback_invoke"

template<typename _mutex, typename... args_>
static void invoke_func(std::unique_lock<_mutex>& lock, const char* name, const std::function<void(args_...)>& ref, args_... args) {
    auto ref_val = ref;
    if (ref_val != nullptr) {
        lock.unlock();
        try {
            ref_val(args...);
        } catch (const std::exception& e) {
            looper_trace_error(cbinvoke_log_module, "Error while invoking func %s: what=%s", name, e.what());
        } catch (...) {
            looper_trace_error(cbinvoke_log_module, "Error while invoking func %s: unknown", name);
        }
        lock.lock();
    }
}

template<typename _mutex, typename r_, typename... args_>
static r_ invoke_func_r(std::unique_lock<_mutex>& lock, const char* name, const std::function<r_(args_...)>& ref, args_... args) {
    auto ref_val = ref;
    r_ result{};
    if (ref_val != nullptr) {
        lock.unlock();
        try {
            result = ref_val(args...);
        } catch (const std::exception& e) {
            looper_trace_error(cbinvoke_log_module, "Error while invoking func %s: what=%s", name, e.what());
        } catch (...) {
            looper_trace_error(cbinvoke_log_module, "Error while invoking func %s: unknown", name);
        }
        lock.lock();
    }
    return result;
}

template<typename... args_>
static void invoke_func_nolock(const char* name, const std::function<void(args_...)>& ref, args_... args) {
    auto ref_val = ref;
    if (ref_val != nullptr) {
        try {
            ref_val(args...);
        } catch (const std::exception& e) {
            looper_trace_error(cbinvoke_log_module, "Error while invoking func %s: what=%s", name, e.what());
        } catch (...) {
            looper_trace_error(cbinvoke_log_module, "Error while invoking func %s: unknown", name);
        }
    }
}

}
