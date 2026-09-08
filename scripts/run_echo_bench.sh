#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${1:-${PROJECT_ROOT}/build-release}"
if [[ ! -d "${BUILD_DIR}" ]]; then
    echo "error: build directory does not exist: ${BUILD_DIR}" >&2
    exit 1
fi
BUILD_DIR="$(cd "${BUILD_DIR}" && pwd -P)"
PORT="${PORT:-9981}"
THREADS="${THREADS:-4}"
HOST="${HOST:-127.0.0.1}"
SERVER_STARTUP_SECONDS="${SERVER_STARTUP_SECONDS:-1}"
FD_SETTLE_SECONDS="${FD_SETTLE_SECONDS:-2}"
BENCH_CASES="${BENCH_CASES:-64,100,10 64,1000,30 1024,1000,30 65536,100,30 1048576,20,30}"
SOAK_CASE="${SOAK_CASE:-}"

SERVER="${BUILD_DIR}/bin/echo_server"
CLIENT="${BUILD_DIR}/bin/ping_pong"
BENCH_LOG_DIR="${BENCH_LOG_DIR:-$(mktemp -d /tmp/mini-muduo-bench-XXXXXX)}"
RESULT_FILE="${BENCH_LOG_DIR}/benchmark-results.txt"

if [[ ! -x "${SERVER}" ]]; then
    echo "error: echo_server not found or not executable: ${SERVER}" >&2
    exit 1
fi

if [[ ! -x "${CLIENT}" ]]; then
    echo "error: ping_pong not found or not executable: ${CLIENT}" >&2
    exit 1
fi

mkdir -p "${BENCH_LOG_DIR}"
exec > >(tee "${RESULT_FILE}") 2>&1

metric_value() {
    local field="$1"
    awk -v field="${field}" '$1 == field ":" { print $2 }' "/proc/${SERVER_PID}/status"
}

fd_count() {
    local -a fds=("/proc/${SERVER_PID}/fd/"*)
    printf '%s\n' "${#fds[@]}"
}

print_metrics() {
    local phase="$1"
    local fds
    fds="$(fd_count)"
    printf 'metrics phase=%s fd_count=%s rss_kib=%s vm_size_kib=%s threads=%s cpu_percent=%s\n' \
        "${phase}" \
        "${fds}" \
        "$(metric_value VmRSS)" \
        "$(metric_value VmSize)" \
        "$(metric_value Threads)" \
        "$(ps -o pcpu= -p "${SERVER_PID}" | tr -d ' ')"
}

run_case() {
    local spec="$1"
    local label="$2"
    local block_size connections duration extra=""

    IFS=, read -r block_size connections duration extra <<<"${spec}"
    if [[ -n "${extra}" || ! "${block_size}" =~ ^[1-9][0-9]*$ || ! "${connections}" =~ ^[1-9][0-9]*$ || ! "${duration}" =~ ^[1-9][0-9]*$ ]]; then
        echo "error: invalid ${label} case '${spec}'; expected block_size,connections,duration" >&2
        return 1
    fi

    echo "============================================================"
    echo "${label}: block_size=${block_size}, connections=${connections}, duration=${duration}s"
    echo "============================================================"
    "${CLIENT}" "${HOST}" "${PORT}" "${block_size}" "${connections}" "${duration}"
    sleep "${FD_SETTLE_SECONDS}"
    print_metrics "after_${label}"
    echo
}

cleanup() {
    if [[ -n "${SERVER_PID:-}" ]]; then
        echo "Stopping echo_server, pid=${SERVER_PID}"
        kill "${SERVER_PID}" 2>/dev/null || true
        wait "${SERVER_PID}" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "Starting echo_server..."
echo "server: ${SERVER}"
echo "client: ${CLIENT}"
echo "host: ${HOST}"
echo "port: ${PORT}"
echo "threads: ${THREADS}"
echo "Benchmark artifacts: ${BENCH_LOG_DIR}"
echo

(
    cd "${BENCH_LOG_DIR}"
    exec "${SERVER}" "${PORT}" "${THREADS}"
) >"${BENCH_LOG_DIR}/server.log" 2>&1 &
SERVER_PID=$!

sleep "${SERVER_STARTUP_SECONDS}"
if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
    echo "error: echo_server exited during startup; see ${BENCH_LOG_DIR}/server.log" >&2
    exit 1
fi

print_metrics baseline

for case_spec in ${BENCH_CASES}; do
    run_case "${case_spec}" case
done

if [[ -n "${SOAK_CASE}" ]]; then
    run_case "${SOAK_CASE}" soak
fi

print_metrics final
echo "Benchmark finished. Results: ${RESULT_FILE}"
