#pragma once

#include <memory>

#include "os/os.h"
#include "poll.h"


namespace looper::os {

std::unique_ptr<poller> create_poller();
std::shared_ptr<event> create_event();

}
