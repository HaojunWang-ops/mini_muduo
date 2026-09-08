# Repository Guidelines

## Project Structure & Module Organization

`base/` contains reusable primitives: threading, synchronization, timestamps, file/logging utilities, and async logging. `net/` implements the Reactor network stack, including `EventLoop`, `Channel`, poller backends, timers, sockets, and TCP server/connection classes. Keep alternative polling implementations in `net/poller/`.

`examples/echo.cpp` is the reference echo server. `tests/` contains GoogleTest unit tests, `scripts/` has build and benchmark helpers, and `docs/` records design and performance notes. Add implementation files alongside their public headers and register new `.cc` files in the appropriate `CMakeLists.txt`.

## Build, Test, and Development Commands

- `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug -j` configures and builds the library, example, tools, and tests.
- `ctest --test-dir build-debug --output-on-failure` runs discovered GoogleTest cases.
- `./build-debug/bin/echo_server 9981 4` runs the example server on port 9981 with four I/O threads.
- `./scripts/asan_build.sh` or `./scripts/tsan_build.sh` produces AddressSanitizer or ThreadSanitizer builds. Do not enable both sanitizer options together.

GTest must be available to configure tests (`find_package(GTest REQUIRED)`). Use `-DBUILD_TESTING=OFF` for a release-only build.

## Coding Style & Naming Conventions

Target C++17; preserve the existing CMake warnings (`-Wall -Wextra -Wpedantic`). Follow nearby code for formatting: braces on their own line, small focused methods, and standard-library headers after project headers. Use `PascalCase` for classes (`TcpConnection`), `camelCase` for methods/functions (`queueInLoop`), and trailing underscores for data members (`outputBuffer_`). Keep code in the `reactor` and `reactor::net` namespaces. Avoid copying concurrency primitives; use the supplied `noncopyable` base where ownership requires it.

## Testing Guidelines

Write focused GoogleTest cases in `tests/<Component>_test.cc`, using suites such as `TEST(BufferTest, AppendLargeData)`. Cover normal behavior and boundary/error cases, especially buffer limits, event-loop thread affinity, and connection state transitions. Add the file to `tests/CMakeLists.txt`, rebuild, then run CTest. Run ASan for memory-sensitive changes and TSan for threaded/event-loop changes.

## Commit & Pull Request Guidelines

Recent history uses short imperative summaries such as `fixed a bug`, `update echo.cpp`, and `add epoll part`. Prefer a concise, scoped imperative subject (for example, `net: fix partial-write handling`). Keep commits narrowly focused. Pull requests should describe the behavior change, list validation commands/results, link relevant issues when present, and include logs or benchmark output for performance or networking behavior changes.
