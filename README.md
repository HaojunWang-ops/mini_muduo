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
### 5.2 AsyncLogging
实现了双缓冲异步日志系统。前端线程负责将日志格式化到内存 `buffer`，后台日志线程批量将 `buffer` 写入文件，减少业务线程直接写磁盘造成的阻塞。
+ `AsyncLogging`分成前端`append`和后端`threadFunc`。
	+ 前端`append`负责将日志内容写入`currentBuffer_`。
	   如果`currentBuffer_`空间足够，就直接`append`。
	   如果`currentBuffer_`写满，就把`currentBuffer_`移入`buffers_`，通知后端线程。
	   然后优先使用`nextBuffer_`作为新的`currentBuffer_`;如果`nextBuffer_`已经被用掉，就临时new一个新的`Buffer`.
	   因此当前端写入速度远大于后端落盘速度时，`buffers_`中会积压多个`buffer`，也可能发生额外内存分配。
	+ 后端`threadFunc`负责把`buffer`内容写入`LogFile`。
	   它会通过条件变量等待：要么被前端唤醒，要么超时自动醒来做`flush`。
	   醒过来后，后端在锁内把`currentBuffer_`也移入`buffers_`，然后用`newBuffer1`来替换`currentBuffer_`，再把`buffers_`和`bufferToWrite`交换。
	   如果`nextBuffer_`为空，则用`newBuffer2`补充`nextBuffer_`。
	   这样锁内操作很短，前端可以尽快继续写日志。

	  锁外，后端遍历`buffersToWrite`，把每个`buffer`的内容`append`到`LogFile`。
	  写完后，后端最多保留两个空`buffer`作为`newBuffer1`和`newBuffer2`继续复用，多余的`buffer`会随着`unique_ptr`析构被释放。
	  随后清空`buffersToWrite`，并`flush`到文件。
+ `LogFile` 和 `FileUtil`
	+ `AppendFile`封装`FILE*`，负责把日志内容追加写入`fp_`，并维护`writtenBytes_`和`flush`操作
	+ `LogFile`持有`AppendFile`，负责日志文件管理。它可以根据`basename`、时间、主机名、`pid`生成日志文件名；当文件大小超过`rollSize_`或跨天滚动到新文件；同时根据`append()`对外接口，根据是否启用`threadSafe`决定是否加锁，最终调用`appned_unlocked()`写入`AppendFile`。`LogFile`还会根据`flushInterval_`定期`flush`，保证日志不会长期停留在用户态缓冲区。
	
### 5.3 Thread / Mutex / Condition / CountDownLatch
封装 pthread 线程、互斥锁、条件变量和启动同步工具，为 EventLoopThread、AsyncLogging 等模块提供基础并发能力。
+ `Thread`类是对`pthread`的`C++`封装，负责保存线程函数、线程名、线程id，并提供`start()`和`join`管理线程生命周期。
   `start()`是对外启动接口，内部调用`pthread_create()`创建线程。`pthread_create()`要求传入一个`void*(*)(void*)`形式的线程入口函数，所以`muduo`定义了`startThread(void*)`作为适配器。
   `ThreadData`是传给`startThread`的参数，里面保存了真正要执行的函数`func_`、线程名`name_`、用于写回`tid_`的指针，以及用于同步线程启动完成的`CountDownLatch`。新线程启动后，`startThread`会把`void*`转回`ThreadData*`，调用`runInThread()`，完成`tid`写回、线程名设置、`latch`通知，最后执行用户函数`func_`。
+ `Mutex`类
+ `Condition`类
+ `CountDownLatch`类
___
# 6. Net 基础组件
# 7.核心流程
___
# 8. 编译运行
## 8.1 普通 Debug 构建
```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```
## 8.2 Release 构建
```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```
## 8.3 Asan 构建
```bash
cmake -S . -B build-asan \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_ASAN=ON

cmake --build build-asan -j
```

运行：
```bash
ASAN_OPTIONS=abort_on_error=1:detect_leaks=1 ./build-asan/main 9981 4
```
## 8.4 Tsan 构建
```bash
cmake -S . -B build-tsan \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_TSAN=ON

cmake --build build-tsan -j
```

运行
```bash
./build-tsan/main 9981 4
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
___
# 10. 测试与验证
## 10.1 Echo ping_pong 压测
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
## 10.2 ASan 检查
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
## 10.3 Tsan 检查
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