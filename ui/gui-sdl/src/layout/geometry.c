#include "geometry.h"

#include <string.h>

#define CDIN_FRONTEND_GUI_SDL_GEOMETRY_MAX_DEPTH 32u

typedef struct cdin_frontend_gui_sdl_geometry_work {
    const cdin_frontend_gui_sdl_layout *layout;
    cdin_frontend_gui_sdl_rect *rects;
    uint8_t visited[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    uint8_t view_seen[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS];
    size_t visited_count;
    size_t leaf_count;
    size_t view_count;
} cdin_frontend_gui_sdl_geometry_work;

static cdin_frontend_gui_sdl_geometry_status compute_node(
    cdin_frontend_gui_sdl_geometry_work *work,
    size_t node_index,
    const cdin_frontend_gui_sdl_rect *rect,
    size_t depth)
{
    const cdin_frontend_gui_sdl_layout_node *node;
    cdin_frontend_gui_sdl_rect first_rect;
    cdin_frontend_gui_sdl_rect second_rect;
    uint64_t first_extent;
    uint32_t extent;
    size_t index;

    if (work == NULL || work->layout == NULL || work->rects == NULL ||
        rect == NULL || node_index >= work->layout->node_count) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    if (depth > CDIN_FRONTEND_GUI_SDL_GEOMETRY_MAX_DEPTH) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_LIMIT;
    }
    if (work->visited[node_index] != 0u) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    work->visited[node_index] = 1u;
    work->visited_count++;
    work->rects[node_index] = *rect;
    node = &work->layout->nodes[node_index];

    if (node->kind == CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF) {
        if (node->first_child != SIZE_MAX ||
            node->second_child != SIZE_MAX ||
            node->view_count > CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS) {
            return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
        }
        for (index = 0u; index < node->view_count; ++index) {
            uint8_t view_index = node->view_indices[index];
            if ((size_t)view_index >= work->layout->view_count ||
                work->view_seen[view_index] != 0u) {
                return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
            }
            work->view_seen[view_index] = 1u;
            work->view_count++;
        }
        if (node->has_active_view != 0) {
            int found = 0;
            if ((size_t)node->active_view_index >= work->layout->view_count) {
                return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
            }
            for (index = 0u; index < node->view_count; ++index) {
                if (node->view_indices[index] == node->active_view_index) {
                    found = 1;
                    break;
                }
            }
            if (found == 0) return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
        }
        work->leaf_count++;
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK;
    }

    if (node->kind != CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT ||
        node->view_count != 0u || node->has_active_view != 0 ||
        node->first_child >= work->layout->node_count ||
        node->second_child >= work->layout->node_count ||
        node->first_child == node->second_child ||
        node->ratio_bp < 1000u || node->ratio_bp > 8999u) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }

    first_rect = *rect;
    second_rect = *rect;
    if (node->axis == CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL) {
        extent = rect->width;
    } else if (node->axis == CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL) {
        extent = rect->height;
    } else {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    if (extent < 2u) return CDIN_FRONTEND_GUI_SDL_GEOMETRY_LIMIT;

    first_extent = ((uint64_t)extent * (uint64_t)node->ratio_bp) / 10000u;
    if (first_extent == 0u) first_extent = 1u;
    if (first_extent >= (uint64_t)extent) first_extent = (uint64_t)extent - 1u;

    if (node->axis == CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL) {
        uint32_t first_width = (uint32_t)first_extent;
        first_rect.width = first_width;
        second_rect.x = rect->x + first_width;
        second_rect.width = rect->width - first_width;
    } else {
        uint32_t first_height = (uint32_t)first_extent;
        first_rect.height = first_height;
        second_rect.y = rect->y + first_height;
        second_rect.height = rect->height - first_height;
    }

    {
        cdin_frontend_gui_sdl_geometry_status status = compute_node(
            work, node->first_child, &first_rect, depth + 1u);
        if (status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK) return status;
        return compute_node(work, node->second_child, &second_rect, depth + 1u);
    }
}

cdin_frontend_gui_sdl_geometry_status cdin_frontend_gui_sdl_geometry_compute(
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_rect *out_node_rects,
    size_t out_capacity)
{
    cdin_frontend_gui_sdl_geometry_work work;
    cdin_frontend_gui_sdl_rect computed[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    cdin_frontend_gui_sdl_geometry_status status;

    if (layout == NULL || viewport == NULL ||
        layout->node_count > CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES ||
        layout->view_count > CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    if (layout->has_root == 0) {
        if (layout->node_count != 0u || layout->leaf_count != 0u ||
            layout->view_count != 0u || layout->has_active_leaf != 0 ||
            layout->root_index != SIZE_MAX) {
            return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
        }
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK;
    }
    if (layout->node_count == 0u || layout->root_index >= layout->node_count ||
        layout->has_active_leaf == 0 ||
        layout->active_leaf_index >= layout->node_count ||
        viewport->width == 0u || viewport->height == 0u ||
        viewport->x > UINT32_MAX - viewport->width ||
        viewport->y > UINT32_MAX - viewport->height) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    if (out_node_rects == NULL) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    if (out_capacity < layout->node_count) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_BUFFER_TOO_SMALL;
    }
    if (layout->nodes[layout->active_leaf_index].kind !=
        CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }

    memset(&work, 0, sizeof(work));
    work.layout = layout;
    work.rects = computed;
    status = compute_node(&work, layout->root_index, viewport, 1u);
    if (status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK) return status;
    if (work.visited_count != layout->node_count ||
        work.leaf_count != layout->leaf_count ||
        work.view_count != layout->view_count) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    memcpy(out_node_rects, computed,
           layout->node_count * sizeof(out_node_rects[0]));
    return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK;
}


static void collect_node_depth(
    const cdin_frontend_gui_sdl_layout *layout,
    size_t node_index,
    size_t depth,
    size_t *depths)
{
    const cdin_frontend_gui_sdl_layout_node *node;
    if (layout == NULL || depths == NULL || node_index >= layout->node_count) {
        return;
    }
    depths[node_index] = depth;
    node = &layout->nodes[node_index];
    if (node->kind == CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT) {
        collect_node_depth(layout, node->first_child, depth + 1u, depths);
        collect_node_depth(layout, node->second_child, depth + 1u, depths);
    }
}

cdin_frontend_gui_sdl_geometry_status cdin_frontend_gui_sdl_geometry_splitter_hit_test(
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    uint32_t x,
    uint32_t y,
    uint32_t hit_radius,
    size_t *out_split_node_index)
{
    cdin_frontend_gui_sdl_rect rectangles[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    size_t depths[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES] = {0u};
    cdin_frontend_gui_sdl_geometry_status status;
    size_t best_index = SIZE_MAX;
    size_t best_depth = 0u;
    uint32_t best_distance = UINT32_MAX;
    size_t index;

    if (out_split_node_index != NULL) *out_split_node_index = SIZE_MAX;
    if (layout == NULL || viewport == NULL || out_split_node_index == NULL ||
        viewport->width == 0u || viewport->height == 0u ||
        viewport->x > UINT32_MAX - viewport->width ||
        viewport->y > UINT32_MAX - viewport->height) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    if (x < viewport->x || y < viewport->y ||
        x >= viewport->x + viewport->width ||
        y >= viewport->y + viewport->height) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OUTSIDE;
    }

    status = cdin_frontend_gui_sdl_geometry_compute(
        layout, viewport, rectangles, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    if (status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK) return status;
    if (layout->has_root == 0) return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OUTSIDE;
    collect_node_depth(layout, layout->root_index, 0u, depths);

    for (index = 0u; index < layout->node_count; ++index) {
        const cdin_frontend_gui_sdl_layout_node *node = &layout->nodes[index];
        const cdin_frontend_gui_sdl_rect *rect = &rectangles[index];
        const cdin_frontend_gui_sdl_rect *first_rect;
        uint32_t boundary;
        uint32_t distance;
        int within_cross_axis;

        if (node->kind != CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT) continue;
        first_rect = &rectangles[node->first_child];
        if (node->axis == CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL) {
            boundary = first_rect->x + first_rect->width;
            distance = x >= boundary ? x - boundary : boundary - x;
            within_cross_axis = y >= rect->y && y < rect->y + rect->height;
        } else {
            boundary = first_rect->y + first_rect->height;
            distance = y >= boundary ? y - boundary : boundary - y;
            within_cross_axis = x >= rect->x && x < rect->x + rect->width;
        }
        if (!within_cross_axis || distance > hit_radius) continue;
        if (best_index == SIZE_MAX || distance < best_distance ||
            (distance == best_distance && depths[index] > best_depth)) {
            best_index = index;
            best_distance = distance;
            best_depth = depths[index];
        }
    }

    if (best_index == SIZE_MAX) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OUTSIDE;
    }
    *out_split_node_index = best_index;
    return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK;
}

cdin_frontend_gui_sdl_geometry_status cdin_frontend_gui_sdl_geometry_hit_test(
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    uint32_t x,
    uint32_t y,
    size_t *out_leaf_node_index,
    size_t *out_active_view_index)
{
    cdin_frontend_gui_sdl_rect rectangles[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    cdin_frontend_gui_sdl_geometry_status status;
    size_t index;

    if (out_leaf_node_index != NULL) *out_leaf_node_index = SIZE_MAX;
    if (out_active_view_index != NULL) *out_active_view_index = SIZE_MAX;
    if (layout == NULL || viewport == NULL ||
        out_leaf_node_index == NULL || out_active_view_index == NULL) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_INVALID;
    }
    status = cdin_frontend_gui_sdl_geometry_compute(
        layout, viewport, rectangles, CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    if (status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK) return status;
    if (layout->has_root == 0 ||
        x < viewport->x || y < viewport->y ||
        x >= viewport->x + viewport->width ||
        y >= viewport->y + viewport->height) {
        return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OUTSIDE;
    }

    for (index = 0u; index < layout->node_count; ++index) {
        const cdin_frontend_gui_sdl_layout_node *node = &layout->nodes[index];
        const cdin_frontend_gui_sdl_rect *rect = &rectangles[index];
        if (node->kind != CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF) continue;
        if (x >= rect->x && y >= rect->y &&
            x < rect->x + rect->width &&
            y < rect->y + rect->height) {
            *out_leaf_node_index = index;
            if (node->has_active_view != 0) {
                *out_active_view_index = (size_t)node->active_view_index;
            }
            return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK;
        }
    }
    return CDIN_FRONTEND_GUI_SDL_GEOMETRY_OUTSIDE;
}
