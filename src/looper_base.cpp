
#include "looper_base.h"


namespace looper {

loop_data::loop_data(const looper::loop handle)
    : handle(handle)
    , loop(std::make_shared<impl::loop>(handle))
    , closing(false)
    , thread(nullptr)
    , events(handles::handle{handle}.index(), handles::type_event)
    , timers(handles::handle{handle}.index(), handles::type_timer)
    , futures(handles::handle{handle}.index(), handles::type_future)
    , tcps(handles::handle{handle}.index(), handles::type_tcp)
    , tcp_servers(handles::handle{handle}.index(), handles::type_tcp_server)
    , udps(handles::handle{handle}.index(), handles::type_udp)
#ifdef LOOPER_UNIX_SOCKETS
    , unix_sockets(handles::handle{handle}.index(), handles::type_unix_socket)
    , unix_socket_servers(handles::handle{handle}.index(), handles::type_unix_socket_server)
#endif
{}

loop_data::~loop_data() {
    if (thread && thread->joinable()) {
        thread->join();
    }
    thread.reset();

    clear_context();
}

void loop_data::clear_context() {
    // must clear all handles first otherwise they cannot access the loop.
    events.clear();
    timers.clear();
    futures.clear();
    tcps.clear();
    tcp_servers.clear();
    udps.clear();
    loop.reset();
}

looper_data::looper_data()
    : mutex()
    , loops(0, handles::type_loop)
{}

looper_data& get_global_loop_data() {
    static looper_data g_instance{};
    return g_instance;
}

std::optional<loop_data*> try_get_loop(const loop loop) {
    if (!get_global_loop_data().loops.has(loop)) {
        return std::nullopt;
    }

    auto& data = get_global_loop_data().loops[loop];
    if (data.closing) {
        return std::nullopt;
    }

    return {&data};
}

loop_data& get_loop(const loop loop) {
    auto& data = get_global_loop_data().loops[loop];
    if (data.closing) {
        throw loop_closing_exception(loop);
    }

    return data;
}

loop get_loop_handle(const handle handle) {
    const handles::handle full(handle);
    const handles::handle loop(0, handles::type_loop, full.parent());
    return loop.raw();
}

loop_data& get_loop_from_handle(const handle handle) {
    const auto loop_handle = get_loop_handle(handle);
    return get_loop(loop_handle);
}

}
