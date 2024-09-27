
#include "os/os.h"


namespace looper::os {

resource::resource(descriptor descriptor)
    : m_descriptor(descriptor)
{}

descriptor resource::get_descriptor() const {
    return m_descriptor;
}

}
