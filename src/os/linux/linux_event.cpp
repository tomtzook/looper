
#include <sys/eventfd.h>
#include <new>

#include "os/linux/linux.h"
#include "os/os_interface.h"


namespace looper::os::interface::event {

static looper::error create_eventfd(os::descriptor& descriptor_out) {
    const auto fd = eventfd(0, EFD_NONBLOCK);
    if (fd < 0) {
        return get_call_error();
    }

    descriptor_out = fd;
    return error_success;
}

struct event {
    os::descriptor fd;
};

looper::error create(event** event_out) {
    os::descriptor descriptor;
    const auto status = create_eventfd(descriptor);
    if (status != error_success) {
        return status;
    }

    auto* _event = new (std::nothrow) event;
    if (_event == nullptr) {
        ::close(descriptor);
        return error_allocation;
    }

    _event->fd = descriptor;

    *event_out = _event;
    return error_success;
}

void close(const event* event) {
    ::close(event->fd);

    delete event;
}

descriptor get_descriptor(const event* event) {
    return event->fd;
}

looper::error set(const event* event) {
    if (::eventfd_write(event->fd, 1)) {
        return get_call_error();
    }

    return error_success;
}

looper::error clear(const event* event) {
    eventfd_t value;
    if (::eventfd_read(event->fd, &value)) {
        return get_call_error();
    }

    return error_success;
}

}
