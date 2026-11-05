/* Bounded one-view semantic adapter for the terminal frontend. */
#include "cdin/frontend/ui/tui/model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TUI_MODEL_MAX_LINES 512u
#define TUI_MODEL_MAX_LINE_BYTES (64u * 1024u)
#define TUI_MODEL_MAX_CACHE_BYTES (8u * 1024u * 1024u)
#define TUI_MODEL_MAX_GROUP_MESSAGES 256u
#define TUI_MODEL_MAX_GROUP_BYTES (1024u * 1024u)
#define TUI_MODEL_SCAN_LINES 256u
#define TUI_MODEL_TEXT_CAPACITY 256u

typedef struct owned_line {
    uint8_t *bytes;
    size_t length;
} owned_line;

typedef struct tui_data {
    char view_id[64];
    char theme[64];
    char status[TUI_MODEL_TEXT_CAPACITY];
    char mode[32];
    char prompt[TUI_MODEL_TEXT_CAPACITY];
    char notification[TUI_MODEL_TEXT_CAPACITY];
    cdin_frontend_tui_layout layout;
    int layout_present;
    uint64_t revision;
    uint64_t document_version;
    uint64_t first_line;
    uint64_t total_lines;
    uint64_t cache_start_byte_offset;
    uint64_t scan_next_line;
    uint64_t scan_byte_offset;
    uint64_t scan_target_offset;
    uint64_t cursor_position;
    uint64_t cursor_scroll_anchor;
    uint64_t selection_anchor;
    uint64_t selection_caret;
    int cursor_known;
    int scroll_anchor_known;
    int selection_known;
    int lines_invalidated;
    int scan_active;
    size_t line_count;
    size_t cache_bytes;
    owned_line lines[TUI_MODEL_MAX_LINES];
} tui_data;

typedef struct pending_message {
    cdin_frontend_protocol_message_type type;
    uint8_t *payload;
    size_t payload_length;
} pending_message;

struct cdin_frontend_tui_model {
    tui_data committed;
    tui_data staging;
    pending_message pending[TUI_MODEL_MAX_GROUP_MESSAGES];
    size_t pending_count;
    size_t pending_bytes;
    size_t visible_line_capacity;
    uint64_t last_flush_sequence;
    int request_pending;
    uint64_t request_first_line;
    uint64_t request_line_count;
    uint64_t request_version;
    cdin_frontend_logger *logger;
    cdin_frontend_tui_line frame_lines[TUI_MODEL_MAX_LINES];
};

static void free_lines(tui_data *data)
{
    size_t index;
    for (index = 0u; index < data->line_count; index++) {
        free(data->lines[index].bytes);
        data->lines[index].bytes = NULL;
        data->lines[index].length = 0u;
    }
    data->line_count = 0u;
    data->cache_bytes = 0u;
}

static void clear_data(tui_data *data, const char *view_id)
{
    free_lines(data);
    memset(data, 0, sizeof(*data));
    if (view_id != NULL) (void)snprintf(data->view_id, sizeof(data->view_id), "%s", view_id);
    data->lines_invalidated = 1;
}

static int clone_data(tui_data *destination, const tui_data *source)
{
    size_t index;
    clear_data(destination, source->view_id);
    destination->layout = source->layout;
    destination->layout_present = source->layout_present;
    destination->revision = source->revision;
    destination->document_version = source->document_version;
    destination->first_line = source->first_line;
    destination->total_lines = source->total_lines;
    destination->cache_start_byte_offset = source->cache_start_byte_offset;
    destination->scan_next_line = source->scan_next_line;
    destination->scan_byte_offset = source->scan_byte_offset;
    destination->scan_target_offset = source->scan_target_offset;
    destination->cursor_position = source->cursor_position;
    destination->cursor_scroll_anchor = source->cursor_scroll_anchor;
    destination->selection_anchor = source->selection_anchor;
    destination->selection_caret = source->selection_caret;
    destination->cursor_known = source->cursor_known;
    destination->scroll_anchor_known = source->scroll_anchor_known;
    destination->selection_known = source->selection_known;
    destination->lines_invalidated = source->lines_invalidated;
    destination->scan_active = source->scan_active;
    (void)snprintf(destination->theme, sizeof(destination->theme), "%s", source->theme);
    (void)snprintf(destination->status, sizeof(destination->status), "%s", source->status);
    (void)snprintf(destination->mode, sizeof(destination->mode), "%s", source->mode);
    (void)snprintf(destination->prompt, sizeof(destination->prompt), "%s", source->prompt);
    (void)snprintf(destination->notification, sizeof(destination->notification), "%s", source->notification);
    for (index = 0u; index < source->line_count; index++) {
        size_t length = source->lines[index].length;
        if (length > TUI_MODEL_MAX_CACHE_BYTES - destination->cache_bytes) {
            free_lines(destination);
            return -1;
        }
        destination->lines[index].bytes = NULL;
        if (length != 0u) {
            destination->lines[index].bytes = (uint8_t *)malloc(length);
            if (destination->lines[index].bytes == NULL) {
                free_lines(destination);
                return -1;
            }
            memcpy(destination->lines[index].bytes, source->lines[index].bytes, length);
        }
        destination->lines[index].length = length;
        destination->cache_bytes += length;
        destination->line_count++;
    }
    return 0;
}

static void discard_pending(cdin_frontend_tui_model *model)
{
    size_t index;
    for (index = 0u; index < model->pending_count; index++) {
        free(model->pending[index].payload);
        model->pending[index].payload = NULL;
        model->pending[index].payload_length = 0u;
    }
    model->pending_count = 0u;
    model->pending_bytes = 0u;
}

static cdin_frontend_tui_model_status get_text_field(
    const uint8_t *payload, size_t payload_length, const char *field,
    char *output, size_t capacity)
{
    size_t length = 0u;
    cdin_frontend_protocol_status status = cdin_frontend_protocol_payload_get_text(
        payload, payload_length, field, output, capacity, &length);
    if (status == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    if (status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) return CDIN_FRONTEND_TUI_MODEL_LIMIT_EXCEEDED;
    return status == CDIN_FRONTEND_PROTOCOL_OK ? CDIN_FRONTEND_TUI_MODEL_OK
                                                : CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
}

static int text_field_optional(const uint8_t *payload, size_t payload_length,
                               const char *field, char *output, size_t capacity)
{
    size_t length = 0u;
    cdin_frontend_protocol_status status = cdin_frontend_protocol_payload_get_text(
        payload, payload_length, field, output, capacity, &length);
    return status == CDIN_FRONTEND_PROTOCOL_OK ? 1 : 0;
}

static uint64_t line_bytes_total(const tui_data *data)
{
    size_t index;
    uint64_t total = 0u;
    for (index = 0u; index < data->line_count; index++) {
        if ((uint64_t)data->lines[index].length > UINT64_MAX - total) return UINT64_MAX;
        total += (uint64_t)data->lines[index].length;
    }
    return total;
}

static void begin_scroll_scan(tui_data *data, uint64_t next_line, uint64_t byte_offset)
{
    free_lines(data);
    data->first_line = next_line;
    data->cache_start_byte_offset = byte_offset;
    data->scan_next_line = next_line;
    data->scan_byte_offset = byte_offset;
    data->scan_target_offset = data->scroll_anchor_known ? data->cursor_scroll_anchor : 0u;
    data->scan_active = next_line != 0u || data->scan_target_offset > byte_offset;
    data->lines_invalidated = 1;
}

static cdin_frontend_tui_model_status apply_lines(cdin_frontend_tui_model *model,
                                                   tui_data *data,
                                                   const uint8_t *payload,
                                                   size_t payload_length)
{
    const uint8_t *array;
    size_t array_length;
    size_t count;
    size_t index;
    char view_id[64];
    uint64_t version;
    uint64_t first_line;
    uint64_t total_lines;
    uint64_t base_byte_offset;
    uint64_t page_end_offset;
    int has_total = 0;
    int was_scanning;
    cdin_frontend_protocol_status status;
    cdin_frontend_tui_model_status model_status;
    model_status = get_text_field(payload, payload_length, "view_id", view_id, sizeof(view_id));
    if (model_status != CDIN_FRONTEND_TUI_MODEL_OK) {
        if (model->logger != NULL) {
            CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                "view.lines has invalid view_id field (model status=%d)",
                (int)model_status);
        }
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (strcmp(view_id, data->view_id) != 0) {
        if (model->logger != NULL) {
            CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                "view.lines view mismatch: received=%s expected=%s",
                view_id, data->view_id);
        }
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    status = cdin_frontend_protocol_payload_get_uint(payload, payload_length, "version", &version);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines has invalid version (protocol status=%s)",
            cdin_frontend_protocol_status_name(status));
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    status = cdin_frontend_protocol_payload_get_uint(payload, payload_length, "first_line", &first_line);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines has invalid first_line (protocol status=%s)",
            cdin_frontend_protocol_status_name(status));
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    status = cdin_frontend_protocol_payload_get_raw(payload, payload_length, "lines", &array, &array_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines has invalid lines array (protocol status=%s)",
            cdin_frontend_protocol_status_name(status));
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (version < data->document_version) {
        if (model->logger != NULL) CDIN_FRONTEND_WARN(model->logger, "MODEL",
            "ignored stale view.lines version=%llu committed_document_version=%llu",
            (unsigned long long)version, (unsigned long long)data->document_version);
        return CDIN_FRONTEND_TUI_MODEL_STALE;
    }
    /* FE-005: a line window is usable only for the exact outstanding request.
     * This prevents late responses from replacing a newer viewport request. */
    if (!model->request_pending || version != model->request_version ||
        first_line != model->request_first_line) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines does not match outstanding request: pending=%d received_version=%llu requested_version=%llu received_first_line=%llu requested_first_line=%llu requested_line_count=%llu",
            model->request_pending, (unsigned long long)version,
            (unsigned long long)model->request_version,
            (unsigned long long)first_line,
            (unsigned long long)model->request_first_line,
            (unsigned long long)model->request_line_count);
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    status = cdin_frontend_protocol_array_count(array, array_length, &count);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || count > TUI_MODEL_MAX_LINES || count == 0u) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines count invalid: protocol_status=%s count=%llu capacity=%llu",
            cdin_frontend_protocol_status_name(status),
            (unsigned long long)(status == CDIN_FRONTEND_PROTOCOL_OK ? count : 0u),
            (unsigned long long)TUI_MODEL_MAX_LINES);
        return CDIN_FRONTEND_TUI_MODEL_LIMIT_EXCEEDED;
    }
    if (first_line > UINT64_MAX - (uint64_t)count ||
        (uint64_t)count > model->request_line_count) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines range exceeds request: first_line=%llu count=%llu requested_count=%llu",
            (unsigned long long)first_line, (unsigned long long)count,
            (unsigned long long)model->request_line_count);
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "total_lines", &total_lines) == CDIN_FRONTEND_PROTOCOL_OK) {
        if (total_lines == 0u || first_line >= total_lines || (uint64_t)count > total_lines - first_line) {
            if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                "view.lines total range invalid: first_line=%llu count=%llu total_lines=%llu",
                (unsigned long long)first_line, (unsigned long long)count,
                (unsigned long long)total_lines);
            return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        }
        has_total = 1;
    }
    was_scanning = data->scan_active;
    if (was_scanning && first_line != data->scan_next_line) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines scan offset mismatch: received_first_line=%llu scan_next_line=%llu scan_byte_offset=%llu",
            (unsigned long long)first_line, (unsigned long long)data->scan_next_line,
            (unsigned long long)data->scan_byte_offset);
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (!was_scanning && first_line != data->first_line) {
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL",
            "view.lines window offset mismatch: received_first_line=%llu cached_first_line=%llu",
            (unsigned long long)first_line, (unsigned long long)data->first_line);
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    base_byte_offset = was_scanning ? data->scan_byte_offset : data->cache_start_byte_offset;

    free_lines(data);
    data->first_line = first_line;
    data->cache_start_byte_offset = base_byte_offset;
    data->document_version = version;
    if (has_total) data->total_lines = total_lines;
    data->line_count = 0u;
    for (index = 0u; index < count; index++) {
        const uint8_t *line_map;
        size_t line_map_length;
        const uint8_t *bytes;
        size_t bytes_length;
        status = cdin_frontend_protocol_array_get_raw(array, array_length, index,
                                                      &line_map, &line_map_length);
        if (status != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_bytes(line_map, line_map_length, "bytes",
                                                     &bytes, &bytes_length) != CDIN_FRONTEND_PROTOCOL_OK) {
            free_lines(data);
            return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        }
        if (bytes_length > TUI_MODEL_MAX_LINE_BYTES || bytes_length > TUI_MODEL_MAX_CACHE_BYTES - data->cache_bytes) {
            free_lines(data);
            return CDIN_FRONTEND_TUI_MODEL_LIMIT_EXCEEDED;
        }
        if (bytes_length != 0u) {
            data->lines[index].bytes = (uint8_t *)malloc(bytes_length);
            if (data->lines[index].bytes == NULL) {
                free_lines(data);
                return CDIN_FRONTEND_TUI_MODEL_NO_MEMORY;
            }
            memcpy(data->lines[index].bytes, bytes, bytes_length);
        } else {
            data->lines[index].bytes = NULL;
        }
        data->lines[index].length = bytes_length;
        data->cache_bytes += bytes_length;
        data->line_count++;
    }

    page_end_offset = base_byte_offset + line_bytes_total(data);
    if (page_end_offset < base_byte_offset) return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    if (was_scanning) {
        uint64_t line_offset = base_byte_offset;
        int found = 0;
        size_t target_index = 0u;
        for (index = 0u; index < data->line_count; index++) {
            uint64_t line_end = line_offset + (uint64_t)data->lines[index].length;
            if (data->scan_target_offset < line_end ||
                (data->lines[index].length == 0u && data->scan_target_offset == line_offset)) {
                found = 1;
                target_index = index;
                break;
            }
            line_offset = line_end;
        }
        if (found || (has_total && first_line + (uint64_t)count >= total_lines)) {
            if (!found) target_index = data->line_count - 1u;
            line_offset = base_byte_offset;
            for (index = 0u; index < target_index; index++) {
                line_offset += (uint64_t)data->lines[index].length;
            }
            data->first_line = first_line + (uint64_t)target_index;
            data->cache_start_byte_offset = line_offset;
            data->scan_active = 0;
            data->lines_invalidated = 1;
            free_lines(data);
        } else {
            data->scan_next_line = first_line + (uint64_t)count;
            data->scan_byte_offset = page_end_offset;
            data->scan_active = 1;
            data->lines_invalidated = 1;
            free_lines(data);
        }
    } else {
        data->scan_active = 0;
        data->lines_invalidated = 0;
    }
    return CDIN_FRONTEND_TUI_MODEL_OK;
}

static cdin_frontend_tui_model_status apply_one(cdin_frontend_tui_model *model,
                                                 tui_data *data,
                                                 cdin_frontend_protocol_message_type type,
                                                 const uint8_t *payload,
                                                 size_t payload_length)
{
    char view_id[64];
    uint64_t version;
    switch (type) {
        case CDIN_FRONTEND_PROTOCOL_UI_OPTIONS:
            (void)text_field_optional(payload, payload_length, "theme", data->theme, sizeof(data->theme));
            return CDIN_FRONTEND_TUI_MODEL_OK;
        case CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE:
            if (get_text_field(payload, payload_length, "view_id", view_id, sizeof(view_id)) != CDIN_FRONTEND_TUI_MODEL_OK ||
                strcmp(view_id, data->view_id) != 0 ||
                cdin_frontend_protocol_payload_get_uint(payload, payload_length, "version", &version) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            if (version < data->document_version) return CDIN_FRONTEND_TUI_MODEL_STALE;
            data->document_version = version;
            data->scan_active = data->scroll_anchor_known && data->cursor_scroll_anchor != 0u;
            data->scan_next_line = 0u;
            data->scan_byte_offset = 0u;
            data->scan_target_offset = data->scan_active ? data->cursor_scroll_anchor : 0u;
            data->first_line = 0u;
            data->cache_start_byte_offset = 0u;
            data->lines_invalidated = 1;
            free_lines(data);
            return CDIN_FRONTEND_TUI_MODEL_OK;
        case CDIN_FRONTEND_PROTOCOL_VIEW_LINES:
            return apply_lines(model, data, payload, payload_length);
        case CDIN_FRONTEND_PROTOCOL_UI_STATUSLINE: {
            const uint8_t *status_map;
            size_t status_length;
            char value[TUI_MODEL_TEXT_CAPACITY];
            if (cdin_frontend_protocol_payload_get_raw(payload, payload_length, "status",
                                                       &status_map, &status_length) == CDIN_FRONTEND_PROTOCOL_OK) {
                if (text_field_optional(status_map, status_length, "mode", data->mode, sizeof(data->mode))) {
                    /* Mode is a display label supplied by Runtime; this layer
                     * never maps it to key bindings or editing policy. */
                }
                if (text_field_optional(status_map, status_length, "text", value, sizeof(value)) ||
                    text_field_optional(status_map, status_length, "message", value, sizeof(value)) ||
                    text_field_optional(status_map, status_length, "left", value, sizeof(value))) {
                    (void)snprintf(data->status, sizeof(data->status), "%s", value);
                }
            }
            return CDIN_FRONTEND_TUI_MODEL_OK;
        }
        case CDIN_FRONTEND_PROTOCOL_UI_PROMPT: {
            const uint8_t *prompt_map;
            size_t prompt_length;
            if (cdin_frontend_protocol_payload_get_raw(payload, payload_length, "prompt",
                                                       &prompt_map, &prompt_length) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            if (prompt_length == 1u && prompt_map[0] == 0xf6u) {
                data->prompt[0] = '\0';
            } else if (!text_field_optional(prompt_map, prompt_length, "text", data->prompt, sizeof(data->prompt)) &&
                       !text_field_optional(prompt_map, prompt_length, "message", data->prompt, sizeof(data->prompt))) {
                data->prompt[0] = '\0';
            }
            return CDIN_FRONTEND_TUI_MODEL_OK;
        }
        case CDIN_FRONTEND_PROTOCOL_UI_NOTIFY:
            (void)text_field_optional(payload, payload_length, "text", data->notification, sizeof(data->notification));
            if (data->notification[0] != '\0') (void)snprintf(data->status, sizeof(data->status), "%s", data->notification);
            return CDIN_FRONTEND_TUI_MODEL_OK;
        case CDIN_FRONTEND_PROTOCOL_ERROR: {
            char code[64];
            char message[TUI_MODEL_TEXT_CAPACITY];
            if (text_field_optional(payload, payload_length, "code", code, sizeof(code)) &&
                text_field_optional(payload, payload_length, "message", message, sizeof(message))) {
                (void)snprintf(data->status, sizeof(data->status), "%.48s: %.190s", code, message);
            }
            return CDIN_FRONTEND_TUI_MODEL_OK;
        }
        case CDIN_FRONTEND_PROTOCOL_UI_CURSOR:
            if (get_text_field(payload, payload_length, "view_id", view_id, sizeof(view_id)) != CDIN_FRONTEND_TUI_MODEL_OK ||
                strcmp(view_id, data->view_id) != 0 ||
                cdin_frontend_protocol_payload_get_uint(payload, payload_length, "version", &version) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            if (version < data->document_version) return CDIN_FRONTEND_TUI_MODEL_STALE;
            if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "position", &data->cursor_position) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            data->document_version = version;
            data->cursor_known = 1;
            {
                uint64_t previous_anchor = data->cursor_scroll_anchor;
                int had_anchor = data->scroll_anchor_known;
                data->scroll_anchor_known = cdin_frontend_protocol_payload_get_uint(
                    payload, payload_length, "scroll_anchor", &data->cursor_scroll_anchor) == CDIN_FRONTEND_PROTOCOL_OK;
                if (data->scroll_anchor_known && (!had_anchor || previous_anchor != data->cursor_scroll_anchor)) {
                    uint64_t cache_end = data->cache_start_byte_offset + line_bytes_total(data);
                    if (data->line_count != 0u && data->cursor_scroll_anchor >= data->cache_start_byte_offset &&
                        data->cursor_scroll_anchor <= cache_end) {
                        uint64_t line_offset = data->cache_start_byte_offset;
                        for (size_t line_index = 0u; line_index < data->line_count; line_index++) {
                            uint64_t line_end = line_offset + (uint64_t)data->lines[line_index].length;
                            if (data->cursor_scroll_anchor < line_end ||
                                (data->lines[line_index].length == 0u && data->cursor_scroll_anchor == line_offset) ||
                                (line_index + 1u == data->line_count && data->cursor_scroll_anchor == line_end &&
                                 data->first_line + (uint64_t)data->line_count >= data->total_lines)) {
                                data->first_line += (uint64_t)line_index;
                                data->cache_start_byte_offset = line_offset;
                                data->scan_active = 0;
                                data->lines_invalidated = 1;
                                free_lines(data);
                                break;
                            }
                            line_offset = line_end;
                        }
                        if (!data->lines_invalidated && data->cursor_scroll_anchor == cache_end &&
                            data->first_line + (uint64_t)data->line_count < data->total_lines) {
                            begin_scroll_scan(data, data->first_line + (uint64_t)data->line_count, cache_end);
                        }
                    } else if (data->cursor_scroll_anchor < data->cache_start_byte_offset) {
                        begin_scroll_scan(data, 0u, 0u);
                    } else {
                        begin_scroll_scan(data, data->first_line + (uint64_t)data->line_count, cache_end);
                    }
                }
            }
            return CDIN_FRONTEND_TUI_MODEL_OK;
        case CDIN_FRONTEND_PROTOCOL_UI_SELECTION:
            if (get_text_field(payload, payload_length, "view_id", view_id, sizeof(view_id)) != CDIN_FRONTEND_TUI_MODEL_OK ||
                strcmp(view_id, data->view_id) != 0 ||
                cdin_frontend_protocol_payload_get_uint(payload, payload_length, "version", &version) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            if (version < data->document_version) return CDIN_FRONTEND_TUI_MODEL_STALE;
            if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "anchor", &data->selection_anchor) != CDIN_FRONTEND_PROTOCOL_OK ||
                cdin_frontend_protocol_payload_get_uint(payload, payload_length, "caret", &data->selection_caret) != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            data->document_version = version;
            data->selection_known = 1;
            return CDIN_FRONTEND_TUI_MODEL_OK;
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT: {
            cdin_frontend_tui_layout parsed;
            cdin_frontend_tui_layout_status layout_status =
                cdin_frontend_tui_layout_parse(&parsed, payload, payload_length);
            if (layout_status != CDIN_FRONTEND_TUI_LAYOUT_OK) {
                return layout_status == CDIN_FRONTEND_TUI_LAYOUT_LIMIT_EXCEEDED
                    ? CDIN_FRONTEND_TUI_MODEL_LIMIT_EXCEEDED
                    : CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
            }
            if (data->layout_present && parsed.revision < data->layout.revision) {
                return CDIN_FRONTEND_TUI_MODEL_STALE;
            }
            data->layout = parsed;
            data->layout_present = 1;
            return CDIN_FRONTEND_TUI_MODEL_OK;
        }
        case CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE:
        case CDIN_FRONTEND_PROTOCOL_UI_PANEL:
            /* Style roles and declarative panel rendering remain separate T3
             * work. The bounded PaneTree is retained transactionally above. */
            return CDIN_FRONTEND_TUI_MODEL_OK;
        default:
            return CDIN_FRONTEND_TUI_MODEL_OK;
    }
}

static cdin_frontend_tui_model_status commit_group(cdin_frontend_tui_model *model,
                                                    const cdin_frontend_protocol_message_view *message)
{
    char scope[16];
    size_t scope_length = 0u;
    uint64_t sequence;
    uint64_t revision = 0u;
    int snapshot;
    int lines_applied = 0;
    size_t index;
    cdin_frontend_protocol_status wire_status;
    if (cdin_frontend_protocol_payload_get_text(message->payload, message->payload_length,
                                                "scope", scope, sizeof(scope), &scope_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        if (model->logger != NULL) {
            CDIN_FRONTEND_ERROR(model->logger, "MODEL", "UI flush has no valid scope");
        }
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (cdin_frontend_protocol_payload_get_uint(message->payload, message->payload_length,
                                                "sequence", &sequence) != CDIN_FRONTEND_PROTOCOL_OK) {
        if (model->logger != NULL) {
            CDIN_FRONTEND_ERROR(model->logger, "MODEL", "UI flush has no valid sequence");
        }
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (sequence <= model->last_flush_sequence) {
        if (model->logger != NULL) {
            CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                "UI flush sequence=%llu is not newer than committed sequence=%llu",
                (unsigned long long)sequence, (unsigned long long)model->last_flush_sequence);
        }
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    snapshot = scope_length == 8u && memcmp(scope, "snapshot", 8u) == 0;
    if (!snapshot && !(scope_length == 8u && memcmp(scope, "response", 8u) == 0)) {
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    wire_status = cdin_frontend_protocol_payload_get_uint(message->payload, message->payload_length,
                                                          "revision", &revision);
    if (snapshot) {
        if (wire_status != CDIN_FRONTEND_PROTOCOL_OK) {
            if (model->logger != NULL) {
                CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                    "snapshot flush sequence=%llu has no valid revision",
                    (unsigned long long)sequence);
            }
            return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        }
        if (model->committed.revision != 0u && revision <= model->committed.revision) {
            if (model->logger != NULL) {
                CDIN_FRONTEND_WARN(model->logger, "MODEL",
                    "ignored stale snapshot flush sequence=%llu revision=%llu committed_revision=%llu",
                    (unsigned long long)sequence, (unsigned long long)revision,
                    (unsigned long long)model->committed.revision);
            }
            return CDIN_FRONTEND_TUI_MODEL_STALE;
        }
        clear_data(&model->staging, model->committed.view_id);
    } else {
        if (wire_status != CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
            if (model->logger != NULL) {
                CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                    "response flush sequence=%llu unexpectedly contains a revision field",
                    (unsigned long long)sequence);
            }
            return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        }
        if (clone_data(&model->staging, &model->committed) != 0) {
            return CDIN_FRONTEND_TUI_MODEL_NO_MEMORY;
        }
    }
    for (index = 0u; index < model->pending_count; index++) {
        if (!snapshot && model->pending[index].type == CDIN_FRONTEND_PROTOCOL_UI_LAYOUT) {
            clear_data(&model->staging, model->committed.view_id);
            return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        }
        cdin_frontend_tui_model_status status = apply_one(model, &model->staging,
            model->pending[index].type, model->pending[index].payload,
            model->pending[index].payload_length);
        if (status == CDIN_FRONTEND_TUI_MODEL_STALE) continue;
        if (status != CDIN_FRONTEND_TUI_MODEL_OK) {
            if (model->logger != NULL) {
                const char *name = cdin_frontend_protocol_message_name(
                    (cdin_frontend_protocol_message_type)model->pending[index].type);
                CDIN_FRONTEND_ERROR(model->logger, "MODEL",
                    "flush sequence=%llu scope=%s revision=%llu rejected pending message type=%u name=%s status=%d",
                    (unsigned long long)sequence, snapshot ? "snapshot" : "response",
                    (unsigned long long)revision, model->pending[index].type,
                    name != NULL ? name : "unknown", (int)status);
            }
            clear_data(&model->staging, model->committed.view_id);
            return status;
        }
        if (model->pending[index].type == CDIN_FRONTEND_PROTOCOL_VIEW_LINES) {
            lines_applied = 1;
        }
    }
    if (snapshot) model->staging.revision = revision;
    {
        tui_data old = model->committed;
        model->committed = model->staging;
        model->staging = old;
    }
    model->last_flush_sequence = sequence;
    if (lines_applied || model->committed.lines_invalidated) model->request_pending = 0;
    if (model->logger != NULL) {
        CDIN_FRONTEND_DEBUG(model->logger, "MODEL", "committed %s flush sequence=%llu revision=%llu",
                            snapshot ? "snapshot" : "response",
                            (unsigned long long)sequence,
                            (unsigned long long)model->committed.revision);
    }
    return CDIN_FRONTEND_TUI_MODEL_OK;
}

cdin_frontend_tui_model *cdin_frontend_tui_model_create(
    const char *view_id, size_t visible_line_capacity, cdin_frontend_logger *logger)
{
    cdin_frontend_tui_model *model;
    size_t length;
    if (view_id == NULL || view_id[0] == '\0' || visible_line_capacity == 0u ||
        visible_line_capacity > TUI_MODEL_MAX_LINES) return NULL;
    length = strlen(view_id);
    if (length >= 64u) return NULL;
    model = (cdin_frontend_tui_model *)calloc(1u, sizeof(*model));
    if (model == NULL) return NULL;
    (void)snprintf(model->committed.view_id, sizeof(model->committed.view_id), "%s", view_id);
    (void)snprintf(model->staging.view_id, sizeof(model->staging.view_id), "%s", view_id);
    model->visible_line_capacity = visible_line_capacity;
    model->logger = logger;
    model->committed.lines_invalidated = 1;
    return model;
}

void cdin_frontend_tui_model_destroy(cdin_frontend_tui_model *model)
{
    if (model == NULL) return;
    discard_pending(model);
    free_lines(&model->committed);
    free_lines(&model->staging);
    free(model);
}

cdin_frontend_tui_model_status cdin_frontend_tui_model_apply_message(
    cdin_frontend_tui_model *model,
    const cdin_frontend_protocol_message_view *message,
    int *out_committed)
{
    pending_message *pending;
    uint8_t *copy;
    cdin_frontend_tui_model_status status;
    if (model == NULL || message == NULL || out_committed == NULL ||
        message->payload == NULL || message->payload_length == 0u) return CDIN_FRONTEND_TUI_MODEL_INVALID_ARGUMENT;
    *out_committed = 0;
    if (message->type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH) {
        status = commit_group(model, message);
        if (status == CDIN_FRONTEND_TUI_MODEL_OK) {
            *out_committed = 1;
        } else if (status != CDIN_FRONTEND_TUI_MODEL_STALE) {
            /* A stale snapshot is a no-op: it must not invalidate a pending
             * version-pinned view.lines request. That response may already
             * be queued behind this older snapshot flush. Invalid groups
             * still clear the request so the adapter can recover explicitly. */
            model->request_pending = 0;
        }
        discard_pending(model);
        return status;
    }
    if (model->pending_count >= TUI_MODEL_MAX_GROUP_MESSAGES ||
        message->payload_length > TUI_MODEL_MAX_GROUP_BYTES - model->pending_bytes) {
        discard_pending(model);
        if (model->logger != NULL) CDIN_FRONTEND_ERROR(model->logger, "MODEL", "UI flush group exceeded its bounded staging capacity");
        return CDIN_FRONTEND_TUI_MODEL_LIMIT_EXCEEDED;
    }
    copy = (uint8_t *)malloc(message->payload_length);
    if (copy == NULL) return CDIN_FRONTEND_TUI_MODEL_NO_MEMORY;
    memcpy(copy, message->payload, message->payload_length);
    pending = &model->pending[model->pending_count++];
    pending->type = message->type;
    pending->payload = copy;
    pending->payload_length = message->payload_length;
    model->pending_bytes += message->payload_length;
    return CDIN_FRONTEND_TUI_MODEL_OK;
}

cdin_frontend_tui_model_status cdin_frontend_tui_model_get_frame(
    cdin_frontend_tui_model *model, cdin_frontend_tui_frame *out_frame)
{
    size_t index;
    uint64_t cursor_offset = 0u;
    uint64_t cursor_line = model != NULL ? model->committed.first_line : 0u;
    uint64_t cursor_byte = 0u;
    if (model == NULL || out_frame == NULL) return CDIN_FRONTEND_TUI_MODEL_INVALID_ARGUMENT;
    for (index = 0u; index < model->committed.line_count; index++) {
        model->frame_lines[index].bytes = model->committed.lines[index].bytes;
        model->frame_lines[index].length = model->committed.lines[index].length;
    }
    if (model->committed.cursor_known && model->committed.scroll_anchor_known &&
        model->committed.cursor_position >= model->committed.cursor_scroll_anchor) {
        cursor_offset = model->committed.cursor_position >= model->committed.cache_start_byte_offset
            ? model->committed.cursor_position - model->committed.cache_start_byte_offset : 0u;
        for (index = 0u; index < model->committed.line_count; index++) {
            size_t line_length = model->committed.lines[index].length;
            if (cursor_offset < (uint64_t)line_length ||
                (index + 1u == model->committed.line_count && cursor_offset <= (uint64_t)line_length)) {
                cursor_line = model->committed.first_line + (uint64_t)index;
                cursor_byte = cursor_offset;
                break;
            }
            cursor_offset -= (uint64_t)line_length;
            cursor_line = model->committed.first_line + (uint64_t)index;
            cursor_byte = (uint64_t)line_length;
        }
    }
    memset(out_frame, 0, sizeof(*out_frame));
    out_frame->title = "CDIN";
    out_frame->mode = model->committed.mode[0] != '\0' ? model->committed.mode : "";
    out_frame->status = model->committed.notification[0] != '\0' ?
        model->committed.notification : model->committed.status;
    out_frame->prompt = model->committed.prompt;
    out_frame->lines = model->frame_lines;
    out_frame->line_count = model->committed.line_count;
    out_frame->cursor_line = cursor_line;
    out_frame->cursor_byte = cursor_byte;
    out_frame->top_line = model->committed.first_line;
    out_frame->first_line_byte_offset = model->committed.cache_start_byte_offset;
    out_frame->selection_anchor = model->committed.selection_anchor;
    out_frame->selection_caret = model->committed.selection_caret;
    out_frame->selection_known = model->committed.selection_known;
    out_frame->document_version = model->committed.document_version;
    out_frame->show_line_numbers = 1;
    return CDIN_FRONTEND_TUI_MODEL_OK;
}

int cdin_frontend_tui_model_needs_lines(const cdin_frontend_tui_model *model)
{
    return model != NULL && model->committed.lines_invalidated && !model->request_pending;
}

cdin_frontend_tui_model_status cdin_frontend_tui_model_request_range(
    cdin_frontend_tui_model *model, uint64_t *out_first_line,
    uint64_t *out_line_count, uint64_t *out_version)
{
    if (model == NULL || out_first_line == NULL || out_line_count == NULL || out_version == NULL) {
        return CDIN_FRONTEND_TUI_MODEL_INVALID_ARGUMENT;
    }
    if (model->request_pending) return CDIN_FRONTEND_TUI_MODEL_STALE;
    if (model->committed.scan_active) {
        uint64_t remaining = model->committed.total_lines > model->committed.scan_next_line
            ? model->committed.total_lines - model->committed.scan_next_line : TUI_MODEL_SCAN_LINES;
        *out_first_line = model->committed.scan_next_line;
        *out_line_count = remaining < TUI_MODEL_SCAN_LINES ? remaining : TUI_MODEL_SCAN_LINES;
        if (*out_line_count == 0u) *out_line_count = 1u;
    } else {
        *out_first_line = model->committed.first_line;
        *out_line_count = (uint64_t)model->visible_line_capacity;
    }
    *out_version = model->committed.document_version;
    model->request_first_line = *out_first_line;
    model->request_line_count = *out_line_count;
    model->request_version = *out_version;
    return CDIN_FRONTEND_TUI_MODEL_OK;
}

void cdin_frontend_tui_model_mark_request_pending(cdin_frontend_tui_model *model)
{
    if (model != NULL && model->request_line_count != 0u) model->request_pending = 1;
}

void cdin_frontend_tui_model_request_failed(cdin_frontend_tui_model *model)
{
    if (model != NULL) model->request_pending = 0;
}

uint64_t cdin_frontend_tui_model_revision(const cdin_frontend_tui_model *model)
{
    return model != NULL ? model->committed.revision : 0u;
}

uint64_t cdin_frontend_tui_model_last_flush(const cdin_frontend_tui_model *model)
{
    return model != NULL ? model->last_flush_sequence : 0u;
}

const cdin_frontend_tui_layout *cdin_frontend_tui_model_get_layout(
    const cdin_frontend_tui_model *model)
{
    return model != NULL && model->committed.layout_present
        ? &model->committed.layout : NULL;
}
