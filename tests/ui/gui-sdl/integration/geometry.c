#include "geometry.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static int rect_equals(
    const cdin_frontend_gui_sdl_rect *rect,
    uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    return rect->x == x && rect->y == y &&
           rect->width == width && rect->height == height;
}

static void set_leaf(
    cdin_frontend_gui_sdl_layout *layout,
    size_t index, const char *id, uint8_t view_index)
{
    cdin_frontend_gui_sdl_layout_node *node = &layout->nodes[index];
    memset(node, 0, sizeof(*node));
    node->kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF;
    (void)strncpy(node->id, id, sizeof(node->id) - 1u);
    node->first_child = SIZE_MAX;
    node->second_child = SIZE_MAX;
    node->view_indices[0] = view_index;
    node->view_count = 1u;
    node->active_view_index = view_index;
    node->has_active_view = 1;
}

static void make_nested_layout(cdin_frontend_gui_sdl_layout *layout)
{
    memset(layout, 0, sizeof(*layout));
    layout->root_index = 0u;
    layout->active_leaf_index = 1u;
    layout->node_count = 5u;
    layout->leaf_count = 3u;
    layout->view_count = 3u;
    layout->has_root = 1;
    layout->has_active_leaf = 1;

    layout->nodes[0].kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT;
    layout->nodes[0].first_child = 1u;
    layout->nodes[0].second_child = 2u;
    layout->nodes[0].axis = CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL;
    layout->nodes[0].ratio_bp = 2500u;

    set_leaf(layout, 1u, "left", 0u);

    layout->nodes[2].kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT;
    layout->nodes[2].first_child = 3u;
    layout->nodes[2].second_child = 4u;
    layout->nodes[2].axis = CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL;
    layout->nodes[2].ratio_bp = 6000u;

    set_leaf(layout, 3u, "top-right", 1u);
    set_leaf(layout, 4u, "bottom-right", 2u);
}

int main(void)
{
    cdin_frontend_gui_sdl_layout layout;
    cdin_frontend_gui_sdl_rect viewport = { 10u, 20u, 1000u, 600u };
    cdin_frontend_gui_sdl_rect rects[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    cdin_frontend_gui_sdl_rect unchanged[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    cdin_frontend_gui_sdl_geometry_status status;

    make_nested_layout(&layout);
    memset(rects, 0xa5, sizeof(rects));
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK);
    assert(rect_equals(&rects[0], 10u, 20u, 1000u, 600u));
    assert(rect_equals(&rects[1], 10u, 20u, 250u, 600u));
    assert(rect_equals(&rects[2], 260u, 20u, 750u, 600u));
    assert(rect_equals(&rects[3], 260u, 20u, 750u, 360u));
    assert(rect_equals(&rects[4], 260u, 380u, 750u, 240u));

    /* Pixel rounding gives the first child floor(extent * ratio / 10000)
     * pixels and assigns every remaining pixel to the second child. */
    layout.nodes[0].ratio_bp = 3333u;
    viewport.x = 0u;
    viewport.y = 0u;
    viewport.width = 7u;
    viewport.height = 9u;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK);
    assert(rects[1].width == 2u);
    assert(rects[2].width == 5u);
    assert(rects[1].width + rects[2].width == viewport.width);
    assert(rects[1].x == viewport.x);
    assert(rects[2].x + rects[2].width == viewport.x + viewport.width);

    /* Both children remain addressable in the smallest splittable viewport. */
    layout.nodes[0].ratio_bp = 1000u;
    layout.nodes[2].ratio_bp = 8999u;
    viewport.width = 2u;
    viewport.height = 2u;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK);
    assert(rects[1].width == 1u);
    assert(rects[2].width == 1u);
    assert(rects[3].height == 1u);
    assert(rects[4].height == 1u);

    /* An unsplittable one-pixel extent fails without publishing partial data. */
    memcpy(unchanged, rects, sizeof(rects));
    viewport.height = 1u;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_LIMIT);
    assert(memcmp(rects, unchanged, sizeof(rects)) == 0);

    /* A short output buffer and malformed topology are also failure-atomic. */
    viewport.height = 2u;
    memcpy(unchanged, rects, sizeof(rects));
    status = cdin_frontend_gui_sdl_geometry_compute(&layout, &viewport, rects, 4u);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_BUFFER_TOO_SMALL);
    assert(memcmp(rects, unchanged, sizeof(rects)) == 0);

    layout.nodes[2].second_child = 99u;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID);
    assert(memcmp(rects, unchanged, sizeof(rects)) == 0);

    make_nested_layout(&layout);
    layout.nodes[4].first_child = 3u; /* A leaf cannot own children. */
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID);

    make_nested_layout(&layout);
    layout.nodes[2].first_child = 0u; /* A cycle must not recurse forever. */
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID);

    /* An empty layout has no output rectangles and is a valid state. */
    memset(&layout, 0, sizeof(layout));
    layout.root_index = SIZE_MAX;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, NULL, 0u);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK);

    /* Every node must be reachable from the declared root. */
    make_nested_layout(&layout);
    layout.node_count = 6u;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID);

    /* Pixel origin plus extent must not overflow the coordinate domain. */
    make_nested_layout(&layout);
    viewport.x = UINT32_MAX;
    viewport.width = 1u;
    status = cdin_frontend_gui_sdl_geometry_compute(
        &layout, &viewport, rects, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    assert(status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID);

    return 0;
}
