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
    CDIN_FRONTEND_GUI_SDL_GEOMETRY_BUFFER_TOO_SMALL,
    CDIN_FRONTEND_GUI_SDL_GEOMETRY_OUTSIDE
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

/*
 * Hit-test the splitter divider bands in pixel coordinates. hit_radius is
 * the inclusive distance from the mathematical divider line (4 gives a
 * 9-pixel target). At nested divider intersections the deepest split wins;
 * equal-distance candidates otherwise prefer the deepest split. The result
 * is an index into layout->nodes; callers resolve its stable semantic ID via
 * layout->nodes[index].id. It is never a Runtime view index. OUTSIDE resets output.
 */
cdin_frontend_gui_sdl_geometry_status cdin_frontend_gui_sdl_geometry_splitter_hit_test(
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    uint32_t x,
    uint32_t y,
    uint32_t hit_radius,
    size_t *out_split_node_index);

/*
 * Hit-test a point in pixel coordinates against the computed PaneTree.
 * Split rectangles are half-open: a point on a split boundary belongs to the
 * second child. On success, out_leaf_node_index identifies the leaf and
 * out_active_view_index identifies that leaf's active Runtime view; the latter
 * is SIZE_MAX when the leaf has no active view. OUTSIDE means the point is not
 * inside any leaf or the layout is empty. Outputs are reset to SIZE_MAX on
 * every non-OK result.
 */
cdin_frontend_gui_sdl_geometry_status cdin_frontend_gui_sdl_geometry_hit_test(
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    uint32_t x,
    uint32_t y,
    size_t *out_leaf_node_index,
    size_t *out_active_view_index);

#ifdef __cplusplus
}
#endif

#endif
