cmake_minimum_required(VERSION 3.30)
project(os_lab3 C)

set(CMAKE_C_STANDARD 11)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(os_lab3 main.c
        common/os/thread_pool.h
        common/raii.h
        common/unlikely.h
        common/os/posix/threads.h
        common/std.h
        common/std.h
        common/os/posix/mutex.h)

target_link_libraries(os_lab3 PRIVATE Threads::Threads)
