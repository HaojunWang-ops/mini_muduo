# mini-muduo

一个参考 `muduo` 核心设计实现的 `C++ Reactor` 网络库原型，包含 `base` 基础组件和 `net` 网络组件，支持异步日志、one loop per thread、多线程 IO、`poll` 事件分发、 `epoll` 事件分发、`eventfd` 跨线程唤醒、`timerfd` 定时器、非阻塞 TCP 连接管理和 `Buffer` 收发机制。

本项目主要用于学习和验证 Linux/C++ 高性能网络库的核心机制。

---

## 1. 项目简介

`mini-muduo` 是一个基于 `Reactor` 模型的 C++ 网络库原型，目标是实现一个可运行、可压测、可解释的 TCP server 框架。

项目分为两层：

- `base`：日志、线程、同步、时间戳等基础组件
- `net`：`EventLoop`、`Channel`、`Poller`、`TcpServer`、`TcpConnection`、`Buffer` 等网络组件

当前主要支持服务端 TCP 网络编程，已完成 `echo server` 示例，并使用自写 `epoll ping-pong client` 进行多连接压测。

---

## 2. 核心特性

- Reactor 事件驱动模型
- one loop per thread 线程模型
- 基于 `poll` 或 `epoll` 的 IO multiplexing
- `eventfd` 实现跨线程 `wakeup`
- `timerfd` 实现定时器
- `TcpServer / TcpConnection` 连接管理
- `Buffer` 输入/输出缓冲区
- 非阻塞 TCP 读写
- `outputBuffer_` 处理 `write` 写不完问题
- `EventLoopThreadPool` 多线程 IO
- `runInLoop` / `queueInLoop` 跨线程任务投递
- `AsyncLogging` 异步日志
- ASan / TSan / fd 泄漏 / 压测验证

---
## 3. 项目结构

```text
.
├── base
│   ├── AsyncLogging.cc
│   ├── AsyncLogging.h
│   ├── Atomic.h
│   ├── Condition.cc
│   ├── Condition.h
│   ├── copyable.h
│   ├── CountDownLatch.cc
│   ├── CountDownLatch.h
│   ├── CurrentThread.cc
│   ├── CurrentThread.h
│   ├── Exception.cc
│   ├── Exception.h
│   ├── FileUtil.cc
│   ├── FileUtil.h
│   ├── LogFile.cc
│   ├── LogFile.h
│   ├── Logging.cc
│   ├── Logging.h
│   ├── LogStream.cc
│   ├── LogStream.h
│   ├── Mutex.h
│   ├── noncopyable.h
│   ├── StringPiece.h
│   ├── Thread.cc
│   ├── Thread.h
│   ├── Timestamp.cc
│   ├── Timestamp.h
│   └── Types.h
├── net
│   ├── Acceptor.cc
│   ├── Acceptor.h
│   ├── Buffer.cc
│   ├── Buffer.h
│   ├── Callbacks.h
│   ├── Channel.cc
│   ├── Channel.h
│   ├── Endian.h
│   ├── EventLoop.cc
│   ├── EventLoop.h
│   ├── EventLoopThread.cc
│   ├── EventLoopThread.h
│   ├── EventLoopThreadPool.cc
│   ├── EventLoopThreadPool.h
│   ├── InetAddress.cc
│   ├── InetAddress.h
│   ├── poller
│   │   ├── DefaultPoller.cc
│   │   ├── EPollPoller.cc
│   │   ├── EPollPoller.h
│   │   ├── PollPoller.cc
│   │   └── PollPoller.h
│   ├── Poller.cc
│   ├── Poller.h
│   ├── Socket.cc
│   ├── Socket.h
│   ├── SocketsOps.cc
│   ├── SocketsOps.h
│   ├── TcpConnection.cc
│   ├── TcpConnection.h
│   ├── TcpServer.cc
│   ├── TcpServer.h
│   ├── Timer.cc
│   ├── Timer.h
│   ├── TimerId.h
│   ├── TimerQueue.cc
│   └── TimerQueue.h
├── examples
│   └── echo.cpp
├── scripts
│   ├── asan_build.sh
│   ├── ping-pong.cc
│   └── run_echo_bech.sh
├── docs
│   ├── benchmark.md
│   └── design_notes.md
├── CMakeLists.txt
└── README.md

```
___
## 4.架构设计
### 4.1 整体结构
```text
TcpServer
 ├── Acceptor
 ├── EventLoopThreadPool
 │    ├── EventLoopThread
 │    └── EventLoop
 └── TcpConnection
      ├── Channel
      ├── Socket
      ├── inputBuffer_
      └── outputBuffer_

EventLoop
 ├── Poller / EPollPoller
 ├── TimerQueue
 ├── wakeupFd
 └── pendingFunctors

```

### 4.2 线程模型
本项目采用了one loop per thread 模型。
- main loop 负责监听`socket` 和 `accept` 新连接
- IO loop负责具体连接的读写操作
- 每个`TcpConnection`只属于一个`EventLoop`
- `Channel / Poller` 操作必须在所属 `EventLoop` 线程中进行
- 跨线程操作通过 `runInLoop` / `queueInLoop` 投递
___
## 5. Base 基础组件
详细设计见 [docs/design_notes.md](docs/design_notes.md)

### 5.1 Logger
日志功能模块。`LogStream` 负责将各类数据格式化写入内部缓冲区，`Logger` 为日志宏入口，`Impl` 完成时间、线程号、级别、文件行号等前缀和后缀的格式化，`SourceFile` 用于截取文件名。

### 5.2 AsyncLogging
双缓冲异步日志系统。前端线程将日志写入内存 `buffer`，后端线程批量将 `buffer` 写入文件，减少直接写磁盘的阻塞。

### 5.3 Thread / MutexLock / Condition / CountDownLatch
封装 pthread 的线程、互斥锁、条件变量和倒计时门闩，为并发模块提供基础支持。

### 5.4 Timestamp
轻量级时间点类，以微秒精度记录从 Epoch 到当前的时间，支持时间比较、格式化输出和算术运算。

---

## 6. 网络基础组件
详细设计见 [docs/design_notes.md](docs/design_notes.md)

### 6.1 Channel
封装文件描述符 `fd` 及其关注的事件和回调。通过 `EventLoop` 更新 `Poller` 中的监听事件，并在事件就绪时分发读写、关闭、错误等回调。

### 6.2 Poller
I/O 多路复用的抽象基类，底层可切换 `poll` 或 `epoll`，管理 `fd` 到 `Channel` 的映射及事件注册。

### 6.3 EventLoop
Reactor 核心调度器，通常绑定一个线程。循环调用 `Poller::poll()` 等待事件，分发活跃的 `Channel` 回调，并支持定时任务和跨线程任务唤醒。

### 6.4 Acceptor
监听新连接，接受连接后通过回调将 `connfd` 交给 `TcpServer` 处理。

### 6.5 TcpServer
服务器主类，持有 `Acceptor` 和 `EventLoopThreadPool`。为新连接创建 `TcpConnection` 并分配合适的 `IO` 线程，管理所有连接的生命周期。

### 6.6 TcpConnection
表示一条已建立的 TCP 连接，负责该连接上的读写、缓冲区管理、状态维护及回调分发，支持优雅关闭。

### 6.7 EventLoopThread
封装一个运行 `EventLoop` 的线程，提供启动和退出接口。

### 6.8 EventLoopThreadPool
IO 线程池，管理一组 `EventLoopThread`，以轮询方式为新连接分配 `EventLoop`。

### 6.9 TimerQueue / TimerId / Timer
定时器管理模块。基于 `timerfd` 实现，支持一次性或重复定时任务的添加、到期触发和取消。
___

## 7.核心流程
服务启动、新连接建立、读写处理等核心流程的详细时序图与说明，
请参阅 [docs/design_notes.md](docs/design_notes.md)。

___
## 8. 编译运行
### 8.1 普通 Debug 构建
```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```
### 8.2 Release 构建
```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release -j
```
或者
```bash
./scripts/release_build.sh
```
### 8.3 Asan 构建
```bash
cmake -S . -B build-asan \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_ASAN=ON

cmake --build build-asan -j
```
或者
```bash
./scripts/asan_build.sh
```
运行：
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 ./build-asan/bin/echo_server 9981 4
```
### 8.4 Tsan 构建
```bash
cmake -S . -B build-tsan \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_TSAN=ON

cmake --build build-tsan -j
```
或者
```bash
./scripts/tsan_build.sh
```
运行
```bash
./build-tsan/bin/echo_server 9981 4
```
___
## 9. Echo Server 示例
运行：
```bash
./build-debug/bin/echo_server 9981 4
```

或者 `Asan` 版本:
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 ./build-asan/bin/echo_server 9981 4
```

参数含义：
```text
9981 :监听端口
4 : io线程数
```

压测：
```bash
./build-debug/bin/ping_pong 127.0.0.1 9981 64 1000 10
```

参数含义:
```text
127.0.0.1 server ip
9981      server port
64        block size
1000      connections
10        duration seconds
```
也可以使用脚本：
```bash
#构建release版本
./scripts/release_build.sh
#开始压测
#压测5组数据
./scripts/run_echo_becnch.sh
```
___
## 10. 测试与验证
### 10.1 Echo ping_pong 压测
测试方式：使用单线程`epoll client`建立多条TCP连接，对`echo server`进行`ping-pong`测试模式压测。每个连接在收到完整`echo block`后继续发送下一块数据。
该测试主要验证：
+ `TcpConnection`生命周期
+ `Buffer`收发
+ 非阻塞写
+ `outputBuffer_`读写
+ `EPOLLOUT`开关
+ 多连接稳定性
不代表极限吞吐

| block size | connections | duration | throughput | messages/s | closed |
| ---------: | ----------: | -------: | ---------: | ---------: | -----: |
|        64B |         100 |      10s |  1.44MiB/s |     235597 |      0 |
|        64B |        1000 |      30s |  1.17MiB/s |     576287 |      0 |
|        1KB |        1000 |      30s |  8.82MiB/s |     271325 |      0 |
|       64KB |         100 |      30s | 17.21MiB/s |       8315 |      0 |
|        1MB |          20 |      30s | 16.93MiB/s |        509 |      0 |
### 10.2 ASan 检查
测试内容:
+ heap-use-free
+ stack-use-after-scope
+ global-buffer-overflow
- double-free
- memory leak

结果：
```text
ASan 检查：在 64B × 1000 连接、64KB × 100 连接、1MB × 20 连接等 echo 压测场景下，server 无 ASan 报错、无崩溃、无断言失败
```
### 10.3 Tsan 检查
测试内容：
- data race
- 线程间未同步读写
- 锁使用错误

结果：
```text
Tsan检查：在64B × 1000 连接、64KB × 100 连接、1MB × 20 连接等 echo 压测场景下,EventLoop / TcpConnection 路径无明显 data race
```
### 10.4 fd 泄漏检查
观察命令：
```
watch -n 1 'ls /proc/$(pidof echo_server)/fd | wc -l'
```

验证方式：
```
压测过程中 fd 数随连接数上升；压测结束后 fd 数回落到初始水平附近。
```

结果：
```
压测结束后 fd 正常回落，无明显连接 fd 泄漏。
```
___

## 11. GTest测试

项目使用 GoogleTest 对部分基础组件进行了单元测试，主要覆盖：

- `Buffer`：append、retrieve、retrieveAll、扩容、索引变化等基础行为
- `LogStream`：整数、字符串、指针、边界值等格式化输出
- `Timestamp`：时间差计算、时间增加、字符串格式化
- `InetAddress`：IP / Port 构造与转换

测试主要用于验证基础组件的不变量和边界行为。复杂网络路径如 `TcpConnection` 生命周期、`EventLoopThreadPool` 分发、fd 泄漏等，主要通过 echo benchmark、ASan、TSan 和手动压测验证。

### 构建并运行测试

```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build-debug -j
ctest --test-dir build-debug --output-on-failure
```
___

## 12. 项目难点

### 12.1 TcpConnection 生命周期

`TcpConnection` 和 `Channel` 存在回调期间对象被释放的风险。项目使用 `shared_ptr` 管理 `TcpConnection`，并通过 `Channel` 的 `tie weak_ptr` 机制保证 `handleEvent` 期间连接对象存活。

### 12.2 跨线程任务投递

`TcpConnection` 所属 `IO loop` 和调用 `send`/`close` 的线程可能不同，因此涉及 `Channel`/`Poller` 的操作必须回到所属 `EventLoop` 执行。项目通过 `runInLoop` / `queueInLoop` 和 `eventfd` `wakeup` 保证跨线程任务被正确投递。

### 12.3 非阻塞写

非阻塞 `socket` 的 `write` 可能一次写不完，需要 `outputBuffer_` 保存剩余数据，并通过 `EPOLLOUT` 继续发送。写完后必须关闭写事件，避免 `EPOLLOUT` 空转。

### 12.4 TimerQueue 取消语义

`TimerQueue` 需要处理定时器未触发、正在触发、重复触发和取消等不同状态，因此使用 `timers_`、`activeTimers_` 和 `cancelingTimers_` 管理定时器生命周期。

### 12.5 异步日志双缓冲

`AsyncLogging` 需要在保证日志完整性的同时减少业务线程阻塞。项目通过 `currentBuffer_` / `nextBuffer_` / `buffers_` 和后台线程批量写入降低锁竞争和磁盘 IO 影响。
___
## 13. 当前边界

当前项目主要实现服务端 TCP 网络库核心，暂未实现：
- TcpClient / Connector
- HTTP 协议
- RPC 框架
- SSL/TLS
- 协程
- 生产级性能优化
- 完整 CI 测试

当前压测结果主要用于验证核心链路稳定性，不代表极限性能。
___
## 14. 后续计划
- 基于 mini-muduo 实现 RPC 框架
- 增加更完整的单元测试
- 增加 pipeline benchmark
- 补充 perf / 火焰图分析
- 继续完善文档和示例
___
## 15. 参考
- muduo 网络库
- Linux epoll
- eventfd / timerfd
- Reactor 模型
- 《Linux 多线程服务端编程》