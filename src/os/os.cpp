
#include "os/os.h"


namespace looper::os {

resource::resource(descriptor descriptor, resource_type type)
    : m_descriptor(descriptor)
    , m_type(type)
{}

descriptor resource::get_descriptor() const {
    return m_descriptor;
}

resource_type resource::get_type() const {
    return m_type;
}

event::event(descriptor descriptor)
    : resource(descriptor, resource_type::event)
{}

}
