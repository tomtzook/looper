
#include <unistd.h>
#include <sys/eventfd.h>
#include <cerrno>

#include <looper_except.h>
#include "linux.h"


namespace looper::os {

static os::descriptor create_eventfd() {
    auto fd = eventfd(0, EFD_NONBLOCK);
    if (fd < 0) {
        throw os_exception(errno);
    }

    return fd;
}

linux_event::linux_event()
    : event(create_eventfd())
{}

linux_event::~linux_event() {
    ::close(get_descriptor());
}

void linux_event::set() {
    ::eventfd_write(get_descriptor(), 1);
}

void linux_event::clear() {
    eventfd_t value;
    ::eventfd_read(get_descriptor(), &value);
}

}
