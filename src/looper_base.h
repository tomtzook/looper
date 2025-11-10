#pragma once

#include <optional>
#include <thread>
#include <cstring>

#include "util/handles.h"

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
    explicit loop_data(loop handle);
    ~loop_data();

    loop_data(const loop_data&) = delete;
    loop_data(loop_data&&) = delete;
    loop_data& operator=(const loop_data&) = delete;
    loop_data& operator=(loop_data&&) = delete;

    void clear_context();

    loop handle;
    impl::loop_ptr loop;
    bool closing;

    std::unique_ptr<std::thread> thread;
    handles::handle_table<impl::event, handle_counts_per_type> events;
    handles::handle_table<impl::timer, handle_counts_per_type> timers;
    handles::handle_table<impl::future, handle_counts_per_type> futures;
    handles::handle_table<impl::tcp, handle_counts_per_type> tcps;
    handles::handle_table<impl::tcp_server, handle_counts_per_type> tcp_servers;
    handles::handle_table<impl::udp, handle_counts_per_type> udps;
};

struct looper_data {
    looper_data();

    looper_data(const looper_data&) = delete;
    looper_data(looper_data&&) = delete;
    looper_data& operator=(const looper_data&) = delete;
    looper_data& operator=(looper_data&&) = delete;

    // todo: we use this mutex everywhere, could be problematic, limit use. perhaps remove lock from loop layer, how?
    //  could use some lock-less mechanisms
    std::mutex mutex;
    handles::handle_table<loop_data, loops_count> loops;
};

looper_data& get_global_loop_data();

std::optional<loop_data*> try_get_loop(loop loop);
loop_data& get_loop(loop loop);
loop get_loop_handle(handle handle);
loop_data& get_loop_from_handle(handle handle);

}
