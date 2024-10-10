#pragma once

#include "os/os.h"

namespace looper::os {

class linux_event : public event {
public:
    linux_event();
    ~linux_event() override;

    void set() override;
    void clear() override;
};

}