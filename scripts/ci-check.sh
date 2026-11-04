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
bash scripts/check-boundaries.sh
bash scripts/lint-docs.sh
