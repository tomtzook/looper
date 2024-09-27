
#include "instance.h"

namespace looper {

instance::instance()
    : m_loops(0, handles::type_loop)
{}

instance::~instance() {
    // todo: stop and destroy all!
}

loop instance::create() {
    auto [handle, data] = m_loops.allocate_new();


    return handle;
}

void instance::destroy(loop loop) {
    auto data = m_loops.release(loop);
}

void instance::run_once(loop loop) {
    auto data = m_loops[loop];
    data->loop.run_once();
}

void instance::run_forever(loop loop) {
    auto data = m_loops[loop];
    // todo: need to stop at some point, when
    // todo: what if someone calls destroy
}

void instance::execute_on(loop loop, execute_callback&& callback) {
    auto data = m_loops[loop];
    data->loop.execute_later(std::move(callback), false);
}

void instance::execute_on_and_wait(loop loop, execute_callback&& callback) {
    auto data = m_loops[loop];
    data->loop.execute_later(std::move(callback), true);
}

}
