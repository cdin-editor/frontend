#include "model.h"

#include <stdio.h>
#include <string.h>

static cdin_frontend_protocol_status get_text(
    const cdin_frontend_protocol_message_view *message,
    const char *field, char *output, size_t capacity)
{
    size_t length = 0u;
    cdin_frontend_protocol_status status;
    if (capacity == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = cdin_frontend_protocol_payload_get_text(
        message->payload, message->payload_length, field,
        output, capacity, &length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) output[0] = '\0';
    return status;
}

static int same_view(const cdin_frontend_gui_sdl_model *model,
                     const cdin_frontend_protocol_message_view *message)
{
    char view_id[64];
    return get_text(message, "view_id", view_id, sizeof(view_id)) ==
               CDIN_FRONTEND_PROTOCOL_OK &&
           strcmp(view_id, model->view_id) == 0;
}

static void snapshot_begin(cdin_frontend_gui_sdl_model *model)
{
    cdin_frontend_gui_sdl_snapshot_state *staged;
    if (model->snapshot_pending_valid) return;
    staged = &model->staged_snapshot;
    memset(staged, 0, sizeof(*staged));
    staged->version = model->version;
    staged->cursor_position = model->cursor_position;
    staged->selection_anchor = model->selection_anchor;
    staged->selection_caret = model->selection_caret;
    staged->layout_revision = model->layout_revision;
    staged->has_cursor = model->has_cursor;
    staged->has_selection = model->has_selection;
    staged->has_layout = model->has_layout;
    staged->needs_lines = model->needs_lines;
    memcpy(staged->theme, model->theme, sizeof(staged->theme));
    memcpy(staged->mode, model->mode, sizeof(staged->mode));
    memcpy(staged->status, model->status, sizeof(staged->status));
    memcpy(staged->notification, model->notification, sizeof(staged->notification));
    model->snapshot_pending_valid = 1;
}

static void snapshot_commit(cdin_frontend_gui_sdl_model *model)
{
    const cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    int version_changed = staged->version != model->version;
    model->version = staged->version;
    model->cursor_position = staged->cursor_position;
    model->selection_anchor = staged->selection_anchor;
    model->selection_caret = staged->selection_caret;
    model->layout_revision = staged->layout_revision;
    model->has_cursor = staged->has_cursor;
    model->has_selection = staged->has_selection;
    model->has_layout = staged->has_layout;
    model->needs_lines = staged->needs_lines;
    memcpy(model->theme, staged->theme, sizeof(model->theme));
    memcpy(model->mode, staged->mode, sizeof(model->mode));
    memcpy(model->status, staged->status, sizeof(model->status));
    memcpy(model->notification, staged->notification, sizeof(model->notification));
    if (version_changed) {
        memset(&model->visible, 0, sizeof(model->visible));
        model->pending_lines_valid = 0;
        model->needs_lines = model->version != 0u;
    }
    model->snapshot_pending_valid = 0;
}

static cdin_frontend_gui_sdl_model_status apply_invalidate(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    uint64_t version = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
        CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < staged->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version != staged->version) {
        staged->has_cursor = 0;
        staged->has_selection = 0;
    }
    staged->version = version;
    staged->needs_lines = 1;
    /* Do not discard an independently staged response here. Invalidation is
     * not visible until its snapshot ui.flush commits; the response may still
     * belong to the currently committed document version. snapshot_commit()
     * invalidates the line cache only after a newer version is committed. */
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_cursor(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    uint64_t version = 0u;
    uint64_t position = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "position", &position) !=
            CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < staged->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    staged->cursor_position = position;
    staged->has_cursor = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_selection(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    uint64_t version = 0u;
    uint64_t anchor = 0u;
    uint64_t caret = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "anchor", &anchor) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "caret", &caret) !=
            CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < staged->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    staged->selection_anchor = anchor;
    staged->selection_caret = caret;
    staged->has_selection = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_lines(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *array = NULL;
    size_t array_length = 0u;
    size_t count = 0u;
    uint64_t version = 0u;
    uint64_t first_line = 0u;
    size_t index;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "first_line", &first_line) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "lines",
            &array, &array_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_array_count(array, array_length, &count) !=
            CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version != model->version || version == 0u) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    if (count > model->visible_line_capacity ||
        count > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }

    memset(&model->pending, 0, sizeof(model->pending));
    model->pending.first_line = first_line;
    model->pending.count = count;
    for (index = 0u; index < count; ++index) {
        const uint8_t *line_map = NULL;
        size_t line_map_length = 0u;
        const uint8_t *bytes = NULL;
        size_t bytes_length = 0u;
        if (cdin_frontend_protocol_array_get_raw(
                array, array_length, index, &line_map, &line_map_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_bytes(
                line_map, line_map_length, "bytes", &bytes, &bytes_length) !=
                CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        if (bytes_length > CDIN_FRONTEND_GUI_SDL_MAX_LINE_BYTES) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
        }
        if (bytes_length != 0u) {
            memcpy(model->pending.lines[index].bytes, bytes, bytes_length);
        }
        model->pending.lines[index].length = bytes_length;
    }
    model->pending_lines_valid = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static void optional_text(const cdin_frontend_protocol_message_view *message,
                          const char *field, char *output, size_t capacity)
{
    if (get_text(message, field, output, capacity) != CDIN_FRONTEND_PROTOCOL_OK) {
        output[0] = '\0';
    }
}

static cdin_frontend_gui_sdl_model_status apply_status(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *map = NULL;
    size_t map_length = 0u;
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    if (cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "status",
            &map, &map_length) == CDIN_FRONTEND_PROTOCOL_OK) {
        size_t length = 0u;
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "mode", staged->mode,
                sizeof(staged->mode), &length) != CDIN_FRONTEND_PROTOCOL_OK) {
            staged->mode[0] = '\0';
        }
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "text", staged->status,
                sizeof(staged->status), &length) != CDIN_FRONTEND_PROTOCOL_OK) {
            staged->status[0] = '\0';
        }
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
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
        if (model->has_snapshot_revision != 0 &&
            revision < model->snapshot_revision) {
            model->snapshot_pending_valid = 0;
            memset(&model->staged_snapshot, 0, sizeof(model->staged_snapshot));
            return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
        }
        if (model->snapshot_pending_valid) snapshot_commit(model);
        model->snapshot_revision = revision;
        model->has_snapshot_revision = 1;
        if (model->needs_lines && model->version != 0u) {
            action->request_lines = 1;
            action->first_line = 0u;
            action->line_count = (uint64_t)model->visible_line_capacity;
            action->version = model->version;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    if (scope_length == 8u && memcmp(scope, "response", 8u) == 0) {
        if (cdin_frontend_protocol_payload_get_uint(
                message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        if (model->pending_lines_valid) {
            model->visible = model->pending;
            model->pending_lines_valid = 0;
            model->needs_lines = 0;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_init(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    size_t visible_line_capacity)
{
    size_t length;
    if (model == NULL || view_id == NULL || visible_line_capacity == 0u ||
        visible_line_capacity > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    length = strlen(view_id);
    if (length == 0u || length >= sizeof(model->view_id)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    memset(model, 0, sizeof(*model));
    memcpy(model->view_id, view_id, length + 1u);
    model->visible_line_capacity = visible_line_capacity;
    model->needs_lines = 1;
    (void)snprintf(model->theme, sizeof(model->theme), "default");
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
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
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT:
            if (cdin_frontend_protocol_payload_get_uint(
                    message->payload, message->payload_length, "revision",
                    &model->staged_snapshot.layout_revision) == CDIN_FRONTEND_PROTOCOL_OK) {
                model->staged_snapshot.has_layout = 1;
            }
            break;
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
