#include "render.h"

#include "../layout/geometry.h"

#include <stdio.h>
#include <string.h>

static void draw_text_ascii(
    SDL_Renderer *renderer,
    float x,
    float y,
    const uint8_t *bytes,
    size_t length,
    size_t capacity)
{
    char text[512];
    size_t amount = length < capacity ? length : capacity;
    size_t index;
    if (amount >= sizeof(text)) amount = sizeof(text) - 1u;
    for (index = 0u; index < amount; ++index) {
        uint8_t value = bytes[index];
        text[index] = value >= 0x20u && value < 0x7fu ? (char)value : '?';
    }
    text[amount] = '\0';
    (void)SDL_RenderDebugText(renderer, x, y, text);
}

static cdin_frontend_gui_sdl_render_status draw_view_pane(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_view *view,
    const char *view_id,
    const cdin_frontend_gui_sdl_rect *rect,
    int focused,
    cdin_frontend_gui_sdl_render_stats *stats)
{
    SDL_FRect background;
    SDL_FRect border;
    SDL_Rect clip;
    uint8_t background_r = focused != 0 ? 31u : 25u;
    uint8_t background_g = focused != 0 ? 36u : 30u;
    uint8_t background_b = focused != 0 ? 46u : 39u;
    uint8_t border_r = focused != 0 ? 107u : 62u;
    uint8_t border_g = focused != 0 ? 159u : 72u;
    uint8_t border_b = focused != 0 ? 230u : 88u;
    float x = (float)rect->x;
    float y = (float)rect->y;
    float width = (float)rect->width;
    float height = (float)rect->height;
    float text_y;
    size_t index;
    const size_t line_number_width = 56u;
    size_t text_capacity;

    background.x = x;
    background.y = y;
    background.w = width;
    background.h = height;
    border = background;

    if (!SDL_SetRenderDrawColor(renderer, background_r, background_g,
                                background_b, 255u) ||
        !SDL_RenderFillRect(renderer, &background) ||
        !SDL_SetRenderDrawColor(renderer, border_r, border_g, border_b, 255u) ||
        !SDL_RenderRect(renderer, &border)) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_SDL_ERROR;
    }

    stats->pane_count++;
    if (width <= 4.0f || height <= 4.0f) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_OK;
    }

    clip.x = (int)rect->x + 2;
    clip.y = (int)rect->y + 2;
    clip.w = (int)rect->width - 4;
    clip.h = (int)rect->height - 4;
    if (clip.w <= 0 || clip.h <= 0) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_OK;
    }
    if (!SDL_SetRenderClipRect(renderer, &clip)) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_SDL_ERROR;
    }

    if (view_id != NULL && view_id[0] != '\0') {
        (void)SDL_SetRenderDrawColor(renderer, 210u, 218u, 231u, 255u);
        (void)SDL_RenderDebugText(renderer, x + 8.0f, y + 8.0f, view_id);
    } else {
        (void)SDL_SetRenderDrawColor(renderer, 142u, 151u, 165u, 255u);
        (void)SDL_RenderDebugText(renderer, x + 8.0f, y + 8.0f, "Empty pane");
    }

    if (view != NULL && view->semantic.visible != 0) {
        stats->rendered_view_count++;
        text_y = y + 26.0f;
        text_capacity = rect->width > line_number_width + 8u
            ? (size_t)(rect->width - (uint32_t)line_number_width - 8u) / 8u
            : 0u;
        (void)SDL_SetRenderDrawColor(renderer, 205u, 211u, 221u, 255u);
        for (index = 0u; index < view->visible.count; ++index) {
            char line_number[24];
            (void)snprintf(line_number, sizeof(line_number), "%5llu ",
                (unsigned long long)(view->visible.first_line +
                                     (uint64_t)index + 1u));
            (void)SDL_RenderDebugText(renderer, x + 8.0f, text_y, line_number);
            draw_text_ascii(renderer, x + (float)line_number_width, text_y,
                view->visible.lines[index].bytes,
                view->visible.lines[index].length, text_capacity);
            text_y += 20.0f;
            if (text_y > y + height - 10.0f) break;
        }
    } else {
        (void)SDL_SetRenderDrawColor(renderer, 142u, 151u, 165u, 255u);
        (void)SDL_RenderDebugText(renderer, x + 8.0f, y + 30.0f,
                                  "Waiting for view data");
    }

    if (!SDL_SetRenderClipRect(renderer, NULL)) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_SDL_ERROR;
    }
    return CDIN_FRONTEND_GUI_SDL_RENDER_OK;
}

static const cdin_frontend_gui_sdl_view *find_focused_view(
    const cdin_frontend_gui_sdl_model *model)
{
    size_t index;
    for (index = 0u; index < model->view_count; ++index) {
        if (model->views[index].semantic.focused != 0) {
            return &model->views[index];
        }
    }
    return NULL;
}

static cdin_frontend_gui_sdl_render_status draw_layout(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_layout *layout,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *stats)
{
    cdin_frontend_gui_sdl_rect rectangles[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    cdin_frontend_gui_sdl_geometry_status geometry_status;
    size_t index;

    geometry_status = cdin_frontend_gui_sdl_geometry_compute(
        layout, viewport, rectangles,
        CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    if (geometry_status == CDIN_FRONTEND_GUI_SDL_GEOMETRY_LIMIT) {
        const cdin_frontend_gui_sdl_view *focused = find_focused_view(model);
        return draw_view_pane(renderer, focused,
            focused != NULL ? focused->semantic.view_id : NULL,
            viewport, focused != NULL, stats);
    }
    if (geometry_status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_LAYOUT;
    }

    for (index = 0u; index < layout->node_count; ++index) {
        const cdin_frontend_gui_sdl_layout_node *node = &layout->nodes[index];
        const cdin_frontend_gui_sdl_view *view = NULL;
        const char *view_id = NULL;
        int focused = index == layout->active_leaf_index;
        if (node->kind != CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF) continue;
        if (node->has_active_view != 0) {
            if ((size_t)node->active_view_index >= model->layout.view_count) {
                return CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_LAYOUT;
            }
            view_id = model->layout.view_ids[node->active_view_index];
            view = cdin_frontend_gui_sdl_model_find_view_const(model, view_id);
        }
        {
            cdin_frontend_gui_sdl_render_status status = draw_view_pane(
                renderer, view, view_id, &rectangles[index], focused, stats);
            if (status != CDIN_FRONTEND_GUI_SDL_RENDER_OK) return status;
        }
    }
    return CDIN_FRONTEND_GUI_SDL_RENDER_OK;
}

cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes_with_layout(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_layout *layout_override,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *out_stats)
{
    cdin_frontend_gui_sdl_render_stats stats;
    const cdin_frontend_gui_sdl_view *focused_view;
    cdin_frontend_gui_sdl_render_status status;
    if (out_stats != NULL) memset(out_stats, 0, sizeof(*out_stats));
    if (renderer == NULL || model == NULL || viewport == NULL ||
        out_stats == NULL || viewport->width == 0u || viewport->height == 0u) {
        return CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_ARGUMENT;
    }
    memset(&stats, 0, sizeof(stats));
    if (model->has_layout != 0) {
        const cdin_frontend_gui_sdl_layout *layout =
            layout_override != NULL ? layout_override : &model->layout;
        if (layout->has_root != 0) {
            status = draw_layout(renderer, model, layout, viewport, &stats);
        } else {
            /* A committed empty PaneTree has no panes; do not invent a
             * viewport for a view that Runtime removed from the layout. */
            status = CDIN_FRONTEND_GUI_SDL_RENDER_OK;
        }
    } else {
        focused_view = find_focused_view(model);
        status = draw_view_pane(
            renderer, focused_view,
            focused_view != NULL ? focused_view->semantic.view_id : NULL,
            viewport, focused_view != NULL, &stats);
    }
    *out_stats = stats;
    return status;
}

cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *out_stats)
{
    return cdin_frontend_gui_sdl_render_panes_with_layout(
        renderer, model, NULL, viewport, out_stats);
}
