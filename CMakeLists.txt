cmake_minimum_required(VERSION 3.16)

project(mini_muduo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -fsanitize=address -fno-omit-frame-pointer")

find_package(Threads REQUIRED)

add_library(mini_muduo STATIC
    base/AsyncLogging.cc
    base/LogFile.cc
    base/LogStream.cc
    base/Condition.cc
    base/CountDownLatch.cc
    base/CurrentThread.cc
    base/Exception.cc
    base/Logging.cc
    base/FileUtil.cc
    base/ProcessInfo.cc
    base/Thread.cc
    base/Timestamp.cc
    
    net/Acceptor.cc
    net/Buffer.cc
    net/Channel.cc
    net/EventLoop.cc
    net/EventLoopThread.cc
    net/InetAddress.cc
    net/Poller.cc
    net/Socket.cc
    net/SocketsOps.cc
    net/TcpServer.cc
    net/TcpConnection.cc
    net/Timer.cc
    net/TimerQueue.cc
)

target_include_directories(mini_muduo
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/base
        ${CMAKE_CURRENT_SOURCE_DIR}/net
)

target_link_libraries(mini_muduo
    PUBLIC
        Threads::Threads
)

target_compile_options(mini_muduo
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(main main.cpp)

target_link_libraries(main
    PRIVATE
        mini_muduo
)

enable_testing()

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
)

foreach(test_src ${TEST_SOURCES})
    get_filename_component(test_name ${test_src} NAME_WE)

    add_executable(${test_name} ${test_src})
    target_link_libraries(${test_name} PRIVATE mini_muduo)

    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
