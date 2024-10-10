#pragma once

#include <memory>

#include "os/os.h"
#include "poll.h"


namespace looper {

std::unique_ptr<poller> create_poller();
std::shared_ptr<os::event> create_event();

}
