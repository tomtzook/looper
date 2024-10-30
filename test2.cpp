
#include <unistd.h>

#include <cstdio>
#include <thread>
#include <cstring>

#include <looper.h>
#include <looper_tcp.h>

#include "src/os/linux/socket.h"


using namespace std::chrono_literals;

int main() {
    looper::initialize();
    auto loop = looper::create();
    auto timer = looper::create_timer(loop, 1s, [](looper::loop loop, looper::timer timer)->void {
        printf("timer\n");
        looper::reset_timer(timer);
    });
    looper::start_timer(timer);

    for (int i = 0; i < 30; ++i) {
        looper::run_once(loop);
        usleep(50000);

        if (i == 20) {
            printf("stop timer\n");
            looper::stop_timer(timer);
        }
    }

    looper::destroy(loop);

    return 0;
}


/*
 *
 * auto event = looper::create_event(loop, [](looper::loop loop, looper::event event)->void {
        printf("event\n");
        looper::clear_event(loop, event);
    });
    auto timer = looper::create_timer(loop, std::chrono::milliseconds(500), [](looper::loop loop, looper::timer timer)->void {
        printf("timer\n");
        looper::reset_timer(loop, timer);
    });

    looper::start_timer(loop, timer);

    looper::execute_on(loop, 500ms, [](looper::loop loop)->void {
        printf("hey\n");
    });
    looper::run_once(loop);

    looper::set_event(loop, event);
    looper::run_once(loop);
 */