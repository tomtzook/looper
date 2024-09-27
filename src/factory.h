#pragma once

#include <memory>

#include "poll.h"


namespace looper {

std::unique_ptr<poller> create_poller();

}
