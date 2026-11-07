#ifndef CDIN_FRONTEND_GUI_SDL_GEOMETRY_H
#define CDIN_FRONTEND_GUI_SDL_GEOMETRY_H

#include "../session/layout.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cdin_frontend_gui_sdl_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} cdin_frontend_gui_sdl_rect;

typedef enum cdin_frontend_gui_sdl_geometry_status {
    CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK = 0,
    CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID,
    CDIN_FRONTEND_GUI_SDL_GEOMETRY_LIMIT,
    CDIN_FRONTEND_GUI_SDL_GEOMETRY_BUFFER_TOO_SMALL
} cdin_frontend_gui_sdl_geometry_status;

/*
 * Computes pixel rectangles for every node in a validated PaneTree.
 *
 * The output is indexed by layout node index, including both split and leaf
 * nodes. Horizontal splits divide the X/width axis (first child on the left);
 * vertical splits divide the Y/height axis (first child on top). The first
 * child receives floor(extent * ratio_bp / 10000) pixels, clamped so both
 * children retain at least one pixel. The second child receives the exact
 * remainder.
 *
 * The function is allocation-free and independent of SDL. The caller supplies
 * storage for layout->node_count rectangles. On every non-OK result the output
 * array remains unchanged. An empty PaneTree succeeds with node_count == 0.
 */
cdin_frontend_gui_sdl_geometry_status cdin_frontend_gui_sdl_geometry_compute(
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_rect *out_node_rects,
    size_t out_capacity);

#ifdef __cplusplus
}
#endif

#endif
