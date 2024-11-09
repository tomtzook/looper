cmake_minimum_required(VERSION 3.28)
project(looper
        LANGUAGES C CXX
        VERSION "1.0.0")

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

if (NOT DEFINED TRACE_LEVEL)
    set(TRACE_LEVEL 2)
endif ()
if (NOT DEFINED TRACE_SINK)
    if (UNIX)
        set(TRACE_SINK 0) # 1
    else ()
        set(TRACE_SINK 0)
    endif ()
endif ()

add_compile_definitions(TRACE_LEVEL=${TRACE_LEVEL} TRACE_SINK=${TRACE_SINK})

set(SOURCES
        include/looper.h
        include/looper_trace.h
        include/looper_types.h
        include/looper_except.h
        include/looper_tcp.h

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
        src/trace.cpp
        src/loop/loop.cpp
        src/loop/loop_base.cpp
        src/loop/loop_tcp.cpp
)

if (UNIX)
    set(SOURCES ${SOURCES}
            src/os/linux/linux.h
            src/os/linux/linux_event.cpp
            src/os/linux/linux_socket.cpp
            src/os/linux/epoll_poller.cpp
    )
endif ()

add_library(looper STATIC ${SOURCES})
target_include_directories(looper
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:looper>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(test1 test1.cpp)
target_link_libraries(test1 looper)
target_include_directories(test1
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(test2 test2.cpp)
target_link_libraries(test2 looper)

add_executable(test3 test3.cpp)
target_link_libraries(test3 looper)

add_executable(test4 test4.cpp)
target_link_libraries(test4 looper)

add_executable(test5 test5.cpp)
target_link_libraries(test5 looper)