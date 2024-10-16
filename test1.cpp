
#include <unistd.h>

#include <cstdio>
#include <thread>
#include <cstring>

#include <looper.h>
#include <looper_tcp.h>

#include "src/os/linux/socket.h"


using namespace std::chrono_literals;

void thread_server() {
    const auto fd = looper::os::create_tcp_socket2();
    looper::os::linux_base_socket server_socket(fd);

    server_socket.configure_blocking(true);
    server_socket.setoption<looper::os::sockopt_keepalive>(true);
    server_socket.setoption<looper::os::sockopt_reuseport>(true);
    server_socket.bind(50000);

    server_socket.listen(2);
    printf("server: waiting for client\n");

    const auto fd_client = server_socket.accept();
    looper::os::linux_base_socket client_socket(fd_client);
    printf("server: client connected\n");

    const char* message = "hello world";
    client_socket.write(reinterpret_cast<const uint8_t*>(message), ::strlen(message));
    printf("server: message written\n");

    uint8_t buffer[256];
    auto read = client_socket.read(buffer, sizeof(buffer));
    printf("server: new data %s\n", buffer);

    sleep(5);
}

int main() {
    std::thread server_thread(&thread_server);

    auto loop = looper::create();

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

    for (int i = 0; i < 100; ++i) {
        looper::run_once(loop);
        usleep(50000);
    }

    looper::destroy(loop);

    if (server_thread.joinable()) {
        server_thread.join();
    }

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