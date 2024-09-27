
#include <cstdio>
#include <looper.h>


int main() {
    auto loop = looper::create();

    looper::execute_on(loop, [](looper::loop loop)->void {
        printf("hey\n");
    });

    looper::run_once(loop);
    looper::destroy(loop);

    return 0;
}
