
#include <unistd.h>

#include <cstdio>
#include <thread>
#include <cstring>

#include <looper.h>
#include <looper_tcp.h>

#include "src/os/linux/socket.h"


using namespace std::chrono_literals;

int main() {
    auto loop = looper::create();
    auto event = looper::create_event(loop, [](looper::loop loop, looper::event event)->void {
        printf("event\n");
    });

    for (int i = 0; i < 15; ++i) {
        looper::run_once(loop);
        usleep(50000);

        if (i == 5) {
            printf("set event\n");
            looper::set_event(event);
        }

        if (i == 10) {
            printf("clear event\n");
            looper::clear_event(event);
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