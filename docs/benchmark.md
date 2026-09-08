# Benchmark

本文只保留可复现的 Release 性能与连接稳定性数据。结果用于同一环境、同一构建配置下的回归比较，不代表生产环境吞吐上限。

## 测试环境

| 项目 | 配置 |
| --- | --- |
| 日期 | 2026-09-08 |
| OS | Ubuntu Linux VM，kernel 7.0.0-31-generic |
| CPU / Memory | 13th Gen Intel i7-13650HX；VM 可见 2 个逻辑 CPU、4 GiB 内存 |
| 编译器 | g++ 13.3.0 |
| 构建 | `Release`，C++17 |
| 网络 | localhost / loopback |
| 服务端 | `echo_server 9981 4`，4 个 I/O 线程 |
| 客户端 | 单线程 `epoll` ping-pong client |

## 运行方式

构建并执行默认矩阵：

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-release -j
./scripts/run_echo_bench.sh ./build-release
```

脚本为每个场景保存 `benchmark-results.txt` 和 `server.log`，并在基线、每个场景后和结束时记录服务端 FD、RSS、虚拟内存、线程数及平均 CPU 使用率。

可通过 `BENCH_CASES` 覆盖默认矩阵；格式为 `block_size,connections,duration_seconds`：

```bash
BENCH_CASES="64,1000,30 65536,100,30" \
./scripts/run_echo_bench.sh ./build-release
```

长时间稳定性测试使用可选的 `SOAK_CASE`，建议单独运行并保存产物：

```bash
SOAK_CASE="64,1000,600" \
BENCH_LOG_DIR=/tmp/mini-muduo-soak \
./scripts/run_echo_bench.sh ./build-release
```

## 结果

 使用上述默认矩阵得到以下结果。每个客户端连接在收到完整 echo block 后发送下一块数据；`closed` 是客户端观察到的关闭连接数。

| block size | connections | duration | throughput | messages | messages/s | closed |
| ---------: | ----------: | -------: | ---------: | --------: | ---------: | -----: |
|        64B |         100 |   10.001s |  1.91 MiB/s |   313,360 |  31,333.96 |      0 |
|        64B |        1000 |   30.004s |  1.92 MiB/s |   941,644 |  31,384.30 |      0 |
|        1KB |        1000 |   30.023s | 13.40 MiB/s |   411,974 |  13,721.76 |      0 |
|       64KB |         100 |   30.030s | 22.97 MiB/s |    11,036 |     367.50 |      0 |
|        1MB |          20 |   30.772s | 21.22 MiB/s |       653 |      21.22 |      0 |

服务端基线为 21 个 FD、19,728 KiB RSS、390,804 KiB 虚拟内存和 6 个线程。各场景结束并等待 2 秒后，FD 均回落至 21，RSS 为 20,372–23,544 KiB，虚拟内存为 391,068–393,708 KiB，线程数保持 6。默认矩阵中没有观察到断连、FD 残留或服务端异常。

## 10,000 并发连接压力场景

同日在相同 Release、loopback 和 4 I/O 线程配置下，执行 `64B × 10,000` 长连接 ping-pong。客户端两轮均成功创建 10,000 条连接，且 `closed=0`。

| block size | connections | duration | throughput | messages | messages/s | closed |
| ---------: | ----------: | -------: | ---------: | --------: | ---------: | -----: |
|        64B |       10000 |   30.067s | 1.30 MiB/s |   640,000 |  21,285.64 |      0 |
|        64B |       10000 |   60.071s | 1.39 MiB/s | 1,367,639 |  22,767.04 |      0 |

该轮服务端基线为 21 个 FD、19,732 KiB RSS、390,804 KiB 虚拟内存和 6 个线程。每个场景结束、客户端关闭连接并等待 2 秒后，FD 均回落至 21；最终 RSS 为 49,584 KiB、虚拟内存为 419,316 KiB、线程数保持 6。该采样证明了本轮结束后的 FD 回收，不替代峰值 FD、长期内存增长或更高并发下的专项观测。

## 本机吞吐配置扫测

为寻找当前 VM 上的单次观测最佳吞吐，固定 `64KB × 100 连接 × 30s`，仅调整服务端 I/O 线程数。每组均 `closed=0`，且结束后 FD 回到各自基线。

| I/O threads | duration | throughput | messages/s | FD baseline → after case |
| ----------: | -------: | ---------: | ---------: | -----------------------: |
|           1 |   30.007s | 19.45 MiB/s |     311.16 |                  12 → 12 |
|           2 |   30.198s | 19.63 MiB/s |     314.06 |                  15 → 15 |
|           4 |   30.023s | **20.06 MiB/s** | **320.89** |                  21 → 21 |

本轮最佳为 4 个 I/O 线程，吞吐 20.06 MiB/s；但它仅比 2 线程高约 2.2%。由于每个配置只运行一次，且客户端、服务端和异步日志共享这台 2 逻辑 CPU VM，差异可能包含运行噪声。若要将该参数作为固定基线，应至少重复 3 次并记录均值、标准差和分位延迟。

## 解读与边界

- 64B 场景主要受每消息的事件、回调和日志开销影响，消息率约为 31k/s。
- 10,000 连接下，64B 消息率约为 21k–23k/s；吞吐低于默认 1,000 连接场景，反映了更高并发下的事件调度与日志开销。
- 在此本机环境中，64KB 场景达到本轮最高吞吐 22.97 MiB/s；1MB 场景为 21.22 MiB/s。
- 当前数据不包含 p50/p99 延迟、慢客户端、半关闭、长时间 soak、跨主机网络或极高连接数的结论。
- ASan、TSan 与单元测试应作为独立验证运行；本文件不将旧轮次的 sanitizer 结果与本次 Release 数据混合记录。
