#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_ROOT}/build-release}"
BUILD_ARGUMENT="${2:-$(realpath --relative-to="${PROJECT_ROOT}" "${BUILD_DIR}")}"
OUTPUT_DIR="$(mktemp -d /tmp/mini-muduo-bench-test-XXXXXX)"

cleanup() {
    rm -rf "${OUTPUT_DIR}"
}
trap cleanup EXIT

output="$(
    cd "${PROJECT_ROOT}"
    timeout 12 env \
        PORT=19990 \
        THREADS=1 \
        BENCH_CASES="1,1,1" \
        SOAK_CASE="" \
        BENCH_LOG_DIR="${OUTPUT_DIR}" \
        "${PROJECT_ROOT}/scripts/run_echo_bench.sh" "${BUILD_ARGUMENT}" 2>&1
)"

printf '%s\n' "${output}"
grep -q 'case: block_size=1, connections=1, duration=1s' <<<"${output}"
grep -q 'metrics phase=baseline' <<<"${output}"
grep -q 'metrics phase=after_case' <<<"${output}"
grep -q "Benchmark artifacts: ${OUTPUT_DIR}" <<<"${output}"
