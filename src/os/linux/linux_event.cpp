
#include <sys/eventfd.h>

#include "os/linux/linux.h"
#include "os/os.h"


namespace looper::os::event {

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
    auto status = create_eventfd(descriptor);
    if (status != error_success) {
        return status;
    }

    auto* _event = reinterpret_cast<event*>(malloc(sizeof(event)));
    if (_event == nullptr) {
        ::close(descriptor);
        return error_allocation;
    }

    _event->fd = descriptor;

    *event_out = _event;
    return error_success;
}

void close(event* event) {
    ::close(event->fd);

    free(event);
}

descriptor get_descriptor(event* event) {
    return event->fd;
}

looper::error set(event* event) {
    if (::eventfd_write(event->fd, 1)) {
        return get_call_error();
    }

    return error_success;
}

looper::error clear(event* event) {
    eventfd_t value;
    if (::eventfd_read(event->fd, &value)) {
        return get_call_error();
    }

    return error_success;
}

}
