# Core input primitives (planned)

This module will hold toolkit-neutral normalized Frontend input event helpers.
It must not decode terminal escape sequences or SDL events; those translations
belong to `ui/tui/input` and `ui/gui-sdl/input` respectively.
