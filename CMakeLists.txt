cmake_minimum_required(VERSION 3.16)

project(mini_muduo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(BUILD_EXAMPLES "Build examples" ON)
option(BUILD_SCRIPTS "Build scripts" ON)

if (ENABLE_ASAN AND ENABLE_TSAN)
    message(FATAL_ERROR "ASan and TSan cannot be enabled at the same time")
endif()

find_package(Threads REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
    base/Thread.cc
    base/Timestamp.cc

    net/Acceptor.cc
    net/Buffer.cc
    net/Channel.cc
    net/EventLoop.cc
    net/EventLoopThread.cc
    net/EventLoopThreadPool.cc
    net/InetAddress.cc
    net/Poller.cc
    net/poller/DefaultPoller.cc
    net/poller/EPollPoller.cc
    net/poller/PollPoller.cc
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

function(enable_sanitizes target_name)
    if (ENABLE_ASAN)
        target_compile_options(${target_name} PRIVATE
            -fsanitize=address
            -fno-omit-frame-pointer
            -O0
            -g
        )
        target_link_options(${target_name} PRIVATE
            -fsanitize=address
        )
    endif()

    if (ENABLE_TSAN)
        target_compile_options(${target_name} PRIVATE
            -fsanitize=thread
            -fno-omit-frame-pointer
            -O1
            -g
        )
        target_link_options(${target_name} PRIVATE
            -fsanitize=thread
        )
    endif()
endfunction()

enable_sanitizes(mini_muduo)

if (BUILD_EXAMPLES)
    add_executable(echo_server
        examples/echo.cpp
    )

    target_link_libraries(echo_server
        PRIVATE
            mini_muduo
    )

    enable_sanitizes(echo_server)
endif()

if (BUILD_SCRIPTS)
    add_executable(ping_pong
        scripts/ping_pong.cc
    )
    target_link_libraries(ping_pong
        PRIVATE
            Threads::Threads
    )

    enable_sanitizes(ping_pong)
endif()

#[[
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON ..

cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON ..

cmake -DCMAKE_BUILD_TYPE=Release .. (此时两个开关默认都是 OFF)
]]