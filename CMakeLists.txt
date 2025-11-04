cmake_minimum_required(VERSION 3.28)
project(looper
        LANGUAGES C CXX
        VERSION "1.0.0")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

include(GNUInstallDirs)

if (NOT DEFINED TRACE_LEVEL)
    set(TRACE_LEVEL 2)
endif ()
if (NOT DEFINED TRACE_SINK)
    if (UNIX)
        set(TRACE_SINK 0)
    else ()
        set(TRACE_SINK 0)
    endif ()
endif ()

add_compile_definitions(TRACE_LEVEL=${TRACE_LEVEL} TRACE_SINK=${TRACE_SINK})

file(GLOB INCLUDE_FILES ${CMAKE_SOURCE_DIR}/include/*.h)

set(SOURCES
        include/looper.h
        include/looper_trace.h
        include/looper_types.h
        include/looper_except.h

        src/util/util.h
        src/util/handles.h
        src/os/os.h
        src/os/factory.h
        src/loop/loop_structs.h
        src/loop/loop_internal.h
        src/loop/loop.h
        src/types_internal.h
        src/looper_base.h

        src/looper.cpp
        src/looper_tcp.cpp
        src/looper_udp.cpp
        src/trace.cpp
        src/loop/loop.cpp
        src/loop/loop_base.cpp
        src/loop/loop_tcp.cpp
        src/loop/loop_udp.cpp
        src/loop/loop_stream.cpp
        src/loop/loop_resource.cpp
        src/loop/loop_udp.h
        src/loop/loop_event.cpp
        src/loop/loop_timer.cpp
        src/loop/loop_future.h
        src/loop/loop_future.cpp
        src/looper_types.cpp
        src/util/handles.cpp
)

if (UNIX)
    set(SOURCES ${SOURCES}
            src/os/linux/linux.h
            src/os/linux/linux_event.cpp
            src/os/linux/linux_socket.cpp
            src/os/linux/epoll_poller.cpp
            src/os/linux/linux_file.cpp
            src/os/linux/linux_io.cpp
    )
endif ()

add_library(looper STATIC ${SOURCES})
target_include_directories(looper
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include/looper>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)

install(TARGETS looper EXPORT looper
        LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(FILES ${INCLUDE_FILES}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/looper)
install(EXPORT looper
        FILE looperConfig.cmake
        NAMESPACE looper::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/looper")

add_executable(test_client
        test_client1.cpp)
target_link_libraries(test_client looper)
add_executable(test_server test_server1.cpp)
target_link_libraries(test_server looper)