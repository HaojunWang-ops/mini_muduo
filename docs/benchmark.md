# Benchmark

本文记录 mini_muduo 的基础压测结果，主要验证：
- 多线程 Reactor 是否能正常工作
- 连接建立与关闭是否稳定
- Echo Server 在并发连接下是否存在明显错误
- 是否存在 fd 泄漏、ASan 报错、TSan报错等问题

## 1. 测试环境

| 项目         | 配置                                          |
| ---------- | ------------------------------------------- |
| OS         | Ubuntu / Linux VM                           |
| CPU        | 13th Gen Intel(R) Core(TM) i7-13650HX       |
| Memory     | 4GiB                                        |
| Compiler   | g++ (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0 |
| Build Type | Debug / Release /ASan Debug / TSan Debug                           |
| Sanitizer  | AddressSanitizer/ThreadSanitizer            |
| Network    | localhost                                   |

> 注：当前测试主要用于功能正确性和稳定性验证，不作为严肃性能对比。

## 2. 编译方式
### 2.1 普通 Debug 构建
```bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
```
### 2.2 Release 构建
```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
```
### 2.3 Asan 构建
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
### 2.4 Tsan 构建
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
### 2.5 Echo Server 示例
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

### 2.6 可复现的性能与稳定性测试

使用 `scripts/run_echo_bench.sh` 启动独立的服务端并运行 ping-pong 场景。默认执行基础吞吐矩阵；每个场景结束后会采集服务端资源指标，并将结果及服务端日志保存到独立目录。

```bash
./scripts/run_echo_bench.sh ./build-release
```

用 `BENCH_CASES` 覆盖默认矩阵。每个场景格式为
`block_size,connections,duration_seconds`，多个场景以空格分隔：

```bash
PORT=9981 THREADS=4 \
BENCH_CASES="64,1000,30 65536,100,30" \
./scripts/run_echo_bench.sh ./build-release
```

用可选的 `SOAK_CASE` 增加长时间稳定性场景；建议先从 10 分钟开始，再逐步延长。`BENCH_LOG_DIR` 可指定产物目录，便于保留和对比结果。

```bash
BENCH_CASES="64,1000,30" \
SOAK_CASE="64,1000,600" \
BENCH_LOG_DIR=/tmp/mini-muduo-soak \
./scripts/run_echo_bench.sh ./build-release
```

脚本输出并记录以下指标：

- `fd_count`：服务端打开的文件描述符数量；结束后应回落至接近 `baseline`。
- `rss_kib`、`vm_size_kib`：常驻/虚拟内存；长时间运行中不应持续无界增长。
- `threads`：服务端线程数；应与配置的 I/O 线程数及基础线程保持一致。
- `cpu_percent`：自进程启动以来的平均 CPU 使用率，仅用于同机、同配置下的趋势对比。

结果保存在 `benchmark-results.txt`，服务端日志保存在同目录的 `server.log`。性能结果应同时记录 CPU、内存、编译模式、连接数和持续时间；本地 VM 数据仅用于回归比较，不能代表生产吞吐上限。
## 3. 测试工具

使用自写 echo client / 压测脚本进行测试。
测试内容包括：
- 短连接压测
- 长连接 echo 压测
- 多线程连接分布检查
- fd 泄漏检查 
- ASan 检查
- TSan检查

## 4. 测试结果

### 4.1 Echo ping_pong 压测
测试方式：使用单线程`epoll client`建立多条TCP连接，对`echo server`进行`ping-pong`测试模式压测。每个连接在收到完整`echo block`后继续发送下一块数据。
该测试主要验证：
+ `TcpConnection`生命周期
+ `Buffer`收发
+ 非阻塞写
+ `outputBuffer_`读写
+ `EPOLLOUT`开关
+ 多连接稳定性
不代表极限吞吐

在本地 Ubuntu VM 上使用 Release 构建运行默认矩阵。服务端使用 4 个 I/O 线程，单线程 `epoll` 客户端通过 loopback 发起连接；机器对该 VM 暴露 2 个逻辑 CPU。每行的吞吐、消息数均为该轮最终输出，不能与不同机器或不同日志配置直接比较。

| block size | connections | duration | throughput | messages | messages/s | closed |
| ---------: | ----------: | -------: | ---------: | --------: | ---------: | -----: |
|        64B |         100 |   10.001s |  1.91 MiB/s |   313,360 |  31,333.96 |      0 |
|        64B |        1000 |   30.004s |  1.92 MiB/s |   941,644 |  31,384.30 |      0 |
|        1KB |        1000 |   30.023s | 13.40 MiB/s |   411,974 |  13,721.76 |      0 |
|       64KB |         100 |   30.030s | 22.97 MiB/s |    11,036 |     367.50 |      0 |
|        1MB |          20 |   30.772s | 21.22 MiB/s |       653 |      21.22 |      0 |

### 4.2 资源与连接稳定性

同一轮压测中，服务端基线为 21 个 FD、19,728 KiB RSS、390,804 KiB 虚拟内存、6 个线程。每个场景结束并等待 2 秒后，FD 均回落到 21；RSS 位于 20,372–23,544 KiB，虚拟内存位于 391,068–393,708 KiB，线程数保持 6，所有场景 `closed=0`。该结果说明该默认矩阵下连接释放路径没有观察到 FD 残留或进程异常；它不等同于长时间泄漏证明，长稳测试仍应使用 `SOAK_CASE` 单独执行。
### 4.3 ASan 检查
测试内容:
- heap-use-after-free
- stack-use-after-scope
- global-buffer-overflow
- double-free
- memory leak

结果：
```text
ASan 检查：在 64B × 1000 连接、64KB × 100 连接、1MB × 20 连接等 echo 压测场景下，server 无 ASan 报错、无崩溃、无断言失败
```
### 4.4 Tsan 检查
测试内容：
- data race
- 线程间未同步读写
- 锁使用错误

结果：
```text
Tsan检查：在64B × 1000 连接、64KB × 100 连接、1MB × 20 连接等 echo 压测场景下,EventLoop / TcpConnection 路径无明显 data race
```
### 4.5 fd 泄漏检查
观察命令：
```
pidof echo_server
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
### 4.6 GTest测试
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

## 5. 结果分析

当前 benchmark 主要证明：
1. Reactor 主流程可以正常工作。
2. TcpServer / TcpConnection 生命周期基本正确。  
3. 多线程 EventLoopThreadPool 可以正常分发连接。    
4. Echo Server 在基础并发场景下没有明显崩溃、fd 泄漏、 ASan 报错、TSan报错。   

当前 benchmark 的局限：
1. 测试环境是本地 VM，性能数据不代表真实生产环境。
2. 当前主要关注正确性和稳定性，吞吐数据仅作参考。
3. 尚未系统测试长时间运行、极高并发、慢客户端、半关闭连接等场景。
4. 尚未加入 p50 / p99 latency 统计。
## 6. 后续改进

- 增加自动化 benchmark 脚本
- 增加 p50 / p95 / p99 延迟统计
- 增加长时间稳定性测试
- 增加慢客户端测试
- 增加大消息测试   
- 增加与原生 echo server / muduo echo server 的对比
