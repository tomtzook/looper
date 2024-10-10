
#include "factory.h"

#include "os/linux/linux.h"
#include "os/linux/epoll_poller.h"

namespace looper {

std::unique_ptr<poller> create_poller() {
    return std::make_unique<os::epoll_poller>();
}

std::shared_ptr<os::event> create_event() {
    return std::make_shared<os::linux_event>();
}

}
