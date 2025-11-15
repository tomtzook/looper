# Looper

**looper** provides easy-to-use non-blocking operations. Including
socket IO and code execution utilities.

## Working with a Loop

Each loop is its own entity and manages its own attached objects.
Once created, objects may be created on this loop

For example, creating a loop and a timer on it.
```c++
using namespace std::chrono_literals;

// create the loop
const auto loop = looper::create();

// create a timer with a 10 second timeout. The callback will be called
// on timeout.
const auto timer = looper::create_timer(loop, 10s, [](looper::timer timer)->void {
    // reset the timer to start again
    looper::reset_timer(timer);
});
// start the timer
looper::start_timer(timer);
```

The loop must be run manually or configured to run automatically.
```c++
// will use the current thread to run the loop.
// blocks until the loop is destoryed.
looper::run_forever(loop);

// will execute this loop in a new thread.
// all loop objects are thread safe.
looper::exec_in_thread(loop);
```

All objects (including loops) are referred to with handles returned by calls.
These handles are just references, and do not hold any information or capabilities on their own.

Once done, the loop can be destroyed to clear it and all remaining objects.
```c++
// this loop handle will no longer refer to any loop after this call, so don't use it
looper::destory(loop);
```

### RAII with Handles

If _RAII_ is wanted, use the `looper_cxx.hpp` header to access `handle_holder`s:
```c++
const auto loop = looper::make_loop();
looper::exec_in_thread(loop);

{
    const auto event = looper::make_event(loop, [](const looper::event event)->void {
        // clear the event as handled
        looper::clear_event(event);
    });
    
    // set the event, the callback will be called
    looper::set_event(event);
    
    sleep(10);
}
// the event object is destroyed now that we've reached the end of the scope.
```

### TCP Sockets

Creating a new TCP client, binding it to a port and connecting to a server
```c++
const auto tcp = looper::create_tcp();
looper::bind_tcp(tcp, 5511); // bind to port 5511

// start connection attempt. when finished, the callback will be called. This function may or may not exit
// by then, depending on when the connection is finished; but only rely on the callback.
looper::connect_tcp(tcp, "172.22.11.2", 5500, [](const looper::tcp tcp, looper::error error)->void {
    if (error != looper::error_success){
        // connection finished with error
        // close the socket
        looper::destroy_tcp(tcp);
    } else {
        // connection finished successfully, can read and write
    }
});
```

When connected, writing and reading can now be done.
```c++
uint8_t buffer[] = "Hello World";
looper::write_tcp(tcp, std::span<const uint8>{buffer, sizeof(buffer)}, [](const looper::tcp tcp, looper::error error)->void {
    if (error != looper::error_success){
        // write failed, see error code
    } else {
        // write succeeded
    }
});
```

Reading will be done automatically when started. Data will be passed as it arrives.
Stop reading when finished.
```c++
looper::start_tcp_read(tcp, [](const looper::tcp tcp, const std::span<const uint8_t>& data, const looper::error error)->void {
    if (error != looper::error_success){
        // read failed, see error code
        // should close socket
        looper::destory_tcp(tcp);
    } else {
        // received new data, see data
    }
});

// to stop
looper::stop_tcp_read(tcp);
```
