cmake_minimum_required(VERSION 3.28)
project(looper
        LANGUAGES C CXX
        VERSION "1.0.0")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if (NOT DEFINED TRACE_LEVEL)
    set(TRACE_LEVEL 1)
endif ()
if (NOT DEFINED TRACE_SINK)
    if (UNIX)
        set(TRACE_SINK 1)
    else ()
        set(TRACE_SINK 0)
    endif ()
endif ()

add_compile_definitions(TRACE_LEVEL=${TRACE_LEVEL} TRACE_SINK=${TRACE_SINK})

add_library(looper STATIC
        include/looper.h
        include/looper_trace.h
        include/looper_types.h

        src/looper.cpp
        src/trace.cpp
        src/util/handles.h
        include/looper_except.h
        src/poll.h
        src/loop.h
        src/loop.cpp
        src/os/linux/epoll_poller.h
        src/os/os.h
        src/os/os.cpp
        src/os/linux/linux.h
        src/os/linux/linux.cpp
        src/factory.h
        src/poll.cpp
        src/os/linux/epoll_poller.cpp
        src/factory.cpp
        src/instance.cpp)
target_include_directories(looper
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(test1 test1.cpp
        src/instance.h)
target_link_libraries(test1 looper)
