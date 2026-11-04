#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="0.1.0"
URL="https://github.com/m-mdy-m/log.c/releases/download/v${VERSION}/m-mdy-m-log-v${VERSION}-source.tar.gz"
EXPECTED_SHA256="cbac350215bb787ad5a82ffde87f29f032c4e0a709568d82748d0ce7c9f71e19"
DEPS_DIR="${ROOT}/.deps"
BUILD_DIR="${ROOT}/build/deps/log.c"
PREFIX="${DEPS_DIR}/log"
ARCHIVE="${DEPS_DIR}/m-mdy-m-log-v${VERSION}-source.tar.gz"
SOURCE_DIR="${DEPS_DIR}/source-log"

command -v curl >/dev/null 2>&1 || { echo "error: curl is required" >&2; exit 1; }
command -v cmake >/dev/null 2>&1 || { echo "error: CMake is required" >&2; exit 1; }
command -v sha256sum >/dev/null 2>&1 || command -v shasum >/dev/null 2>&1 || {
    echo "error: sha256sum or shasum is required" >&2; exit 1;
}
mkdir -p "${DEPS_DIR}"
if [[ ! -f "${ARCHIVE}" ]]; then
    curl --fail --location --retry 3 --retry-delay 1 "${URL}" --output "${ARCHIVE}"
fi
if command -v sha256sum >/dev/null 2>&1; then
    ACTUAL_SHA256="$(sha256sum "${ARCHIVE}" | awk '{print $1}')"
else
    ACTUAL_SHA256="$(shasum -a 256 "${ARCHIVE}" | awk '{print $1}')"
fi
if [[ "${ACTUAL_SHA256,,}" != "${EXPECTED_SHA256}" ]]; then
    echo "error: m-mdy-m/log.c source archive checksum mismatch" >&2
    exit 1
fi
rm -rf "${SOURCE_DIR}"
mkdir -p "${SOURCE_DIR}"
tar -xzf "${ARCHIVE}" -C "${SOURCE_DIR}" --strip-components=1
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}" \
    -DBUILD_SHARED_LIBS=OFF \
    -DM_MDY_M_LOG_BUILD_TESTS=OFF \
    -DM_MDY_M_LOG_BUILD_EXAMPLES=OFF
cmake --build "${BUILD_DIR}" --config Release --parallel
cmake --install "${BUILD_DIR}" --config Release
printf 'Installed m-mdy-m/log.c %s to %s\n' "${VERSION}" "${PREFIX}"
