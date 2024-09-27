
#include <looper.h>

#include "instance.h"

namespace looper {

instance g_instance;

loop create() {
    return g_instance.create();
}

void destroy(loop loop) {
    g_instance.destroy(loop);
}

void run_once(loop loop) {
    g_instance.run_once(loop);
}

void run_forever(loop loop) {
    g_instance.run_forever(loop);
}

void execute_on(loop loop, execute_callback&& callback) {
    g_instance.execute_on(loop, std::move(callback));
}

void execute_on_and_wait(loop loop, execute_callback&& callback) {
    g_instance.execute_on_and_wait(loop, std::move(callback));
}

}
