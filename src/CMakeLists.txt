cmake_minimum_required(VERSION 3.30)
project(os_lab3 C)

set(CMAKE_C_STANDARD 11)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(os_lab3 main.c
        common/os/thread_pool.h
        common/os/thread_pool.c
        common/raii.h
        common/unlikely.h
        common/os/posix/threads.h
        common/std.h
        common/os/posix/mutex.h
        matrix.h
        matrix.c
        input.h
        input.c
        convolution.c
        convolution.h)

target_include_directories(os_lab3 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(os_lab3 PRIVATE Threads::Threads)
