cmake_minimum_required(VERSION 3.10)

project(mini_muduo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g")

add_compile_options(-Wall -Wextra -g)

find_package(Threads REQUIRED)

add_library(mini_muduo STATIC
    src/Channel.cc
    src/EventLoop.cc
    src/Poller.cc
    src/Timestamp.cc
    src/currentThread.cc
    src/Timer.cc
    src/TimerQueue.cc
)

target_include_directories(mini_muduo PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(mini_muduo PUBLIC
    Threads::Threads
)

add_executable(main main.cpp)
target_link_libraries(main PRIVATE mini_muduo)