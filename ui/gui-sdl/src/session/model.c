#include "model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cdin_frontend_protocol_status message_text(
    const cdin_frontend_protocol_message_view *message,
    const char *field, char *output, size_t capacity, size_t *out_length)
{
    if (message == NULL || field == NULL || output == NULL ||
        capacity == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    return cdin_frontend_protocol_payload_get_text(
        message->payload, message->payload_length, field,
        output, capacity, out_length);
}

static int raw_is_null(const uint8_t *raw, size_t length)
{
    return raw != NULL && length == 1u && raw[0] == 0xf6u;
}

static int raw_is_map(const uint8_t *raw, size_t length)
{
    return raw != NULL && length != 0u && (raw[0] >> 5u) == 5u;
}

static void line_window_dispose(cdin_frontend_gui_sdl_line_window *window)
{
    size_t index;
    if (window == NULL) return;
    for (index = 0u; index < window->count; ++index) {
        free(window->lines[index].bytes);
        free(window->lines[index].spans_cbor);
    }
    memset(window, 0, sizeof(*window));
}

static void line_window_release(
    cdin_frontend_gui_sdl_model *model,
    cdin_frontend_gui_sdl_line_window *window)
{
    if (model == NULL || window == NULL) return;
    if (window->bytes <= model->cache_bytes) {
        model->cache_bytes -= window->bytes;
    } else {
        model->cache_bytes = 0u;
    }
    line_window_dispose(window);
}

static void view_release(
    cdin_frontend_gui_sdl_model *model,
    cdin_frontend_gui_sdl_view *view)
{
    if (view == NULL) return;
    line_window_release(model, &view->visible);
    line_window_release(model, &view->pending);
    memset(view, 0, sizeof(*view));
}

static uint64_t next_access(cdin_frontend_gui_sdl_model *model)
{
    if (model->access_clock != UINT64_MAX) {
        model->access_clock++;
    } else {
        /* A clock wrap is practically unreachable; renormalize deterministically. */
        size_t index;
        uint64_t value = 1u;
        for (index = 0u; index < model->view_count; ++index) {
            model->views[index].last_used = value++;
        }
        model->access_clock = value;
    }
    return model->access_clock;
}

static cdin_frontend_gui_sdl_model_status ensure_cache_room(
    cdin_frontend_gui_sdl_model *model,
    size_t required,
    cdin_frontend_gui_sdl_view *target)
{
    size_t pass;
    if (required > CDIN_FRONTEND_GUI_SDL_MAX_CACHE_BYTES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    for (pass = 0u; pass < 3u &&
         model->cache_bytes > CDIN_FRONTEND_GUI_SDL_MAX_CACHE_BYTES - required;
         ++pass) {
        size_t index;
        cdin_frontend_gui_sdl_view *candidate = NULL;
        for (index = 0u; index < model->view_count; ++index) {
            cdin_frontend_gui_sdl_view *view = &model->views[index];
            int eligible = view != target && view->visible.count != 0u;
            if (pass == 0u && view->semantic.focused != 0) eligible = 0;
            if (pass == 2u && view != target) eligible = 0;
            if (pass < 2u && view == target) eligible = 0;
            if (!eligible) continue;
            if (candidate == NULL ||
                view->last_used < candidate->last_used) {
                candidate = view;
            }
        }
        if (candidate == NULL) {
            if (pass == 0u) continue;
            if (pass == 1u) {
                /* The target's old window is the final replaceable cache. */
                if (target != NULL && target->visible.count != 0u) {
                    candidate = target;
                } else {
                    continue;
                }
            } else {
                continue;
            }
        }
        line_window_release(model, &candidate->visible);
        candidate->semantic.needs_lines = 1;
        candidate->last_used = 0u;
    }
    if (model->cache_bytes > CDIN_FRONTEND_GUI_SDL_MAX_CACHE_BYTES - required) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static size_t find_snapshot_view(
    const cdin_frontend_gui_sdl_snapshot_state *snapshot,
    const char *view_id)
{
    size_t index;
    if (snapshot == NULL || view_id == NULL) return SIZE_MAX;
    for (index = 0u; index < snapshot->view_count; ++index) {
        if (strcmp(snapshot->views[index].view_id, view_id) == 0) return index;
    }
    return SIZE_MAX;
}

static size_t find_committed_view_index(
    const cdin_frontend_gui_sdl_model *model,
    const char *view_id)
{
    size_t index;
    if (model == NULL || view_id == NULL) return SIZE_MAX;
    for (index = 0u; index < model->view_count; ++index) {
        if (strcmp(model->views[index].semantic.view_id, view_id) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

cdin_frontend_gui_sdl_view *cdin_frontend_gui_sdl_model_find_view(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id)
{
    size_t index = find_committed_view_index(model, view_id);
    return index == SIZE_MAX ? NULL : &model->views[index];
}

const cdin_frontend_gui_sdl_view *cdin_frontend_gui_sdl_model_find_view_const(
    const cdin_frontend_gui_sdl_model *model,
    const char *view_id)
{
    size_t index = find_committed_view_index(model, view_id);
    return index == SIZE_MAX ? NULL : &model->views[index];
}

cdin_frontend_gui_sdl_view *cdin_frontend_gui_sdl_model_focused_view(
    cdin_frontend_gui_sdl_model *model)
{
    size_t index;
    if (model == NULL) return NULL;
    for (index = 0u; index < model->view_count; ++index) {
        if (model->views[index].semantic.focused != 0) {
            return &model->views[index];
        }
    }
    return NULL;
}

static void snapshot_begin(cdin_frontend_gui_sdl_model *model)
{
    cdin_frontend_gui_sdl_snapshot_state *staged;
    size_t index;
    if (model->snapshot_pending_valid) return;
    staged = &model->staged_snapshot;
    memset(staged, 0, sizeof(*staged));
    staged->layout_revision = model->layout_revision;
    staged->layout = model->layout;
    staged->has_layout = model->has_layout;
    staged->view_count = model->view_count;
    for (index = 0u; index < model->view_count; ++index) {
        staged->views[index] = model->views[index].semantic;
    }
    memcpy(staged->theme, model->theme, sizeof(staged->theme));
    memcpy(staged->mode, model->mode, sizeof(staged->mode));
    memcpy(staged->status, model->status, sizeof(staged->status));
    memcpy(staged->notification, model->notification, sizeof(staged->notification));
    model->snapshot_pending_valid = 1;
}

static cdin_frontend_gui_sdl_model_status ensure_staged_view(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    cdin_frontend_gui_sdl_view_snapshot **out_view)
{
    size_t index;
    size_t length;
    if (model == NULL || view_id == NULL || view_id[0] == '\0' ||
        out_view == NULL) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    index = find_snapshot_view(&model->staged_snapshot, view_id);
    if (index != SIZE_MAX) {
        *out_view = &model->staged_snapshot.views[index];
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    /* Once a complete layout has been staged, view-scoped messages must name
     * one of its views. Before that message arrives, permit the initial
     * ordered snapshot to stage view metadata without binding to ATTACH.view_id. */
    if (model->staged_snapshot.has_layout != 0 ||
        model->staged_snapshot.view_count >= CDIN_FRONTEND_GUI_SDL_MAX_VIEWS) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    length = strlen(view_id);
    if (length > CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    index = model->staged_snapshot.view_count++;
    memset(&model->staged_snapshot.views[index], 0,
           sizeof(model->staged_snapshot.views[index]));
    memcpy(model->staged_snapshot.views[index].view_id, view_id, length + 1u);
    model->staged_snapshot.views[index].in_layout = 1;
    model->staged_snapshot.views[index].visible = 1;
    model->staged_snapshot.views[index].focused = index == 0u;
    model->staged_snapshot.views[index].needs_lines = 1;
    *out_view = &model->staged_snapshot.views[index];
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status stage_layout_views(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_layout *layout)
{
    cdin_frontend_gui_sdl_view_snapshot replacement[CDIN_FRONTEND_GUI_SDL_MAX_VIEWS];
    size_t index;
    memset(replacement, 0, sizeof(replacement));
    for (index = 0u; index < layout->view_count; ++index) {
        size_t prior = find_snapshot_view(&model->staged_snapshot,
                                          layout->view_ids[index]);
        size_t committed;
        if (prior != SIZE_MAX) {
            replacement[index] = model->staged_snapshot.views[prior];
        } else {
            committed = find_committed_view_index(model, layout->view_ids[index]);
            if (committed != SIZE_MAX) {
                replacement[index] = model->views[committed].semantic;
            } else {
                replacement[index].needs_lines = 1;
            }
            (void)memcpy(replacement[index].view_id, layout->view_ids[index],
                         strlen(layout->view_ids[index]) + 1u);
            replacement[index].in_layout = 1;
        }
        replacement[index].in_layout = 1;
        replacement[index].visible = 0;
        replacement[index].focused = 0;
    }

    for (index = 0u; index < layout->node_count; ++index) {
        const cdin_frontend_gui_sdl_layout_node *node = &layout->nodes[index];
        size_t child;
        if (node->kind != CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF ||
            node->has_active_view == 0) continue;
        for (child = 0u; child < node->view_count; ++child) {
            uint8_t view_index = node->view_indices[child];
            size_t target;
            if ((size_t)view_index >= layout->view_count) {
                return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
            }
            /* The parser's global view index maps directly to the matching
             * layout view id; the replacement array uses that same order. */
            target = (size_t)view_index;
            if (target >= layout->view_count) {
                return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
            }
            if (layout->view_count != 0u &&
                strcmp(replacement[target].view_id,
                       layout->view_ids[view_index]) != 0) {
                return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
            }
            if (node->view_indices[child] == node->active_view_index) {
                replacement[target].visible = 1;
                if (index == layout->active_leaf_index) {
                    replacement[target].focused = 1;
                }
            }
        }
    }

    memcpy(model->staged_snapshot.views, replacement, sizeof(replacement));
    model->staged_snapshot.view_count = layout->view_count;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static void snapshot_commit(cdin_frontend_gui_sdl_model *model)
{
    cdin_frontend_gui_sdl_view replacement[CDIN_FRONTEND_GUI_SDL_MAX_VIEWS];
    uint8_t moved[CDIN_FRONTEND_GUI_SDL_MAX_VIEWS] = {0u};
    size_t index;
    memset(replacement, 0, sizeof(replacement));

    for (index = 0u; index < model->staged_snapshot.view_count; ++index) {
        const cdin_frontend_gui_sdl_view_snapshot *staged =
            &model->staged_snapshot.views[index];
        size_t old_index = find_committed_view_index(model, staged->view_id);
        if (old_index != SIZE_MAX) {
            replacement[index] = model->views[old_index];
            moved[old_index] = 1u;
            memset(&model->views[old_index].visible, 0,
                   sizeof(model->views[old_index].visible));
            memset(&model->views[old_index].pending, 0,
                   sizeof(model->views[old_index].pending));
            model->views[old_index].pending_lines_valid = 0;
            if (replacement[index].semantic.version != staged->version) {
                line_window_release(model, &replacement[index].visible);
                line_window_release(model, &replacement[index].pending);
                replacement[index].has_line_request = 0;
                replacement[index].pending_lines_valid = 0;
                replacement[index].semantic.needs_lines = staged->version != 0u;
            }
        } else {
            replacement[index].semantic.needs_lines = staged->version != 0u;
        }
        replacement[index].semantic = *staged;
        if (replacement[index].semantic.version == 0u) {
            replacement[index].semantic.needs_lines = 0;
        } else if (replacement[index].visible.count == 0u &&
                   replacement[index].pending_lines_valid == 0) {
            replacement[index].semantic.needs_lines = 1;
        }
    }

    for (index = 0u; index < model->view_count; ++index) {
        if (moved[index] == 0u) view_release(model, &model->views[index]);
    }
    memcpy(model->views, replacement, sizeof(replacement));
    model->view_count = model->staged_snapshot.view_count;

    model->layout_revision = model->staged_snapshot.layout_revision;
    model->layout = model->staged_snapshot.layout;
    model->has_layout = model->staged_snapshot.has_layout;
    model->presentation = model->staged_snapshot.presentation;
    memcpy(model->theme, model->staged_snapshot.theme, sizeof(model->theme));
    memcpy(model->mode, model->staged_snapshot.mode, sizeof(model->mode));
    memcpy(model->status, model->staged_snapshot.status, sizeof(model->status));
    memcpy(model->notification, model->staged_snapshot.notification,
           sizeof(model->notification));
    model->snapshot_pending_valid = 0;
}

static cdin_frontend_gui_sdl_model_status apply_invalidate(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    size_t id_length = 0u;
    cdin_frontend_gui_sdl_view_snapshot *view;
    uint64_t version = 0u;
    cdin_frontend_gui_sdl_model_status status;
    if (message_text(message, "view_id", view_id, sizeof(view_id), &id_length) !=
            CDIN_FRONTEND_PROTOCOL_OK || id_length == 0u) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    status = ensure_staged_view(model, view_id, &view);
    if (status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) return status;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
        CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < view->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version != view->version) {
        view->has_cursor = 0;
        view->has_selection = 0;
        view->has_scroll_anchor = 0;
    }
    view->version = version;
    view->needs_lines = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_cursor(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    size_t id_length = 0u;
    cdin_frontend_gui_sdl_view_snapshot *view;
    uint64_t version = 0u;
    uint64_t position = 0u;
    uint64_t scroll_anchor = 0u;
    cdin_frontend_protocol_status optional_status;
    cdin_frontend_gui_sdl_model_status status;
    if (message_text(message, "view_id", view_id, sizeof(view_id), &id_length) !=
            CDIN_FRONTEND_PROTOCOL_OK || id_length == 0u ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "position", &position) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    status = ensure_staged_view(model, view_id, &view);
    if (status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) return status;
    if (version < view->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version != view->version) {
        view->has_selection = 0;
        view->has_scroll_anchor = 0;
    }
    view->version = version;
    view->cursor_position = position;
    view->has_cursor = 1;
    optional_status = cdin_frontend_protocol_payload_get_uint(
        message->payload, message->payload_length, "scroll_anchor", &scroll_anchor);
    if (optional_status == CDIN_FRONTEND_PROTOCOL_OK) {
        view->scroll_anchor = scroll_anchor;
        view->has_scroll_anchor = 1;
    } else if (optional_status == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
        view->has_scroll_anchor = 0;
    } else {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_selection(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    size_t id_length = 0u;
    cdin_frontend_gui_sdl_view_snapshot *view;
    uint64_t version = 0u;
    uint64_t anchor = 0u;
    uint64_t caret = 0u;
    cdin_frontend_gui_sdl_model_status status;
    if (message_text(message, "view_id", view_id, sizeof(view_id), &id_length) !=
            CDIN_FRONTEND_PROTOCOL_OK || id_length == 0u ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "anchor", &anchor) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "caret", &caret) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    status = ensure_staged_view(model, view_id, &view);
    if (status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) return status;
    if (version < view->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version != view->version) {
        view->has_cursor = 0;
        view->has_scroll_anchor = 0;
    }
    view->version = version;
    view->selection_anchor = anchor;
    view->selection_caret = caret;
    view->has_selection = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status validate_line_spans(
    const uint8_t *raw,
    size_t raw_length,
    size_t line_length,
    size_t *out_count)
{
    size_t count = 0u;
    size_t index;
    if (out_count != NULL) *out_count = 0u;
    if (raw == NULL || raw_length == 0u || out_count == NULL ||
        cdin_frontend_protocol_array_count(raw, raw_length, &count) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    if (count > CDIN_FRONTEND_GUI_SDL_MAX_LINE_SPANS) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    for (index = 0u; index < count; ++index) {
        const uint8_t *span = NULL;
        size_t span_length = 0u;
        uint64_t start = 0u;
        uint64_t end = 0u;
        char role[CDIN_FRONTEND_GUI_SDL_MAX_PANEL_ID_BYTES + 1u];
        size_t role_length = 0u;
        cdin_frontend_protocol_status status;
        if (cdin_frontend_protocol_array_get_raw(
                raw, raw_length, index, &span, &span_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(
                span, span_length, "start", &start) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(
                span, span_length, "end", &end) !=
                CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        status = cdin_frontend_protocol_payload_get_text(
            span, span_length, "role", role, sizeof(role), &role_length);
        if (status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
        }
        if (status != CDIN_FRONTEND_PROTOCOL_OK || role_length == 0u ||
            start > end || end > (uint64_t)line_length) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
    }
    *out_count = count;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_lines(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *array = NULL;
    size_t array_length = 0u;
    size_t count = 0u;
    const uint8_t *source[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    const uint8_t *span_source[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    size_t lengths[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    size_t span_lengths[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    size_t span_counts[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    uint64_t version = 0u;
    uint64_t first_line = 0u;
    uint64_t first_byte = 0u;
    int has_first_byte = 0;
    size_t id_length = 0u;
    size_t total_bytes = 0u;
    size_t index;
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    cdin_frontend_gui_sdl_view *view;
    cdin_frontend_gui_sdl_line_window pending;
    cdin_frontend_gui_sdl_model_status cache_status;
    cdin_frontend_gui_sdl_model_status status;

    memset(&pending, 0, sizeof(pending));
    memset(span_source, 0, sizeof(span_source));
    memset(span_lengths, 0, sizeof(span_lengths));
    memset(span_counts, 0, sizeof(span_counts));
    if (message_text(message, "view_id", view_id, sizeof(view_id), &id_length) !=
            CDIN_FRONTEND_PROTOCOL_OK || id_length == 0u ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "first_line", &first_line) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "lines",
            &array, &array_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_array_count(array, array_length, &count) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    {
        cdin_frontend_protocol_status byte_status =
            cdin_frontend_protocol_payload_get_uint(
                message->payload, message->payload_length, "first_byte", &first_byte);
        if (byte_status == CDIN_FRONTEND_PROTOCOL_OK) has_first_byte = 1;
        else if (byte_status != CDIN_FRONTEND_PROTOCOL_UNSUPPORTED)
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    view = cdin_frontend_gui_sdl_model_find_view(model, view_id);
    if (view == NULL) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version == 0u || version != view->semantic.version || count == 0u ||
        view->has_line_request == 0 ||
        version != view->requested_version ||
        count > view->requested_line_count ||
        (view->has_requested_anchor_byte == 0 &&
         first_line != view->requested_first_line) ||
        (view->expects_first_byte != 0 && has_first_byte == 0) ||
        (view->expects_first_byte == 0 && has_first_byte != 0)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    if (view->has_requested_anchor_byte != 0 &&
        (has_first_byte == 0 || first_byte > view->requested_anchor_byte)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    if (count > model->visible_line_capacity ||
        count > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }

    for (index = 0u; index < count; ++index) {
        const uint8_t *line_map = NULL;
        size_t line_map_length = 0u;
        cdin_frontend_protocol_status span_status;
        if (cdin_frontend_protocol_array_get_raw(
                array, array_length, index, &line_map, &line_map_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_bytes(
                line_map, line_map_length, "bytes",
                &source[index], &lengths[index]) != CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        if (lengths[index] > CDIN_FRONTEND_GUI_SDL_MAX_LINE_BYTES ||
            lengths[index] > SIZE_MAX - total_bytes) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
        }
        total_bytes += lengths[index];

        span_status = cdin_frontend_protocol_payload_get_raw(
            line_map, line_map_length, "spans",
            &span_source[index], &span_lengths[index]);
        if (span_status == CDIN_FRONTEND_PROTOCOL_OK) {
            status = validate_line_spans(
                span_source[index], span_lengths[index],
                lengths[index], &span_counts[index]);
            if (status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) return status;
            if (span_lengths[index] > SIZE_MAX - total_bytes) {
                return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
            }
            total_bytes += span_lengths[index];
        } else if (span_status != CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
    }
    if (view->has_requested_anchor_byte != 0 &&
        view->requested_anchor_byte - first_byte > (uint64_t)lengths[0]) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    cache_status = ensure_cache_room(model, total_bytes, view);
    if (cache_status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) return cache_status;

    pending.first_line = first_line;
    pending.first_byte = first_byte;
    pending.has_first_byte = has_first_byte;
    pending.count = count;
    pending.bytes = total_bytes;
    for (index = 0u; index < count; ++index) {
        if (lengths[index] != 0u) {
            pending.lines[index].bytes = (uint8_t *)malloc(lengths[index]);
            if (pending.lines[index].bytes == NULL) {
                line_window_dispose(&pending);
                return CDIN_FRONTEND_GUI_SDL_MODEL_NO_MEMORY;
            }
            memcpy(pending.lines[index].bytes, source[index], lengths[index]);
        }
        pending.lines[index].length = lengths[index];
        pending.lines[index].span_count = span_counts[index];
        pending.lines[index].spans_cbor_length = span_lengths[index];
        if (span_lengths[index] != 0u) {
            pending.lines[index].spans_cbor =
                (uint8_t *)malloc(span_lengths[index]);
            if (pending.lines[index].spans_cbor == NULL) {
                line_window_dispose(&pending);
                return CDIN_FRONTEND_GUI_SDL_MODEL_NO_MEMORY;
            }
            memcpy(pending.lines[index].spans_cbor,
                   span_source[index], span_lengths[index]);
        }
    }

    line_window_release(model, &view->pending);
    view->pending = pending;
    view->pending_lines_valid = 1;
    model->cache_bytes += total_bytes;
    view->last_used = next_access(model);
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static void apply_response_flush(cdin_frontend_gui_sdl_model *model)
{
    size_t index;
    for (index = 0u; index < model->view_count; ++index) {
        cdin_frontend_gui_sdl_view *view = &model->views[index];
        if (view->pending_lines_valid == 0) continue;
        line_window_release(model, &view->visible);
        view->visible = view->pending;
        memset(&view->pending, 0, sizeof(view->pending));
        view->pending_lines_valid = 0;
        view->has_line_request = 0;
        view->has_requested_anchor_byte = 0;
        view->expects_first_byte = 0;
        view->semantic.needs_lines = 0;
        view->last_used = next_access(model);
    }
}

static cdin_frontend_gui_sdl_model_status apply_flush(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_model_action *action)
{
    char scope[16];
    size_t scope_length = 0u;
    uint64_t sequence = 0u;
    uint64_t revision = 0u;
    size_t index;
    if (cdin_frontend_protocol_payload_get_text(
            message->payload, message->payload_length, "scope",
            scope, sizeof(scope), &scope_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "sequence", &sequence) !=
            CDIN_FRONTEND_PROTOCOL_OK || sequence == 0u) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    action->acknowledge_sequence = sequence;
    if (scope_length == 8u && memcmp(scope, "snapshot", 8u) == 0) {
        if (cdin_frontend_protocol_payload_get_uint(
                message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        if (revision == 0u ||
            (model->has_snapshot_revision != 0 &&
             revision <= model->snapshot_revision)) {
            model->snapshot_pending_valid = 0;
            memset(&model->staged_snapshot, 0, sizeof(model->staged_snapshot));
            return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
        }
        if (model->snapshot_pending_valid != 0) snapshot_commit(model);
        model->snapshot_revision = revision;
        model->has_snapshot_revision = 1;
        for (index = 0u; index < model->view_count; ++index) {
            const cdin_frontend_gui_sdl_view *view = &model->views[index];
            if (view->semantic.visible == 0 || view->semantic.version == 0u ||
                view->semantic.needs_lines == 0 ||
                view->has_line_request != 0 || view->pending_lines_valid != 0) {
                continue;
            }
            if (action->request_count >= CDIN_FRONTEND_GUI_SDL_MAX_VIEWS) break;
            (void)memcpy(action->requests[action->request_count].view_id,
                         view->semantic.view_id,
                         strlen(view->semantic.view_id) + 1u);
            action->requests[action->request_count].first_line = 0u;
            action->requests[action->request_count].line_count =
                (uint64_t)model->visible_line_capacity;
            action->requests[action->request_count].version =
                view->semantic.version;
            action->requests[action->request_count].has_anchor_byte =
                view->semantic.has_scroll_anchor;
            action->requests[action->request_count].anchor_byte =
                view->semantic.scroll_anchor;
            action->request_count++;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    if (scope_length == 8u && memcmp(scope, "response", 8u) == 0) {
        if (cdin_frontend_protocol_payload_get_uint(
                message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        apply_response_flush(model);
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
}

static void optional_text(const cdin_frontend_protocol_message_view *message,
                          const char *field, char *output, size_t capacity)
{
    size_t length = 0u;
    if (cdin_frontend_protocol_payload_get_text(
            message->payload, message->payload_length, field,
            output, capacity, &length) != CDIN_FRONTEND_PROTOCOL_OK) {
        output[0] = '\0';
    }
}

static size_t find_panel_index(
    const cdin_frontend_gui_sdl_presentation *presentation,
    const char *panel_id)
{
    size_t index;
    if (presentation == NULL || panel_id == NULL) return SIZE_MAX;
    for (index = 0u; index < presentation->panel_count; ++index) {
        if (strcmp(presentation->panels[index].panel_id, panel_id) == 0) {
            return index;
        }
    }
    return SIZE_MAX;
}

static cdin_frontend_gui_sdl_model_status apply_style_table(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_presentation *presentation =
        &model->staged_snapshot.presentation;
    const uint8_t *styles = NULL;
    size_t styles_length = 0u;
    uint64_t revision = 0u;
    size_t base_bytes;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length,
            "revision", &revision) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length,
            "styles", &styles, &styles_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        !raw_is_map(styles, styles_length)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    if (styles_length > CDIN_FRONTEND_GUI_SDL_MAX_STYLE_TABLE_BYTES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    base_bytes = presentation->encoded_bytes -
        presentation->style_table_length;
    if (base_bytes > CDIN_FRONTEND_GUI_SDL_MAX_PRESENTATION_BYTES -
                     styles_length) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    memcpy(presentation->style_table_cbor, styles, styles_length);
    presentation->style_table_length = styles_length;
    presentation->style_revision = revision;
    presentation->encoded_bytes = base_bytes + styles_length;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_panel(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_presentation *presentation =
        &model->staged_snapshot.presentation;
    const uint8_t *panel = NULL;
    size_t panel_length = 0u;
    char panel_id[CDIN_FRONTEND_GUI_SDL_MAX_PANEL_ID_BYTES + 1u];
    size_t id_length = 0u;
    uint64_t revision = 0u;
    size_t index;
    size_t old_length = 0u;
    size_t base_bytes;
    cdin_frontend_protocol_status id_status;

    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length,
            "revision", &revision) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length,
            "panel", &panel, &panel_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    id_status = message_text(message, "panel_id", panel_id,
                             sizeof(panel_id), &id_length);
    if (id_status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    if (id_status != CDIN_FRONTEND_PROTOCOL_OK || id_length == 0u) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    (void)revision;
    index = find_panel_index(presentation, panel_id);
    if (index != SIZE_MAX) {
        old_length = presentation->panels[index].payload_length;
    }
    base_bytes = presentation->encoded_bytes - old_length;

    if (raw_is_null(panel, panel_length)) {
        if (index != SIZE_MAX) {
            if (index + 1u < presentation->panel_count) {
                memmove(&presentation->panels[index],
                        &presentation->panels[index + 1u],
                        (presentation->panel_count - index - 1u) *
                            sizeof(presentation->panels[0]));
            }
            presentation->panel_count--;
            memset(&presentation->panels[presentation->panel_count], 0,
                   sizeof(presentation->panels[0]));
            presentation->encoded_bytes = base_bytes;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    if (!raw_is_map(panel, panel_length)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    if (panel_length > CDIN_FRONTEND_GUI_SDL_MAX_PANEL_BYTES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    if (index == SIZE_MAX &&
        presentation->panel_count >= CDIN_FRONTEND_GUI_SDL_MAX_PANELS) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    if (panel_length > CDIN_FRONTEND_GUI_SDL_MAX_PRESENTATION_BYTES -
                       base_bytes) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    if (index == SIZE_MAX) index = presentation->panel_count++;
    (void)memcpy(presentation->panels[index].panel_id,
                 panel_id, id_length + 1u);
    memcpy(presentation->panels[index].payload_cbor, panel, panel_length);
    presentation->panels[index].payload_length = panel_length;
    presentation->encoded_bytes = base_bytes + panel_length;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_prompt(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_presentation *presentation =
        &model->staged_snapshot.presentation;
    const uint8_t *prompt = NULL;
    size_t prompt_length = 0u;
    uint64_t revision = 0u;
    size_t base_bytes;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length,
            "revision", &revision) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length,
            "prompt", &prompt, &prompt_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    (void)revision;
    base_bytes = presentation->encoded_bytes - presentation->prompt_length;
    if (raw_is_null(prompt, prompt_length)) {
        presentation->has_prompt = 0;
        presentation->prompt_length = 0u;
        presentation->encoded_bytes = base_bytes;
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    if (!raw_is_map(prompt, prompt_length)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    if (prompt_length > CDIN_FRONTEND_GUI_SDL_MAX_PROMPT_BYTES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    if (prompt_length > CDIN_FRONTEND_GUI_SDL_MAX_PRESENTATION_BYTES -
                        base_bytes) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }
    memcpy(presentation->prompt_cbor, prompt, prompt_length);
    presentation->prompt_length = prompt_length;
    presentation->has_prompt = 1;
    presentation->encoded_bytes = base_bytes + prompt_length;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_status(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *map = NULL;
    size_t map_length = 0u;
    if (cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "status",
            &map, &map_length) == CDIN_FRONTEND_PROTOCOL_OK) {
        size_t length = 0u;
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "mode", model->staged_snapshot.mode,
                sizeof(model->staged_snapshot.mode), &length) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
            model->staged_snapshot.mode[0] = '\0';
        }
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "text", model->staged_snapshot.status,
                sizeof(model->staged_snapshot.status), &length) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
            model->staged_snapshot.status[0] = '\0';
        }
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_init(
    cdin_frontend_gui_sdl_model *model,
    const char *attach_view_id,
    size_t visible_line_capacity)
{
    if (model == NULL || attach_view_id == NULL ||
        visible_line_capacity == 0u ||
        visible_line_capacity > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    memset(model, 0, sizeof(*model));
    model->visible_line_capacity = visible_line_capacity;
    (void)snprintf(model->theme, sizeof(model->theme), "default");
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

void cdin_frontend_gui_sdl_model_destroy(cdin_frontend_gui_sdl_model *model)
{
    size_t index;
    if (model == NULL) return;
    for (index = 0u; index < model->view_count; ++index) {
        view_release(model, &model->views[index]);
    }
    memset(model, 0, sizeof(*model));
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_expect_lines_ex(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    uint64_t first_line_fallback,
    uint64_t line_count,
    uint64_t version,
    int has_anchor_byte,
    uint64_t anchor_byte,
    int expects_first_byte)
{
    cdin_frontend_gui_sdl_view *view;
    if (model == NULL || view_id == NULL || view_id[0] == '\0' ||
        version == 0u || line_count == 0u ||
        line_count > (uint64_t)model->visible_line_capacity ||
        first_line_fallback > UINT64_MAX - line_count ||
        (has_anchor_byte != 0 && expects_first_byte == 0)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    view = cdin_frontend_gui_sdl_model_find_view(model, view_id);
    if (view == NULL || version != view->semantic.version) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    line_window_release(model, &view->pending);
    view->pending_lines_valid = 0;
    view->requested_first_line = first_line_fallback;
    view->requested_line_count = line_count;
    view->requested_version = version;
    view->requested_anchor_byte = anchor_byte;
    view->has_requested_anchor_byte = has_anchor_byte != 0;
    view->expects_first_byte = expects_first_byte != 0;
    view->has_line_request = 1;
    view->semantic.needs_lines = 1;
    view->last_used = next_access(model);
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_expect_lines(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version)
{
    return cdin_frontend_gui_sdl_model_expect_lines_ex(
        model, view_id, first_line, line_count, version, 0, 0u, 0);
}

void cdin_frontend_gui_sdl_model_cancel_line_request(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id)
{
    cdin_frontend_gui_sdl_view *view =
        cdin_frontend_gui_sdl_model_find_view(model, view_id);
    if (view == NULL) return;
    view->has_line_request = 0;
    view->has_requested_anchor_byte = 0;
    view->expects_first_byte = 0;
    line_window_release(model, &view->pending);
    view->pending_lines_valid = 0;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_apply(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_model_action *out_action)
{
    cdin_frontend_gui_sdl_model_status status = CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    if (model == NULL || message == NULL || out_action == NULL ||
        (message->payload == NULL && message->payload_length != 0u)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    memset(out_action, 0, sizeof(*out_action));
    if (message->type != CDIN_FRONTEND_PROTOCOL_UI_FLUSH &&
        message->type != CDIN_FRONTEND_PROTOCOL_VIEW_LINES) {
        snapshot_begin(model);
    }
    switch (message->type) {
        case CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE:
            status = apply_invalidate(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_VIEW_LINES:
            status = apply_lines(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_CURSOR:
            status = apply_cursor(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_SELECTION:
            status = apply_selection(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_STATUSLINE:
            status = apply_status(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE:
            status = apply_style_table(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_PANEL:
            status = apply_panel(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_PROMPT:
            status = apply_prompt(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_OPTIONS:
            optional_text(message, "theme", model->staged_snapshot.theme,
                          sizeof(model->staged_snapshot.theme));
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT: {
            cdin_frontend_gui_sdl_layout parsed;
            cdin_frontend_gui_sdl_layout_status layout_status =
                cdin_frontend_gui_sdl_layout_parse(message, &parsed);
            if (layout_status == CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) {
                status = stage_layout_views(model, &parsed);
                if (status == CDIN_FRONTEND_GUI_SDL_MODEL_OK) {
                    model->staged_snapshot.layout = parsed;
                    model->staged_snapshot.layout_revision = parsed.revision;
                    model->staged_snapshot.has_layout = 1;
                }
            } else {
                status = layout_status == CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT
                    ? CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT
                    : CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
            }
            break;
        }
        case CDIN_FRONTEND_PROTOCOL_UI_NOTIFY:
            optional_text(message, "text", model->staged_snapshot.notification,
                          sizeof(model->staged_snapshot.notification));
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_FLUSH:
            status = apply_flush(model, message, out_action);
            break;
        default:
            break;
    }
    return status;
}
