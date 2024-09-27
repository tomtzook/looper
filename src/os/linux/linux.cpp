
#include <unistd.h>

#include "linux.h"


namespace looper::os {

linux_resource::linux_resource(descriptor descriptor)
    : resource(descriptor)
{}

linux_resource::~linux_resource() {
    close();
}

void linux_resource::close() {
    ::close(get_descriptor());
}

}
