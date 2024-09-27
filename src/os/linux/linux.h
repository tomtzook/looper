#pragma once

#include "os/os.h"

namespace looper::os {

class linux_resource : public resource {
public:
    explicit linux_resource(descriptor descriptor);
    ~linux_resource() override;

    void close();
};

}