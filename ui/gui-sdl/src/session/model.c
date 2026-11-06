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

static cdin_frontend_gui_sdl_model_status apply_invalidate(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    uint64_t version = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
        CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < model->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    model->pending_version = version;
    model->needs_lines = 1;
    model->pending_lines_valid = 0;
    model->pending_cursor_valid = 0;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_cursor(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    uint64_t version = 0u;
    uint64_t position = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "position", &position) !=
            CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < model->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    model->pending_cursor_position = position;
    model->pending_cursor_valid = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_selection(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
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
    if (version < model->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    model->selection_anchor = anchor;
    model->selection_caret = caret;
    model->has_selection = 1;
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
    if (cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "status",
            &map, &map_length) == CDIN_FRONTEND_PROTOCOL_OK) {
        size_t length = 0u;
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "mode", model->mode,
                sizeof(model->mode), &length) != CDIN_FRONTEND_PROTOCOL_OK) {
            model->mode[0] = '\0';
        }
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "text", model->status,
                sizeof(model->status), &length) != CDIN_FRONTEND_PROTOCOL_OK) {
            model->status[0] = '\0';
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
        if (model->pending_version != 0u) {
            if (model->pending_version != model->version) {
                memset(&model->visible, 0, sizeof(model->visible));
                model->version = model->pending_version;
                model->has_cursor = 0;
                model->has_selection = 0;
            }
            model->pending_version = 0u;
        }
        if (model->pending_cursor_valid) {
            model->cursor_position = model->pending_cursor_position;
            model->has_cursor = 1;
            model->pending_cursor_valid = 0;
        }
        if (model->needs_lines && model->version != 0u) {
            action->request_lines = 1;
            action->first_line = 0u;
            action->line_count = (uint64_t)model->visible_line_capacity;
            action->version = model->version;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    if (scope_length == 8u && memcmp(scope, "response", 8u) == 0) {
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
            optional_text(message, "theme", model->theme, sizeof(model->theme));
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT:
            if (cdin_frontend_protocol_payload_get_uint(
                    message->payload, message->payload_length, "revision",
                    &model->layout_revision) == CDIN_FRONTEND_PROTOCOL_OK) {
                model->has_layout = 1;
            }
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_NOTIFY:
            optional_text(message, "text", model->notification,
                          sizeof(model->notification));
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_FLUSH:
            status = apply_flush(model, message, out_action);
            break;
        default:
            break;
    }
    return status;
}
