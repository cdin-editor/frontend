#include "render.h"

#include "../layout/geometry.h"

#include <stdio.h>
#include <string.h>

static size_t display_scalar_width(
    const uint8_t *bytes, size_t length, size_t offset)
{
    uint8_t first;
    uint8_t second;
    uint8_t third;
    uint8_t fourth;
    if (bytes == NULL || offset >= length) return 0u;
    first = bytes[offset];
    if (first < 0x80u) return 1u;
    if (first >= 0xc2u && first <= 0xdfu && offset + 1u < length) {
        second = bytes[offset + 1u];
        return second >= 0x80u && second <= 0xbfu ? 2u : 1u;
    }
    if (first >= 0xe0u && first <= 0xefu && offset + 2u < length) {
        second = bytes[offset + 1u];
        third = bytes[offset + 2u];
        if (third < 0x80u || third > 0xbfu) return 1u;
        if (first == 0xe0u) return second >= 0xa0u && second <= 0xbfu ? 3u : 1u;
        if (first == 0xedu) return second >= 0x80u && second <= 0x9fu ? 3u : 1u;
        return second >= 0x80u && second <= 0xbfu ? 3u : 1u;
    }
    if (first >= 0xf0u && first <= 0xf4u && offset + 3u < length) {
        second = bytes[offset + 1u];
        third = bytes[offset + 2u];
        fourth = bytes[offset + 3u];
        if (third < 0x80u || third > 0xbfu ||
            fourth < 0x80u || fourth > 0xbfu) return 1u;
        if (first == 0xf0u) return second >= 0x90u && second <= 0xbfu ? 4u : 1u;
        if (first == 0xf4u) return second >= 0x80u && second <= 0x8fu ? 4u : 1u;
        return second >= 0x80u && second <= 0xbfu ? 4u : 1u;
    }
    return 1u;
}

static size_t debug_text_columns(
    const uint8_t *bytes, size_t length, size_t capacity,
    char output[512])
{
    size_t offset = 0u;
    size_t count = 0u;
    size_t limit = capacity < 511u ? capacity : 511u;
    while (offset < length && count < limit) {
        size_t width = display_scalar_width(bytes, length, offset);
        uint8_t first = bytes[offset];
        output[count++] = width == 1u && first >= 0x20u && first < 0x7fu
            ? (char)first : '?';
        offset += width == 0u ? 1u : width;
    }
    output[count] = '\0';
    return count;
}

static void draw_text_run(
    SDL_Renderer *renderer,
    float x,
    float y,
    const uint8_t *bytes,
    size_t length,
    size_t column,
    size_t capacity,
    const cdin_frontend_gui_sdl_style_role *style,
    const uint8_t base_background[3])
{
    char text[512];
    size_t columns;
    uint8_t foreground[4] = {205u, 211u, 221u, 255u};
    uint8_t background[4] = {0u, 0u, 0u, 255u};
    int draw_background = 0;
    SDL_FRect background_rect;
    float run_x = x + (float)column * 8.0f;
    size_t channel;

    if (renderer == NULL || bytes == NULL || length == 0u ||
        column >= capacity) return;
    columns = debug_text_columns(bytes, length, capacity - column, text);
    if (columns == 0u) return;
    if (base_background != NULL) {
        background[0] = base_background[0];
        background[1] = base_background[1];
        background[2] = base_background[2];
    }
    if (style != NULL) {
        if (style->has_foreground != 0u) {
            memcpy(foreground, style->foreground, sizeof(foreground));
        }
        if (style->has_background != 0u) {
            memcpy(background, style->background, sizeof(background));
            draw_background = 1;
        }
        if (style->inverse != 0u) {
            uint8_t previous_foreground[4];
            memcpy(previous_foreground, foreground, sizeof(previous_foreground));
            memcpy(foreground, background, sizeof(foreground));
            memcpy(background, previous_foreground, sizeof(background));
            draw_background = 1;
        }
        if (style->dim != 0u) {
            for (channel = 0u; channel < 3u; ++channel) {
                foreground[channel] =
                    (uint8_t)(((unsigned)foreground[channel] * 2u) / 3u);
            }
        }
    }
    if (draw_background != 0) {
        background_rect.x = run_x;
        background_rect.y = y;
        background_rect.w = (float)columns * 8.0f;
        background_rect.h = 9.0f;
        (void)SDL_SetRenderDrawColor(renderer, background[0], background[1],
                                     background[2], background[3]);
        (void)SDL_RenderFillRect(renderer, &background_rect);
    }
    (void)SDL_SetRenderDrawColor(renderer, foreground[0], foreground[1],
                                 foreground[2], foreground[3]);
    if (style != NULL && style->italic != 0u) {
        size_t index;
        for (index = 0u; index < columns; ++index) {
            char glyph[2] = {text[index], '\0'};
            float slant = (float)(columns - index - 1u) * 0.75f;
            (void)SDL_RenderDebugText(renderer,
                run_x + (float)index * 8.0f + slant, y, glyph);
            if (style->bold != 0u) {
                (void)SDL_RenderDebugText(renderer,
                    run_x + (float)index * 8.0f + slant + 1.0f, y, glyph);
            }
        }
    } else {
        (void)SDL_RenderDebugText(renderer, run_x, y, text);
        if (style != NULL && style->bold != 0u) {
            (void)SDL_RenderDebugText(renderer, run_x + 1.0f, y, text);
        }
    }
    if (style != NULL && (style->underline != 0u ||
                          style->strikethrough != 0u)) {
        if (style->underline != 0u) {
            (void)SDL_RenderLine(renderer, run_x, y + 8.0f,
                                 run_x + (float)columns * 8.0f, y + 8.0f);
        }
        if (style->strikethrough != 0u) {
            (void)SDL_RenderLine(renderer, run_x, y + 4.0f,
                                 run_x + (float)columns * 8.0f, y + 4.0f);
        }
    }
}

static void draw_line_with_styles(
    SDL_Renderer *renderer,
    float x,
    float y,
    const cdin_frontend_gui_sdl_line *line,
    size_t capacity,
    const cdin_frontend_gui_sdl_presentation *presentation,
    const uint8_t base_background[3])
{
    size_t span_count = 0u;
    size_t span_index;
    size_t byte_cursor = 0u;
    size_t column = 0u;
    if (line == NULL || line->bytes == NULL || line->length == 0u) return;
    if (line->spans_cbor != NULL && line->spans_cbor_length != 0u &&
        cdin_frontend_protocol_array_count(
            line->spans_cbor, line->spans_cbor_length, &span_count) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
        draw_text_run(renderer, x, y, line->bytes, line->length, 0u,
                      capacity, NULL, base_background);
        return;
    }
    for (span_index = 0u; span_index < span_count && column < capacity;
         ++span_index) {
        const uint8_t *span = NULL;
        size_t span_length = 0u;
        uint64_t start = 0u;
        uint64_t end = 0u;
        char role_id[CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLE_ID_BYTES + 1u];
        size_t role_length = 0u;
        const cdin_frontend_gui_sdl_style_role *style = NULL;
        if (cdin_frontend_protocol_array_get_raw(
                line->spans_cbor, line->spans_cbor_length, span_index,
                &span, &span_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(
                span, span_length, "start", &start) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(
                span, span_length, "end", &end) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_text(
                span, span_length, "role", role_id, sizeof(role_id),
                &role_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            start < (uint64_t)byte_cursor || end <= start ||
            end > (uint64_t)line->length) {
            draw_text_run(renderer, x, y, line->bytes, line->length, 0u,
                          capacity, NULL, base_background);
            return;
        }
        (void)role_length;
        if (start > (uint64_t)byte_cursor) {
            size_t prefix = (size_t)start - byte_cursor;
            char ignored[512];
            size_t rendered = debug_text_columns(
                line->bytes + byte_cursor, prefix, capacity - column, ignored);
            draw_text_run(renderer, x, y, line->bytes + byte_cursor, prefix,
                          column, capacity, NULL, base_background);
            column += rendered;
        }
        if (presentation != NULL) {
            style = cdin_frontend_gui_sdl_style_table_find(
                &presentation->decoded_style_table, role_id);
        }
        {
            size_t styled_length = (size_t)(end - start);
            char ignored[512];
            size_t rendered = debug_text_columns(
                line->bytes + (size_t)start, styled_length,
                capacity - column, ignored);
            draw_text_run(renderer, x, y,
                          line->bytes + (size_t)start, styled_length,
                          column, capacity, style, base_background);
            column += rendered;
        }
        byte_cursor = (size_t)end;
    }
    if (byte_cursor < line->length && column < capacity) {
        draw_text_run(renderer, x, y, line->bytes + byte_cursor,
                      line->length - byte_cursor, column, capacity,
                      NULL, base_background);
    }
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
    float text_x = (float)rect->x + 36.0f;
    float text_y = (float)rect->y + (float)rect->height - 18.0f;
    SDL_FRect selection;
    if (ime == NULL || ime->view_id == NULL || ime->preedit == NULL ||
        rect->width < 72u || rect->height < 40u) return;
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
}

static cdin_frontend_gui_sdl_render_status draw_view_pane(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_view *view,
    const char *view_id,
    const cdin_frontend_gui_sdl_rect *rect,
    int focused,
    const cdin_frontend_gui_sdl_ime_overlay *ime_overlay,
    const cdin_frontend_gui_sdl_presentation *presentation,
    cdin_frontend_gui_sdl_render_stats *stats)
{
    SDL_FRect background;
    uint8_t base_background[3];
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

    base_background[0] = background_r;
    base_background[1] = background_g;
    base_background[2] = background_b;
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
        if (text_capacity > 511u) text_capacity = 511u;
        (void)SDL_SetRenderDrawColor(renderer, 205u, 211u, 221u, 255u);
        for (index = 0u; index < view->visible.count; ++index) {
            char line_number[24];
            (void)snprintf(line_number, sizeof(line_number), "%5llu ",
                (unsigned long long)(view->visible.first_line +
                                     (uint64_t)index + 1u));
            (void)SDL_RenderDebugText(renderer, x + 8.0f, text_y, line_number);
            draw_line_with_styles(
                renderer, x + (float)line_number_width, text_y,
                &view->visible.lines[index], text_capacity,
                presentation, base_background);
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
            viewport, focused != NULL, ime_overlay, &model->presentation, stats);
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
                ime_overlay, &model->presentation, stats);
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
            viewport, focused_view != NULL, ime_overlay,
            &model->presentation, &stats);
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
