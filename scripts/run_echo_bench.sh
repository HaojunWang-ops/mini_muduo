#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${1:-${PROJECT_ROOT}/build-release}"
PORT="${PORT:-9981}"
THREADS="${THREADS:-4}"
HOST="${HOST:-127.0.0.1}"

SERVER="${BUILD_DIR}/bin/echo_server"
CLIENT="${BUILD_DIR}/bin/ping_pong"

if [[ ! -x "${SERVER}" ]]; then
    echo "error: echo_server not found or not executable: ${SERVER}"
    exit 1
fi

if [[ ! -x "${CLIENT}" ]]; then
    echo "error: ping_pong not found or not executable: ${CLIENT}"
    exit 1
fi

echo "Starting echo_server..."
echo "server: ${SERVER}"
echo "client: ${CLIENT}"
echo "port: ${PORT}"
echo "threads: ${THREADS}"
echo

#后台启动echo_server
#./build-release/bin/echo_server 9981 4
#& 表示让服务端在后台运行
"${SERVER}" "${PORT}" "${THREADS}" &

#保存上一个后台进程的pid
SERVER_PID=$!

cleanup() {
    echo
    echo "Stopping echo_server, pid=${SERVER_PID}"
    kill "${SERVER_PID}" 2>/dev/null || true
    wait "${SERVER_PID}" 2>/dev/null || true
}

trap cleanup EXIT

sleep 1

run_case() {
    local block_size="$1"
    local connections="$2"
    local duration="$3"

    echo "============================================================"
    echo "case: block_size=${block_size}, connections=${connections}, duration=${duration}s"
    echo "============================================================"

    "${CLIENT}" "${HOST}" "${PORT}" "${block_size}" "${connections}" "${duration}"
    echo
}

run_case 64      100   10
run_case 64      1000  30
run_case 1024    1000  30
run_case 65536   100   30
run_case 1048576 20    30

echo "Benchmark finished."