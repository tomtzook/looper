#pragma once

#include <optional>
#include <thread>

#include <looper.h>
#include <looper_tcp.h>
#include <cstring>

#include "util/handles.h"
#include "util/util.h"
#include "os/factory.h"

#include "loop/loop.h"
#include "loop/loop_timer.h"
#include "loop/loop_future.h"
#include "loop/loop_event.h"
#include "loop/loop_tcp.h"
#include "loop/loop_udp.h"

namespace looper {

#define looper_log_module "looper"

static constexpr size_t handle_counts_per_type = 64;
static constexpr size_t loops_count = 8;

struct loop_data {
    explicit loop_data(loop handle)
        : m_handle(handle)
        , m_context(impl::create_loop(handle))
        , m_closing(false)
        , m_thread(nullptr)
        , m_events(handles::handle{handle}.index(), handles::type_event)
        , m_timers(handles::handle{handle}.index(), handles::type_timer)
        , m_futures(handles::handle{handle}.index(), handles::type_future)
        , m_tcps(handles::handle{handle}.index(), handles::type_tcp)
        , m_tcp_servers(handles::handle{handle}.index(), handles::type_tcp_server)
        , m_udps(handles::handle{handle}.index(), handles::type_udp)
    {}
    ~loop_data() {
        if (m_thread && m_thread->joinable()) {
            m_thread->join();
        }
        m_thread.reset();

        clear_context();
    }

    loop_data(const loop_data&) = delete;
    loop_data(loop_data&&) = delete;
    loop_data& operator=(const loop_data&) = delete;
    loop_data& operator=(loop_data&&) = delete;

    void clear_context() {
        if (m_context != nullptr) {
            impl::destroy_loop(m_context);
            m_context = nullptr;
        }
    }

    loop m_handle;
    impl::loop_context* m_context;
    bool m_closing;

    std::unique_ptr<std::thread> m_thread;
    handles::handle_table<impl::event, handle_counts_per_type> m_events;
    handles::handle_table<impl::timer, handle_counts_per_type> m_timers;
    handles::handle_table<impl::future, handle_counts_per_type> m_futures;
    handles::handle_table<impl::tcp, handle_counts_per_type> m_tcps;
    handles::handle_table<impl::tcp_server, handle_counts_per_type> m_tcp_servers;
    handles::handle_table<impl::udp, handle_counts_per_type> m_udps;
};

struct looper_data {
    looper_data()
        : m_mutex()
        , m_loops(0, handles::type_loop)
    {}

    looper_data(const looper_data&) = delete;
    looper_data(looper_data&&) = delete;
    looper_data& operator=(const looper_data&) = delete;
    looper_data& operator=(looper_data&&) = delete;

    // todo: we use this mutex everywhere, could be problematic, limit use. perhaps remove lock from loop layer, how?
    //  could use some lock-less mechanisms, or spinlocks
    std::mutex m_mutex;
    handles::handle_table<loop_data, loops_count> m_loops;
};

looper_data& get_global_loop_data();

static inline std::optional<loop_data*> try_get_loop(loop loop) {
    if (!get_global_loop_data().m_loops.has(loop)) {
        return std::nullopt;
    }

    auto& data = get_global_loop_data().m_loops[loop];
    if (data.m_closing) {
        return std::nullopt;
    }

    return {&data};
}

static inline loop_data& get_loop(loop loop) {
    auto& data = get_global_loop_data().m_loops[loop];
    if (data.m_closing) {
        throw loop_closing_exception(loop);
    }

    return data;
}

static inline loop get_loop_handle(handle handle) {
    handles::handle full(handle);
    handles::handle loop(0, handles::type_loop, full.parent());
    return loop.raw();
}

static inline loop_data& get_loop_from_handle(handle handle) {
    auto loop_handle = get_loop_handle(handle);
    return get_loop(loop_handle);
}

}
