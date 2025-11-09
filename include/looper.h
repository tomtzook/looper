#pragma once

#include <chrono>

#include <looper_types.h>
#include <looper_except.h>

// todo: "interface" for dgram and stream work (tcp, unix socket, servers // udp)
//  unify server and client types?

namespace looper {

/**
 * Creates a new loop. Each loop exists individually and manages its own objects and resources.
 * The loop is created empty.
 *
 * @return loop handle
 */
loop create();

/**
 * Destroys a given loop. Any attached objects are destroyed as well. Any run calls will result in exceptions.
 * If currently running (due to a call to run_for, run_forever, or exec_in_thread), it will be stopped.
 *
 * @param loop loop
 */
void destroy(loop loop);

/**
 * Queries as to the loop which the given handle belongs to. If the handle is of a loop, an exception
 * is thrown.
 *
 * @param handle handle to query
 * @return handle of the owning loop.
 */
loop get_parent_loop(handle handle);

/**
 * Runs the loop once. This blocks the current thread until finished running.
 *
 * @param loop loop handle
 */
void run_once(loop loop);

/**
 * Runs the loop several runs until the given time passes. The amount of loop occurring cannot be specified
 * as this depends on the activity in the loop. This blocks the current thread until finished running.
 *
 * @param loop loop handle
 * @param time time to run
 */
void run_for(loop loop, std::chrono::milliseconds time);

/**
 * Runs the loop forever, only returning when the loop is destroyed. This blocks the current thread until finished running.
 *
 * @param loop loop handle
 */
void run_forever(loop loop);

/**
 * Runs the loop forever in a separate thread. The thread will terminate when the loop is destroyed. Calls to `run_`
 * on the loop will no longer be possible. Does not block.
 *
 * @param loop loop handle
 */
void exec_in_thread(loop loop);

/**
 * Create a new future object and attaches it to the given loop. A future provides a timed execution of
 * a callback. When can schedule the callback to execute after some time. This can be done multiple times
 * sequentially, but not in parallel.
 *
 * @param loop loop handle
 * @param callback callback to call when the future executes
 * @return future handle
 */
future create_future(loop loop, future_callback&& callback);

/**
 * Destroys the given future, making it unusable. If currently executing, the destruction will occur
 * after execution.
 *
 * @param future future handle
 */
void destroy_future(future future);

/**
 * Executes the given future once after the given delay has passed. If not delay is given, the future
 * will execute as soon as possible. There is no guarantee of accuracy of execution timing in relation to the wanted
 * delay.
 *
 * When executing, the callback of the future will be called.
 *
 * @param future future handle
 * @param delay wanted delay for execution, or 0 for no delay
 */
void execute_once(future future, std::chrono::milliseconds delay = no_timeout);

/**
 * Waits for the execution of a future to finish. If the future was not called to be executed, then this
 * will return immediately with `false`.
 *
 * @param future future handle
 * @param timeout timeout for waiting, or 0 for no timeout
 * @return false if the future has executed, or true if timed out.
 */
bool wait_for(future future, std::chrono::milliseconds timeout = no_timeout);

/**
 * Creates a future inplace with the given callback and executes it. The future will be destroyed
 * afterward.
 *
 * @param loop loop handle
 * @param callback execution callback for future
 */
void execute_later(loop loop, loop_callback&& callback);

/**
 * Creates a future inplace with the given callback and executes it. This function will wait until
 * the future finishes execution before returning. The future will be destroyed afterward.
 *
 *
 * @param loop loop handle
 * @param callback execution callback for future
 * @param timeout timeout for waiting for the future to execute, or 0 for no timeout.
 * @return false if executed, or true if timed out.
 */
bool execute_later_and_wait(loop loop, loop_callback&& callback, std::chrono::milliseconds timeout = no_timeout);

/**
 * Creates a new event object and attaches it to the given loop. An event can be set or cleared.
 * When set, the callback will execute. Clearing the event is necessary before setting it again or nothing
 * will occur.
 * The event object is created as clear.
 *
 * @param loop loop handle
 * @param callback callback to be called on event occurrence.
 * @return event handle
 */
event create_event(loop loop, event_callback&& callback);

/**
 * Destroys the given event, making it unusable.
 *
 * @param event event handle
 */
void destroy_event(event event);

/**
 * Mark event as `set` indicating that the event has occurred. This will cause execution of the callback.
 * Calling this when the event is already set will do nothing. Clearing is thus neceesary.
 *
 * @param event event handle
 */
void set_event(event event);

/**
 * Removes the event from `set` state, indicating the event has been handled. If it is already clear, then
 * nothing happens.
 *
 * @param event event handle
 */
void clear_event(event event);

/**
 * Creates a new timer object and attaches it to the given loop. A timer counts time from its start
 * and will execute its callback upon reaching timeout if not stopped prior.
 * The timer objects is created as stopped and reset.
 *
 * @param loop loop handle
 * @param timeout timeout for timer
 * @param callback callback to execute on timeout
 * @return timer handle.
 */
timer create_timer(loop loop, std::chrono::milliseconds timeout, timer_callback&& callback);

/**
 * Destroys the given timer, making it unusable.
 *
 * @param timer timer handle
 */
void destroy_timer(timer timer);

/**
 * Starts the timer counting. If the timer was started and stopped previously, the count will restart.
 *
 * @param timer timer handle
 */
void start_timer(timer timer);

/**
 * Stops the timer counting. If the timer is already stopped, nothing occurs.
 *
 * @param timer timer handle
 */
void stop_timer(timer timer);

/**
 * Resets the timer state. This should be used after a call to start was made, to reset the count of the timer.
 * The timer will continue running. If called when the timer is stopped, nothing will happen.
 *
 * @param timer timer handle
 */
void reset_timer(timer timer);

// tcp
/**
 * Creates a new tcp client object and attaches it to the given loop. This object provides a tcp
 * client. At the time of creation, the socket is neither connected nor bound.
 *
 * @param loop loop handle
 * @return tcp handle
 */
tcp create_tcp(loop loop);

/**
 * Destroys the given tcp, making it unusable. If was connected, the connection is terminated.
 *
 * @param tcp tcp handle
 */
void destroy_tcp(tcp tcp);

/**
 * Binds the tcp client to a specific port on the local machine. The IP address is not specified and as such
 * will work with any interface. If the tcp is already bound or connection an exception will be thrown.
 *
 * @param tcp tcp handle
 * @param port port number
 */
void bind_tcp(tcp tcp, uint16_t port);

/**
 * Binds the tcp client to a specific IP and port on the local machine.
 * If the tcp is already bound or connection an exception will be thrown.
 *
 * @param tcp tcp handle
 * @param address IPv4 address
 * @param port port number
 */
void bind_tcp(tcp tcp, std::string_view address, uint16_t port);

/**
 * Attempts to connect the client to a remote tcp server. If not bound, the socket will be bound
 * automatically to some port. If already connected, and exception is thrown.
 * This merely starts the connection process, when finished, whether successful or not, the given callback
 * will be called.
 *
 * @param tcp tcp handle
 * @param address server IPv4 address
 * @param port server port
 * @param callback callback called on connection finished or failed.
 */
void connect_tcp(tcp tcp, std::string_view address, uint16_t port, tcp_callback&& callback);

/**
 * Starts automatic reading from the client. Must be connected to do so. When new data arrives, the given
 * callback will be invoked with it. If any errors occur while reading, the callback will be called with the error
 * information. If already reading an exception is thrown.
 *
 * @param tcp tcp handle
 * @param callback callback called on read or error
 */
void start_tcp_read(tcp tcp, read_callback&& callback);

/**
 * Stops automatic reading of the tcp client. If not reading, nothing occurs.
 *
 * @param tcp tcp handle
 */
void stop_tcp_read(tcp tcp);

/**
 * Writes data over the tcp client. Must be connected to do so. When writing is finished, whether
 * successful or not, the callback will be called with information about it.
 *
 * @param tcp tcp handle
 * @param buffer data buffer to write
 * @param callback callback for write result
 */
void write_tcp(tcp tcp, std::span<const uint8_t> buffer, write_callback&& callback);

/**
 * Creates a new tcp server object and attaches it to the given loop. This provides a tcp server.
 * At the time of creation, the socket is neither connected nor bound.
 *
 * @param loop loop handle
 * @return tcp server handle
 */
tcp_server create_tcp_server(loop loop);

/**
 * Destroys the given tcp, making it unusable. If was connected to any client, the connections are terminated.
 *
 * @param tcp tcp server handle
 */
void destroy_tcp_server(tcp_server tcp);

/**
 * Binds the tcp server to a specific port on the local machine. The IP address is not specified and as such
 * will work with any interface. If the tcp is already bound or connection an exception will be thrown.
 *
 * @param tcp tcp server handle
 * @param port port number
 */
void bind_tcp_server(tcp_server tcp, uint16_t port);

/**
 * Binds the tcp server  to a specific IP and port on the local machine.
 * If the tcp is already bound or connection an exception will be thrown.
 *
 * @param tcp tcp server handle
 * @param address IPv4 address to bind to
 * @param port port to bind to
 */
void bind_tcp_server(tcp_server tcp, std::string_view address, uint16_t port);

/**
 * Start the socket to listen for incoming connection. This does not block. When a connection attempt is
 * made, the callback will be called.
 *
 * @param tcp tcp server handle
 * @param backlog backlog of connections pending
 * @param callback callback to call on connection attempt
 */
void listen_tcp(tcp_server tcp, size_t backlog, tcp_server_callback&& callback);

/**
 * Accept a pending client connection. Should be called from a listen callback.
 *
 * @param tcp tcp server handle
 * @return new connected tcp client handle
 */
tcp accept_tcp(tcp_server tcp);

/**
 * Creates a new udp object and attaches it to the given loop.
 * At the time of creation, the socket is not bound.
 *
 * @param loop loop handle
 * @return udp handle
 */
udp create_udp(loop loop);

/**
 * Destroys the given tcp, making it unusable.
 *
 * @param udp udp handle
 */
void destroy_udp(udp udp);

/**
 * Binds the udp to a specific port on the local machine. The IP address is not specified and as such
 * will work with any interface. If the udp is already bound an exception will be thrown.
 *
 * @param udp udp handle
 * @param port port number
 */
void bind_udp(udp udp, uint16_t port);

/**
 * Starts automatic reading from the socket. When new data arrives, the given
 * callback will be invoked with it. If any errors occur while reading, the callback will be called with the error
 * information. If already reading an exception is thrown.
 *
 * @param udp udp handle
 * @param callback callback called on read or error
 */
void start_udp_read(udp udp, udp_read_callback&& callback);

/**
 * Stops automatic reading of the socket. If not reading, nothing occurs.
 *
 * @param udp udp handle
 */
void stop_udp_read(udp udp);

/**
 * Writes data over the udp. When writing is finished, whether
 * successful or not, the callback will be called with information about it.
 *
 * @param udp udp handle
 * @param destination destination IPv4 IP and Port
 * @param buffer data to write
 * @param callback callback on write finished
 */
void write_udp(udp udp, inet_address_view destination, std::span<const uint8_t> buffer, udp_callback&& callback);

}
