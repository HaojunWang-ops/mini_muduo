#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${PROJECT_ROOT}/build-release"

cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release  \
    -DBUILD_TESTING=OFF

cmake --build "${BUILD_DIR}" -j

echo "Release build finished: ${BUILD_DIR}"