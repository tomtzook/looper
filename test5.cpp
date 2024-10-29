
#include <unistd.h>

#include <cstdio>
#include <thread>
#include <cstring>

#include <looper.h>
#include <looper_tcp.h>


using namespace std::chrono_literals;

int main() {
    auto loop = looper::create();
    looper::exec_in_thread(loop);

    auto tcp_server = looper::create_tcp_server(loop);
    looper::bind_tcp_server(tcp_server, 50000);
    looper::listen_tcp(tcp_server, 2, [](looper::loop loop, looper::tcp_server server)->void {
        printf("server got new connection\n");
        auto tcp = looper::accept_tcp(server);
        printf("accepted new connection\n");

        std::string_view message = "hey jude";
        looper::write_tcp(tcp, {reinterpret_cast<const uint8_t*>(message.data()), message.size()}, [](looper::loop loop, looper::tcp tcp, looper::error error)->void {
            if (error != 0) {
                printf("error from client writing: %u\n", error);
                looper::destroy_tcp(tcp);
                return;
            }

            printf("data written from client\n");
        });
    });

    auto tcp = looper::create_tcp(loop);
    looper::bind_tcp(tcp, 50001);

    sleep(1);
    looper::connect_tcp(tcp, "127.0.0.1", 50000, [](looper::loop loop, looper::tcp tcp, looper::error error)->void {
        if (error != 0) {
            printf("error connecting: %u\n", error);
            looper::destroy_tcp(tcp);
            return;
        }

        printf("connected!\n");
        looper::start_tcp_read(tcp, [](looper::loop loop, looper::tcp tcp, std::span<const uint8_t> buffer, looper::error error)->void {
            if (error != 0) {
                printf("error reading: %u\n", error);
                looper::destroy_tcp(tcp);
                return;
            }

            printf("new message: %s\n", reinterpret_cast<const char*>(buffer.data()));
            looper::write_tcp(tcp, buffer, [](looper::loop loop, looper::tcp tcp, looper::error error)->void {
                if (error != 0) {
                    printf("error writing: %u\n", error);
                    looper::destroy_tcp(tcp);
                    return;
                }

                printf("data written\n");
            });
        });
    });

    sleep(10);

    printf("done\n");
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