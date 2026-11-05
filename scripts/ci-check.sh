#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$repo_root"

bash scripts/install-deps.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${repo_root}/.deps/log" \
    -DCDIN_FRONTEND_USE_LOG_STUB=OFF \
    -DCDIN_FRONTEND_BUILD_UI_TUI=ON \
    -DCDIN_FRONTEND_BUILD_UI_SDL=OFF \
    -DCDIN_FRONTEND_BUILD_UI_HEADLESS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure

# FE-010 / FE-019: verify the installed public package from a pristine prefix.
# This consumer is intentionally outside the Frontend source tree and links
# only exported targets; CDIN must consume the same interface without vendoring.
install_prefix="${repo_root}/build/install-prefix"
rm -rf "${install_prefix}" "${repo_root}/build/package-consumer"
cmake --install build --prefix "${install_prefix}"
cmake -S tests/package-consumer -B build/package-consumer \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${install_prefix};${repo_root}/.deps/log"
cmake --build build/package-consumer --config Release --parallel
ctest --test-dir build/package-consumer -C Release --output-on-failure

bash scripts/check-boundaries.sh
bash scripts/lint-docs.sh
