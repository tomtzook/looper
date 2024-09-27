#pragma once

#include <looper.h>

#include "util/handles.h"
#include "loop.h"
#include "factory.h"

namespace looper {

class instance {
public:
    instance();
    ~instance();

    loop create();
    void destroy(loop loop);

    void run_once(loop loop);
    void run_forever(loop loop);

    void execute_on(loop loop, execute_callback&& callback);
    void execute_on_and_wait(loop loop, execute_callback&& callback);

private:
    struct loop_data {
    public:
        explicit loop_data(handle handle)
            : loop(handle, create_poller())
        {}

        loop_impl loop;
    };

    handles::handle_table<loop_data, 8> m_loops;
};

}
