#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"
cd "$repo_root"

fail=0
scan_roots=(include core ui examples)
forbidden='(^|[<"])(cdin/(kernel\.h|backend\.h|protocol\.h|backend_protocol\.h)|runtime/.*\.lua|lua\.h)|cdin::(backend|protocol)|cdin_backend|cdin-kernel|cdin_kernel_log'
if grep -R -n -E "$forbidden" "${scan_roots[@]}" CMakeLists.txt Makefile 2>/dev/null; then
    echo "Forbidden Backend/Kernel/Runtime implementation dependency found in Frontend" >&2
    fail=1
fi

# Core must be environment-neutral: no terminal, SDL, window or device APIs.
if grep -R -n -E '(<SDL[0-9]*/|SDL_[A-Za-z0-9_]+|termios\.h|windows\.h|ioctl\s*\(|tcsetattr\s*\(|ReadConsoleInput|WriteConsole)' core include/cdin/frontend/core 2>/dev/null; then
    echo "Platform/UI implementation leaked into Frontend Core" >&2
    fail=1
fi

# Diagnostics are emitted through the standalone logger facade. Only the
# logger adapter owns the direct m-mdy-m/log.h include; UI renderers may write
# terminal frames through the terminal adapter, not printf-style diagnostics.
if grep -R -n -E '\b(printf|fprintf|puts|perror)\s*\(' core ui/tui/src --exclude='log.c' --exclude='tui.c'; then
    echo "Direct diagnostic output found outside the logger/terminal renderer" >&2
    fail=1
fi
if grep -R -n -E '#include[[:space:]]*[<"]m-mdy-m/log\.h' core ui include/cdin/frontend --exclude='log.h'; then
    echo "Diagnostics adapter must be the only direct logger include" >&2
    fail=1
fi
if ! grep -q 'm-mdy-m::log' CMakeLists.txt; then
    echo "CMake does not use the standalone m-mdy-m/log.c dependency" >&2
    fail=1
fi

# Core may not depend on any UI public header; UI adapters depend on Core.
if grep -R -n -E '#include[[:space:]]*[<"]cdin/frontend/ui/' core include/cdin/frontend/core 2>/dev/null; then
    echo "Frontend Core includes a UI-specific public header" >&2
    fail=1
fi
if grep -n -E 'target_link_libraries\(cdin_frontend_core[^)]*(frontend_ui_tui|frontend_ui_gui_sdl|frontend_ui_headless)' CMakeLists.txt; then
    echo "Frontend Core must not link a UI adapter target" >&2
    fail=1
fi
if grep -R -n -E 'SDL_[A-Za-z0-9_]+|#include[[:space:]]*[<"]SDL[0-9]*/' ui/tui/include ui/tui/src 2>/dev/null; then
    echo "TUI must not depend on the SDL GUI toolkit" >&2
    fail=1
fi


# SDL headers and symbols belong exclusively to the GUI-SDL peer. This keeps
# both Core and the terminal/Headless adapters toolkit-independent (FE-012/FE-020).
if grep -R -n -E '(<SDL[0-9]*/|SDL_[A-Za-z0-9_]+)' include core ui/tui ui/headless apps 2>/dev/null; then
    echo "SDL dependency leaked outside the GUI-SDL peer" >&2
    fail=1
fi

# Headless is a Protocol-only peer: it must not pull in any display or terminal
# platform and must not depend on another UI adapter.
if grep -R -n -E '(<SDL[0-9]*/|SDL_[A-Za-z0-9_]+|termios\.h|windows\.h|ioctl\s*\(|tcsetattr\s*\(|ReadConsoleInput|WriteConsole)' ui/headless 2>/dev/null; then
    echo "Headless must remain display- and terminal-independent" >&2
    fail=1
fi
if grep -R -n -E 'cdin_frontend_ui_(tui|gui_sdl)|cdin::frontend_ui_(tui|gui_sdl)|\b(printf|fprintf|puts|perror)\s*\(' ui/headless 2>/dev/null; then
    echo "Headless must not depend on peer adapters or write ad-hoc diagnostics" >&2
    fail=1
fi
if grep -n -E 'target_link_libraries\(cdin_frontend_ui_headless[^)]*(frontend_ui_tui|frontend_ui_gui_sdl)' CMakeLists.txt; then
    echo "Headless must link Core only" >&2
    fail=1
fi

# UI implementations are peers: TUI and GUI must depend on Core, never on one another.
if grep -R -n -E 'cdin_frontend_ui_(tui|gui_sdl)|cdin::frontend_ui_(tui|gui_sdl)' ui/tui/include ui/tui/src 2>/dev/null; then
    echo "A UI implementation depends on another UI implementation" >&2
    fail=1
fi
if grep -R -n -E 'cdin::frontend_ui_tui|cdin_frontend_ui_tui' ui/gui-sdl 2>/dev/null; then
    echo "GUI SDL must not depend on TUI" >&2
    fail=1
fi

if [[ "$fail" -ne 0 ]]; then exit 1; fi
echo "PASS Core/UI boundaries, peer frontends and diagnostics dependency"
