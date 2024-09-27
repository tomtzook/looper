
#include "factory.h"

#include "os/linux/epoll_poller.h"

namespace looper {

std::unique_ptr<poller> create_poller() {
    return std::make_unique<looper::os::epoll_poller>();
}

}
