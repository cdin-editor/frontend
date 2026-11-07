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

static void line_window_dispose(cdin_frontend_gui_sdl_line_window *window)
{
    size_t index;
    if (window == NULL) return;
    for (index = 0u; index < window->count; ++index) {
        free(window->lines[index].bytes);
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

static cdin_frontend_gui_sdl_model_status apply_lines(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *array = NULL;
    size_t array_length = 0u;
    size_t count = 0u;
    const uint8_t *source[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    size_t lengths[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
    uint64_t version = 0u;
    uint64_t first_line = 0u;
    size_t id_length = 0u;
    size_t total_bytes = 0u;
    size_t index;
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    cdin_frontend_gui_sdl_view *view;
    cdin_frontend_gui_sdl_line_window pending;
    cdin_frontend_gui_sdl_model_status cache_status;

    memset(&pending, 0, sizeof(pending));
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
    view = cdin_frontend_gui_sdl_model_find_view(model, view_id);
    if (view == NULL) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version == 0u || version != view->semantic.version ||
        view->has_line_request == 0 ||
        version != view->requested_version ||
        first_line != view->requested_first_line ||
        count > view->requested_line_count) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    if (count > model->visible_line_capacity ||
        count > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }

    for (index = 0u; index < count; ++index) {
        const uint8_t *line_map = NULL;
        size_t line_map_length = 0u;
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
    }
    cache_status = ensure_cache_room(model, total_bytes, view);
    if (cache_status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) return cache_status;

    pending.first_line = first_line;
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

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_expect_lines(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version)
{
    cdin_frontend_gui_sdl_view *view;
    if (model == NULL || view_id == NULL || view_id[0] == '\0' ||
        version == 0u || line_count == 0u ||
        line_count > (uint64_t)model->visible_line_capacity ||
        first_line > UINT64_MAX - line_count) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    view = cdin_frontend_gui_sdl_model_find_view(model, view_id);
    if (view == NULL || version != view->semantic.version) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    line_window_release(model, &view->pending);
    view->pending_lines_valid = 0;
    view->requested_first_line = first_line;
    view->requested_line_count = line_count;
    view->requested_version = version;
    view->has_line_request = 1;
    view->semantic.needs_lines = 1;
    view->last_used = next_access(model);
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

void cdin_frontend_gui_sdl_model_cancel_line_request(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id)
{
    cdin_frontend_gui_sdl_view *view =
        cdin_frontend_gui_sdl_model_find_view(model, view_id);
    if (view == NULL) return;
    view->has_line_request = 0;
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
