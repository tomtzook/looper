
#include "factory.h"

#include "os/linux/linux.h"
#include "os/linux/epoll_poller.h"

namespace looper::os {

std::unique_ptr<poller> create_poller() {
    return std::make_unique<epoll_poller>();
}

std::shared_ptr<event> create_event() {
    return std::make_shared<linux_event>();
}

std::shared_ptr<tcp_socket> create_tcp_socket() {
    return std::make_shared<linux_tcp_socket>();
}

std::shared_ptr<tcp_server_socket> create_tcp_server_socket() {
    return std::make_shared<linux_tcp_server_socket>();
}

}
