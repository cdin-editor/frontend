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


static size_t byte_offset_to_codepoints(const uint8_t *bytes, size_t length,
                                        uint64_t byte_offset)
{
    size_t offset = 0u;
    size_t count = 0u;
    if (byte_offset > (uint64_t)length) return 0u;
    while (offset < (size_t)byte_offset) {
        uint8_t first = bytes[offset];
        size_t width = first < 0x80u ? 1u :
                       (first >= 0xc2u && first <= 0xdfu ? 2u :
                        (first >= 0xe0u && first <= 0xefu ? 3u : 4u));
        if (width > (size_t)byte_offset - offset) break;
        offset += width;
        count++;
    }
    return count;
}

static void draw_ime_overlay(
    SDL_Renderer *renderer, const cdin_frontend_gui_sdl_ime_overlay *ime,
    const cdin_frontend_gui_sdl_rect *rect)
{
    char text[512];
    size_t index = 0u, offset = 0u, cursor_column, selection_start, selection_end;
    size_t cursor = 0u;
    float text_x = (float)rect->x + 36.0f;
    float text_y = (float)rect->y + (float)rect->height - 18.0f;
    SDL_FRect selection;
    if (ime == NULL || ime->view_id == NULL || ime->preedit == NULL ||
        ime->preedit_length == 0u || rect->width < 72u || rect->height < 40u) return;
    while (offset < ime->preedit_length && index + 1u < sizeof(text) &&
           index < 56u) {
        uint8_t first = ime->preedit[offset];
        size_t width = first < 0x80u ? 1u :
                       (first >= 0xc2u && first <= 0xdfu ? 2u :
                        (first >= 0xe0u && first <= 0xefu ? 3u : 4u));
        text[index++] = width == 1u && first >= 0x20u && first < 0x7fu
            ? (char)first : '?';
        offset += width;
    }
    text[index] = '\0';
    cursor_column = byte_offset_to_codepoints(ime->preedit, ime->preedit_length,
                                               ime->cursor_byte);
    selection_start = byte_offset_to_codepoints(ime->preedit,
        ime->preedit_length, ime->selection_start_byte);
    selection_end = byte_offset_to_codepoints(ime->preedit,
        ime->preedit_length, ime->selection_end_byte);
    if (cursor_column > 56u) cursor_column = 56u;
    if (selection_start > 56u) selection_start = 56u;
    if (selection_end > 56u) selection_end = 56u;
    if (!SDL_SetRenderDrawColor(renderer, 45u, 58u, 76u, 255u)) return;
    selection.x = text_x + (float)selection_start * 8.0f;
    selection.y = text_y - 1.0f;
    selection.w = (float)(selection_end - selection_start) * 8.0f;
    selection.h = 11.0f;
    if (selection.w > 0.0f) (void)SDL_RenderFillRect(renderer, &selection);
    (void)SDL_SetRenderDrawColor(renderer, 235u, 240u, 248u, 255u);
    (void)SDL_RenderDebugText(renderer, (float)rect->x + 8.0f,
                              text_y, "IME");
    (void)SDL_RenderDebugText(renderer, text_x, text_y, text);
    (void)SDL_SetRenderDrawColor(renderer, 245u, 190u, 82u, 255u);
    (void)SDL_RenderLine(renderer, text_x + (float)cursor_column * 8.0f,
                         text_y - 1.0f, text_x + (float)cursor_column * 8.0f,
                         text_y + 10.0f);
    (void)cursor;
}

static cdin_frontend_gui_sdl_render_status draw_view_pane(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_view *view,
    const char *view_id,
    const cdin_frontend_gui_sdl_rect *rect,
    int focused,
    const cdin_frontend_gui_sdl_ime_overlay *ime_overlay,
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

    if (ime_overlay != NULL && view != NULL &&
        ime_overlay->view_id != NULL &&
        strcmp(ime_overlay->view_id, view->semantic.view_id) == 0) {
        draw_ime_overlay(renderer, ime_overlay, rect);
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
    const cdin_frontend_gui_sdl_ime_overlay *ime_overlay,
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
            viewport, focused != NULL, ime_overlay, stats);
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
                renderer, view, view_id, &rectangles[index], focused,
                ime_overlay, stats);
            if (status != CDIN_FRONTEND_GUI_SDL_RENDER_OK) return status;
        }
    }
    return CDIN_FRONTEND_GUI_SDL_RENDER_OK;
}

cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes_with_layout_and_ime(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_layout *layout_override,
    const cdin_frontend_gui_sdl_ime_overlay *ime_overlay,
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
            status = draw_layout(renderer, model, layout, viewport, ime_overlay, &stats);
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
            viewport, focused_view != NULL, ime_overlay, &stats);
    }
    *out_stats = stats;
    return status;
}

cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes_with_layout(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_layout *layout_override,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *out_stats)
{
    return cdin_frontend_gui_sdl_render_panes_with_layout_and_ime(
        renderer, model, layout_override, NULL, viewport, out_stats);
}

cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *out_stats)
{
    return cdin_frontend_gui_sdl_render_panes_with_layout_and_ime(
        renderer, model, NULL, NULL, viewport, out_stats);
}
