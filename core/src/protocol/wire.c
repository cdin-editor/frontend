/*
 * Frontend-owned deterministic-CBOR codec for CDIN Frontend Protocol 1.0.0.
 * It is intentionally self-contained and links neither Backend nor Kernel.
 */
#include "cdin/frontend/core/protocol/wire.h"

#include <limits.h>
#include <string.h>

typedef struct cbor_cursor {
    const uint8_t *data;
    size_t length;
    size_t position;
} cbor_cursor;

typedef struct cbor_head {
    uint8_t major;
    uint64_t value;
} cbor_head;

typedef enum cbor_value_kind {
    CBOR_KIND_UINT,
    CBOR_KIND_TEXT,
    CBOR_KIND_BYTES,
    CBOR_KIND_BOOL,
    CBOR_KIND_MAP,
    CBOR_KIND_ARRAY,
    CBOR_KIND_NULL
} cbor_value_kind;

static int valid_utf8(const uint8_t *data, size_t length)
{
    size_t i = 0u;
    while (i < length) {
        uint8_t first = data[i];
        size_t extra;
        uint32_t codepoint;
        uint32_t minimum;
        size_t j;
        if (first <= 0x7fu) {
            i++;
            continue;
        }
        if (first >= 0xc2u && first <= 0xdfu) {
            extra = 1u;
            codepoint = (uint32_t)(first & 0x1fu);
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            extra = 2u;
            codepoint = (uint32_t)(first & 0x0fu);
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            extra = 3u;
            codepoint = (uint32_t)(first & 0x07u);
            minimum = 0x10000u;
        } else {
            return 0;
        }
        if (extra > length - i - 1u) return 0;
        for (j = 1u; j <= extra; j++) {
            uint8_t next = data[i + j];
            if ((next & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6u) | (uint32_t)(next & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) return 0;
        i += extra + 1u;
    }
    return 1;
}

static cdin_frontend_protocol_status read_head(cbor_cursor *cursor, cbor_head *out)
{
    uint8_t initial;
    uint8_t additional;
    uint64_t value;
    size_t count;
    size_t i;
    if (cursor == NULL || out == NULL || cursor->position >= cursor->length) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    initial = cursor->data[cursor->position++];
    out->major = (uint8_t)(initial >> 5u);
    additional = (uint8_t)(initial & 0x1fu);
    if (additional < 24u) {
        out->value = additional;
        return CDIN_FRONTEND_PROTOCOL_OK;
    }
    if (additional == 24u) count = 1u;
    else if (additional == 25u) count = 2u;
    else if (additional == 26u) count = 4u;
    else if (additional == 27u) count = 8u;
    else return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    if (count > cursor->length - cursor->position) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    value = 0u;
    for (i = 0u; i < count; i++) {
        value = (value << 8u) | cursor->data[cursor->position++];
    }
    if ((count == 1u && value < 24u) ||
        (count == 2u && value <= UINT8_MAX) ||
        (count == 4u && value <= UINT16_MAX) ||
        (count == 8u && value <= UINT32_MAX)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    out->value = value;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static cdin_frontend_protocol_status read_text(cbor_cursor *cursor,
                                                const uint8_t **out_text,
                                                size_t *out_length)
{
    cbor_head head;
    cdin_frontend_protocol_status status;
    if (out_text == NULL || out_length == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = read_head(cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (head.major != 3u || head.value > SIZE_MAX ||
        (size_t)head.value > cursor->length - cursor->position) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    *out_text = cursor->data + cursor->position;
    *out_length = (size_t)head.value;
    if (!valid_utf8(*out_text, *out_length)) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    cursor->position += *out_length;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static cdin_frontend_protocol_status skip_item(cbor_cursor *cursor, unsigned depth);

static cdin_frontend_protocol_status validate_map(cbor_cursor *cursor,
                                                   uint64_t entries,
                                                   unsigned depth)
{
    size_t previous_key_start = 0u;
    size_t previous_key_encoded_length = 0u;
    uint64_t i;
    if (depth > CDIN_FRONTEND_PROTOCOL_MAX_DEPTH) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    if (entries > 4096u) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    for (i = 0u; i < entries; i++) {
        const uint8_t *key;
        size_t key_length;
        size_t key_start = cursor->position;
        size_t key_encoded_length;
        cdin_frontend_protocol_status status = read_text(cursor, &key, &key_length);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
        key_encoded_length = cursor->position - key_start;
        if (i != 0u) {
            const uint8_t *previous_key = cursor->data + previous_key_start;
            const uint8_t *current_key = cursor->data + key_start;
            size_t compare_length;
            int order;
            if (key_encoded_length < previous_key_encoded_length) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            compare_length = key_encoded_length < previous_key_encoded_length
                                 ? key_encoded_length : previous_key_encoded_length;
            order = memcmp(previous_key, current_key, compare_length);
            if (key_encoded_length == previous_key_encoded_length && order >= 0) {
                return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            }
            if (key_encoded_length > previous_key_encoded_length && order > 0) {
                return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            }
        }
        previous_key_start = key_start;
        previous_key_encoded_length = key_encoded_length;
        status = skip_item(cursor, depth + 1u);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    }
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static cdin_frontend_protocol_status skip_item(cbor_cursor *cursor, unsigned depth)
{
    cbor_head head;
    uint64_t i;
    cdin_frontend_protocol_status status;
    if (depth > CDIN_FRONTEND_PROTOCOL_MAX_DEPTH) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    status = read_head(cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    switch (head.major) {
        case 0u: /* unsigned integer */
        case 1u: /* negative integer; unknown optional values may use it */
            return CDIN_FRONTEND_PROTOCOL_OK;
        case 2u: /* byte string */
        case 3u: /* UTF-8 text */
            if (head.value > SIZE_MAX || (size_t)head.value > cursor->length - cursor->position) {
                return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            }
            if (head.major == 3u &&
                !valid_utf8(cursor->data + cursor->position, (size_t)head.value)) {
                return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            }
            cursor->position += (size_t)head.value;
            return CDIN_FRONTEND_PROTOCOL_OK;
        case 4u:
            if (head.value > cursor->length) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            for (i = 0u; i < head.value; i++) {
                status = skip_item(cursor, depth + 1u);
                if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
            }
            return CDIN_FRONTEND_PROTOCOL_OK;
        case 5u:
            return validate_map(cursor, head.value, depth);
        case 6u: /* tags are legal CBOR values; the tagged value is still validated */
            return skip_item(cursor, depth + 1u);
        case 7u: /* only bool and null are admitted; all floats/simple values reject */
            if (head.value == 20u || head.value == 21u || head.value == 22u) {
                return CDIN_FRONTEND_PROTOCOL_OK;
            }
            return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        default:
            return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
}

static cdin_frontend_protocol_status validate_map_payload(const uint8_t *payload,
                                                           size_t payload_length)
{
    cbor_cursor cursor;
    cbor_head head;
    cdin_frontend_protocol_status status;
    if (payload == NULL || payload_length == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    cursor.data = payload;
    cursor.length = payload_length;
    cursor.position = 0u;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (head.major != 5u) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    status = validate_map(&cursor, head.value, 1u);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    return cursor.position == cursor.length ? CDIN_FRONTEND_PROTOCOL_OK
                                           : CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
}

static cdin_frontend_protocol_status find_value(const uint8_t *payload,
                                                size_t payload_length,
                                                const char *field,
                                                const uint8_t **out_value,
                                                size_t *out_value_length)
{
    cbor_cursor cursor;
    cbor_head head;
    size_t field_length;
    uint64_t i;
    cdin_frontend_protocol_status status;
    if (payload == NULL || field == NULL || out_value == NULL || out_value_length == NULL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    status = validate_map_payload(payload, payload_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    cursor.data = payload;
    cursor.length = payload_length;
    cursor.position = 0u;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    field_length = strlen(field);
    for (i = 0u; i < head.value; i++) {
        const uint8_t *key;
        size_t key_length;
        size_t value_start;
        status = read_text(&cursor, &key, &key_length);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
        value_start = cursor.position;
        status = skip_item(&cursor, 1u);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
        if (key_length == field_length && memcmp(key, field, field_length) == 0) {
            *out_value = payload + value_start;
            *out_value_length = cursor.position - value_start;
            return CDIN_FRONTEND_PROTOCOL_OK;
        }
    }
    return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
}

static cdin_frontend_protocol_status parse_uint_value(const uint8_t *value,
                                                       size_t value_length,
                                                       uint64_t *out_value)
{
    cbor_cursor cursor = { value, value_length, 0u };
    cbor_head head;
    cdin_frontend_protocol_status status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (head.major != 0u || cursor.position != cursor.length) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    *out_value = head.value;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static cdin_frontend_protocol_status parse_text_value(const uint8_t *value,
                                                       size_t value_length,
                                                       const uint8_t **out_text,
                                                       size_t *out_length)
{
    cbor_cursor cursor = { value, value_length, 0u };
    cdin_frontend_protocol_status status = read_text(&cursor, out_text, out_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    return cursor.position == cursor.length ? CDIN_FRONTEND_PROTOCOL_OK
                                            : CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
}

static cdin_frontend_protocol_status parse_bytes_value(const uint8_t *value,
                                                        size_t value_length,
                                                        const uint8_t **out_bytes,
                                                        size_t *out_length)
{
    cbor_cursor cursor = { value, value_length, 0u };
    cbor_head head;
    cdin_frontend_protocol_status status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (head.major != 2u || head.value > SIZE_MAX ||
        (size_t)head.value != cursor.length - cursor.position) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    *out_bytes = cursor.data + cursor.position;
    *out_length = (size_t)head.value;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static cdin_frontend_protocol_status parse_bool_value(const uint8_t *value,
                                                       size_t value_length,
                                                       int *out_value)
{
    cbor_cursor cursor = { value, value_length, 0u };
    cbor_head head;
    cdin_frontend_protocol_status status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (head.major != 7u || (head.value != 20u && head.value != 21u) ||
        cursor.position != cursor.length) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    *out_value = head.value == 21u ? 1 : 0;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

const char *cdin_frontend_protocol_status_name(cdin_frontend_protocol_status status)
{
    switch (status) {
        case CDIN_FRONTEND_PROTOCOL_OK: return "OK";
        case CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case CDIN_FRONTEND_PROTOCOL_INVALID_FRAME: return "INVALID_FRAME";
        case CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA: return "INVALID_SCHEMA";
        case CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH: return "VERSION_MISMATCH";
        case CDIN_FRONTEND_PROTOCOL_UNSUPPORTED: return "UNSUPPORTED";
        case CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED: return "LIMIT_EXCEEDED";
        case CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case CDIN_FRONTEND_PROTOCOL_IO_ERROR: return "IO_ERROR";
        case CDIN_FRONTEND_PROTOCOL_EOF: return "EOF";
        case CDIN_FRONTEND_PROTOCOL_AGAIN: return "AGAIN";
        default: return "UNKNOWN";
    }
}

const char *cdin_frontend_protocol_message_name(cdin_frontend_protocol_message_type type)
{
    switch (type) {
        case CDIN_FRONTEND_PROTOCOL_UI_OPTIONS: return "ui.options";
        case CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE: return "ui.style_table";
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT: return "ui.layout";
        case CDIN_FRONTEND_PROTOCOL_VIEW_LINES: return "view.lines";
        case CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE: return "view.invalidate";
        case CDIN_FRONTEND_PROTOCOL_UI_STATUSLINE: return "ui.statusline";
        case CDIN_FRONTEND_PROTOCOL_UI_PROMPT: return "ui.prompt";
        case CDIN_FRONTEND_PROTOCOL_UI_PANEL: return "ui.panel";
        case CDIN_FRONTEND_PROTOCOL_UI_NOTIFY: return "ui.notify";
        case CDIN_FRONTEND_PROTOCOL_REQUEST_CLIPBOARD_GET: return "request.clipboard_get";
        case CDIN_FRONTEND_PROTOCOL_REQUEST_CLIPBOARD_SET: return "request.clipboard_set";
        case CDIN_FRONTEND_PROTOCOL_REQUEST_DIALOG: return "request.dialog";
        case CDIN_FRONTEND_PROTOCOL_ERROR: return "error";
        case CDIN_FRONTEND_PROTOCOL_UI_CURSOR: return "view.cursor";
        case CDIN_FRONTEND_PROTOCOL_UI_SELECTION: return "view.selection";
        case CDIN_FRONTEND_PROTOCOL_UI_FLUSH: return "ui.flush";
        case CDIN_FRONTEND_PROTOCOL_HELLO_ACK: return "hello_ack";
        case CDIN_FRONTEND_PROTOCOL_DETACH_ACK: return "detach_ack";
        case CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED: return "driver.role_changed";
        case CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT: return "pane.splitter.result";
        case CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE: return "pane.splitter.resize";
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START: return "ime.composition.start";
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE: return "ime.composition.update";
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL: return "ime.composition.cancel";
        case CDIN_FRONTEND_PROTOCOL_HELLO: return "hello";
        case CDIN_FRONTEND_PROTOCOL_ATTACH: return "attach";
        case CDIN_FRONTEND_PROTOCOL_DETACH: return "detach";
        case CDIN_FRONTEND_PROTOCOL_INPUT_KEY: return "input.key";
        case CDIN_FRONTEND_PROTOCOL_INPUT_TEXT: return "input.text";
        case CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE: return "input.mouse";
        case CDIN_FRONTEND_PROTOCOL_INPUT_PASTE: return "input.paste";
        case CDIN_FRONTEND_PROTOCOL_WINDOW_RESIZE: return "window.resize";
        case CDIN_FRONTEND_PROTOCOL_FOCUS: return "focus";
        case CDIN_FRONTEND_PROTOCOL_VIEW_REQUEST_LINES: return "view.request_lines";
        case CDIN_FRONTEND_PROTOCOL_VIEW_SET_POSITION: return "view.set_position";
        case CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND: return "request.command";
        case CDIN_FRONTEND_PROTOCOL_RESPONSE_CLIPBOARD: return "response.clipboard";
        case CDIN_FRONTEND_PROTOCOL_RESPONSE_DIALOG: return "response.dialog";
        case CDIN_FRONTEND_PROTOCOL_ACK: return "ack";
        default: return "unknown";
    }
}

static cdin_frontend_protocol_message_type message_from_name(const uint8_t *name, size_t length)
{
    int value;
    for (value = 1; value <= 20; value++) {
        const char *candidate = cdin_frontend_protocol_message_name(
            (cdin_frontend_protocol_message_type)value);
        if (strlen(candidate) == length && memcmp(candidate, name, length) == 0) {
            return (cdin_frontend_protocol_message_type)value;
        }
    }
    for (value = 64; value <= 82; value++) {
        const char *candidate = cdin_frontend_protocol_message_name(
            (cdin_frontend_protocol_message_type)value);
        if (strlen(candidate) == length && memcmp(candidate, name, length) == 0) {
            return (cdin_frontend_protocol_message_type)value;
        }
    }
    return (cdin_frontend_protocol_message_type)0;
}

int cdin_frontend_protocol_message_is_inbound(cdin_frontend_protocol_message_type type)
{
    return type >= CDIN_FRONTEND_PROTOCOL_HELLO && type <= CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL;
}

int cdin_frontend_protocol_message_requires_driver(cdin_frontend_protocol_message_type type)
{
    switch (type) {
        case CDIN_FRONTEND_PROTOCOL_INPUT_KEY:
        case CDIN_FRONTEND_PROTOCOL_INPUT_TEXT:
        case CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE:
        case CDIN_FRONTEND_PROTOCOL_INPUT_PASTE:
        case CDIN_FRONTEND_PROTOCOL_WINDOW_RESIZE:
        case CDIN_FRONTEND_PROTOCOL_FOCUS:
        case CDIN_FRONTEND_PROTOCOL_VIEW_SET_POSITION:
        case CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND:
        case CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE:
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START:
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE:
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL:
            return 1;
        default:
            return 0;
    }
}

typedef enum payload_field_kind {
    FIELD_UINT,
    FIELD_TEXT,
    FIELD_BYTES,
    FIELD_BOOL,
    FIELD_MAP,
    FIELD_ARRAY,
    FIELD_NULL_OR_MAP
} payload_field_kind;

typedef struct payload_field_rule {
    const char *name;
    payload_field_kind kind;
    int required;
} payload_field_rule;

static cdin_frontend_protocol_status field_has_kind(const uint8_t *value, size_t length,
                                                     payload_field_kind kind)
{
    cbor_cursor cursor = { value, length, 0u };
    cbor_head head;
    cdin_frontend_protocol_status status = read_head(&cursor, &head);
    int matches = 0;
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    switch (kind) {
        case FIELD_UINT: matches = head.major == 0u; break;
        case FIELD_TEXT: matches = head.major == 3u; break;
        case FIELD_BYTES: matches = head.major == 2u; break;
        case FIELD_BOOL: matches = head.major == 7u && (head.value == 20u || head.value == 21u); break;
        case FIELD_MAP: matches = head.major == 5u; break;
        case FIELD_ARRAY: matches = head.major == 4u; break;
        case FIELD_NULL_OR_MAP: matches = head.major == 5u || (head.major == 7u && head.value == 22u); break;
        default: return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (!matches) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    cursor.position = 0u;
    status = skip_item(&cursor, 1u);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    return cursor.position == cursor.length ? CDIN_FRONTEND_PROTOCOL_OK
                                            : CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
}

#define RULE_REQUIRED(field_name, field_kind) { field_name, field_kind, 1 }
#define RULE_OPTIONAL(field_name, field_kind) { field_name, field_kind, 0 }

static int raw_is_null(const uint8_t *value, size_t length)
{
    return value != NULL && length == 1u && value[0] == 0xf6u;
}

static int raw_text_equals(const uint8_t *value, size_t length, const char *expected)
{
    const uint8_t *text = NULL;
    size_t text_length = 0u;
    return parse_text_value(value, length, &text, &text_length) == CDIN_FRONTEND_PROTOCOL_OK &&
           text_length == strlen(expected) && memcmp(text, expected, text_length) == 0;
}

static int raw_is_map(const uint8_t *value, size_t length)
{
    cbor_cursor cursor = { value, length, 0u };
    cbor_head head;
    return read_head(&cursor, &head) == CDIN_FRONTEND_PROTOCOL_OK &&
           head.major == 5u && skip_item(&(cbor_cursor){ value, length, 0u }, 1u) ==
               CDIN_FRONTEND_PROTOCOL_OK && cursor.position <= length;
}

static int validate_options_map(const uint8_t *options, size_t options_length)
{
    static const char *const cursor_shapes[] = { "block", "bar", "underline" };
    static const char *const boolean_fields[] = {
        "wrap", "line_numbers", "relative_line_numbers", "cursor_blink"
    };
    const uint8_t *value = NULL;
    size_t value_length = 0u;
    uint64_t number;
    size_t index;
    int present;
    if (!raw_is_map(options, options_length)) return 0;
    if (find_value(options, options_length, "title", &value, &value_length) == CDIN_FRONTEND_PROTOCOL_OK &&
        field_has_kind(value, value_length, FIELD_TEXT) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
    if (find_value(options, options_length, "tab_width", &value, &value_length) == CDIN_FRONTEND_PROTOCOL_OK) {
        if (parse_uint_value(value, value_length, &number) != CDIN_FRONTEND_PROTOCOL_OK ||
            number < 1u || number > 64u) return 0;
    }
    for (index = 0u; index < sizeof(boolean_fields) / sizeof(boolean_fields[0]); index++) {
        if (find_value(options, options_length, boolean_fields[index], &value, &value_length) ==
            CDIN_FRONTEND_PROTOCOL_OK &&
            field_has_kind(value, value_length, FIELD_BOOL) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
    }
    if (find_value(options, options_length, "cursor_shape", &value, &value_length) ==
        CDIN_FRONTEND_PROTOCOL_OK) {
        int found = 0;
        for (index = 0u; index < sizeof(cursor_shapes) / sizeof(cursor_shapes[0]); index++) {
            if (raw_text_equals(value, value_length, cursor_shapes[index])) found = 1;
        }
        if (!found) return 0;
    }
    (void)present;
    return 1;
}

static int validate_pane_node(const uint8_t *node, size_t node_length, unsigned depth,
                              const uint8_t *active_leaf, size_t active_leaf_length,
                              int *active_leaf_found)
{
    const uint8_t *kind = NULL, *id = NULL, *views = NULL, *active_view = NULL;
    const uint8_t *first = NULL, *second = NULL, *axis = NULL, *ratio = NULL;
    size_t kind_length = 0u, id_length = 0u, views_length = 0u, active_view_length = 0u;
    size_t first_length = 0u, second_length = 0u, axis_length = 0u, ratio_length = 0u;
    uint64_t ratio_value;
    size_t view_count = 0u, i;
    char view_id[129];
    int matches = 0;
    if (depth > CDIN_FRONTEND_PROTOCOL_MAX_DEPTH || active_leaf_found == NULL ||
        find_value(node, node_length, "kind", &kind, &kind_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        find_value(node, node_length, "id", &id, &id_length) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
    {
        const uint8_t *text = NULL; size_t length = 0u;
        if (parse_text_value(id, id_length, &text, &length) != CDIN_FRONTEND_PROTOCOL_OK ||
            length == 0u || length > 128u) return 0;
        id = text; id_length = length;
    }
    if (raw_text_equals(kind, kind_length, "leaf")) {
        if (find_value(node, node_length, "views", &views, &views_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            find_value(node, node_length, "active_view", &active_view, &active_view_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_array_count(views, views_length, &view_count) !=
                CDIN_FRONTEND_PROTOCOL_OK || view_count > 64u) return 0;
        if (!raw_is_null(active_view, active_view_length)) {
            const uint8_t *active_text = NULL; size_t active_text_length = 0u;
            if (parse_text_value(active_view, active_view_length, &active_text, &active_text_length) !=
                CDIN_FRONTEND_PROTOCOL_OK || active_text_length == 0u || active_text_length > 128u) return 0;
            for (i = 0u; i < view_count; i++) {
                size_t item_length = 0u;
                if (cdin_frontend_protocol_array_get_text(views, views_length, i, view_id,
                        sizeof(view_id), &item_length) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
                if (item_length == active_text_length && memcmp(view_id, active_text, item_length) == 0)
                    matches = 1;
            }
            if (!matches) return 0;
        }
        if (id_length == active_leaf_length && memcmp(id, active_leaf, id_length) == 0)
            *active_leaf_found = 1;
    } else if (raw_text_equals(kind, kind_length, "split")) {
        if (find_value(node, node_length, "axis", &axis, &axis_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            (!raw_text_equals(axis, axis_length, "horizontal") &&
             !raw_text_equals(axis, axis_length, "vertical")) ||
            find_value(node, node_length, "ratio_bp", &ratio, &ratio_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            parse_uint_value(ratio, ratio_length, &ratio_value) != CDIN_FRONTEND_PROTOCOL_OK ||
            ratio_value < 1000u || ratio_value > 8999u ||
            find_value(node, node_length, "first", &first, &first_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            !raw_is_map(first, first_length) ||
            find_value(node, node_length, "second", &second, &second_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            !raw_is_map(second, second_length)) return 0;
        if (!validate_pane_node(first, first_length, depth + 1u, active_leaf,
                                active_leaf_length, active_leaf_found) ||
            !validate_pane_node(second, second_length, depth + 1u, active_leaf,
                                active_leaf_length, active_leaf_found)) return 0;
    } else return 0;
    return 1;
}

static int validate_layout_payload(const uint8_t *payload, size_t payload_length)
{
    const uint8_t *layout = NULL, *root = NULL, *active = NULL;
    size_t layout_length = 0u, root_length = 0u, active_length = 0u;
    const uint8_t *active_text = NULL; size_t active_text_length = 0u;
    int found = 0;
    if (find_value(payload, payload_length, "layout", &layout, &layout_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        find_value(layout, layout_length, "root", &root, &root_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        find_value(layout, layout_length, "active_leaf", &active, &active_length) != CDIN_FRONTEND_PROTOCOL_OK)
        return 0;
    if (raw_is_null(root, root_length)) return raw_is_null(active, active_length);
    if (!raw_is_map(root, root_length) || raw_is_null(active, active_length) ||
        parse_text_value(active, active_length, &active_text, &active_text_length) !=
            CDIN_FRONTEND_PROTOCOL_OK || active_text_length == 0u || active_text_length > 128u) return 0;
    return validate_pane_node(root, root_length, 1u, active_text, active_text_length, &found) && found;
}

static cdin_frontend_protocol_status validate_payload_rules(
    const uint8_t *payload, size_t payload_length,
    const payload_field_rule *rules, size_t rule_count)
{
    size_t index;
    for (index = 0u; index < rule_count; index++) {
        const uint8_t *value = NULL;
        size_t value_length = 0u;
        cdin_frontend_protocol_status status = find_value(payload, payload_length,
                                                          rules[index].name,
                                                          &value, &value_length);
        if (status == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED && !rules[index].required) continue;
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        status = field_has_kind(value, value_length, rules[index].kind);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    }
    return CDIN_FRONTEND_PROTOCOL_OK;
}


static int ime_utf8_boundary(const uint8_t *bytes, size_t length, uint64_t offset)
{
    if (offset > (uint64_t)length) return 0;
    if (offset == 0u || offset == (uint64_t)length) return 1;
    return (bytes[(size_t)offset] & 0xc0u) != 0x80u;
}

static int ime_cancel_reason_valid(const char *reason)
{
    static const char *const reasons[] = {
        "ime_cancelled", "focus_lost", "view_changed", "stale_document",
        "selection_changed", "view_removed", "driver_revoked", "session_reset"
    };
    size_t i;
    if (reason == NULL) return 0;
    for (i = 0u; i < sizeof(reasons) / sizeof(reasons[0]); ++i)
        if (strcmp(reason, reasons[i]) == 0) return 1;
    return 0;
}

cdin_frontend_protocol_status cdin_frontend_protocol_validate_payload(
    cdin_frontend_protocol_message_type type, const uint8_t *payload, size_t payload_length)
{
    static const payload_field_rule hello[] = {
        RULE_REQUIRED("minimum_major", FIELD_UINT), RULE_REQUIRED("minimum_minor", FIELD_UINT),
        RULE_REQUIRED("maximum_major", FIELD_UINT), RULE_REQUIRED("maximum_minor", FIELD_UINT),
        RULE_REQUIRED("frontend_name", FIELD_TEXT), RULE_REQUIRED("capabilities", FIELD_ARRAY),
        RULE_OPTIONAL("metadata", FIELD_MAP)
    };
    static const payload_field_rule attach[] = {
        RULE_REQUIRED("session_id", FIELD_UINT), RULE_REQUIRED("units", FIELD_TEXT),
        RULE_REQUIRED("width", FIELD_UINT), RULE_REQUIRED("height", FIELD_UINT),
        RULE_OPTIONAL("view_id", FIELD_TEXT)
    };
    static const payload_field_rule detach[] = { RULE_OPTIONAL("reason", FIELD_TEXT) };
    static const payload_field_rule input_key[] = {
        RULE_REQUIRED("key", FIELD_TEXT), RULE_REQUIRED("action", FIELD_TEXT),
        RULE_REQUIRED("modifiers", FIELD_ARRAY), RULE_OPTIONAL("scancode", FIELD_UINT)
    };
    static const payload_field_rule input_text[] = {
        RULE_REQUIRED("text", FIELD_TEXT), RULE_OPTIONAL("composition_id", FIELD_UINT)
    };
    static const payload_field_rule ime_start[] = {
        RULE_REQUIRED("id", FIELD_UINT), RULE_REQUIRED("view_id", FIELD_TEXT),
        RULE_REQUIRED("document_version", FIELD_UINT)
    };
    static const payload_field_rule ime_update[] = {
        RULE_REQUIRED("id", FIELD_UINT), RULE_REQUIRED("preedit", FIELD_TEXT),
        RULE_REQUIRED("cursor_byte", FIELD_UINT), RULE_REQUIRED("selection_start_byte", FIELD_UINT),
        RULE_REQUIRED("selection_end_byte", FIELD_UINT)
    };
    static const payload_field_rule ime_cancel[] = {
        RULE_REQUIRED("id", FIELD_UINT), RULE_REQUIRED("reason", FIELD_TEXT)
    };
    static const payload_field_rule input_mouse[] = {
        RULE_REQUIRED("action", FIELD_TEXT), RULE_REQUIRED("button", FIELD_TEXT),
        RULE_REQUIRED("modifiers", FIELD_ARRAY), RULE_OPTIONAL("target", FIELD_TEXT),
        RULE_OPTIONAL("position", FIELD_MAP)
    };
    static const payload_field_rule input_paste[] = { RULE_REQUIRED("bytes", FIELD_BYTES) };
    static const payload_field_rule resize[] = {
        RULE_REQUIRED("width", FIELD_UINT), RULE_REQUIRED("height", FIELD_UINT), RULE_REQUIRED("units", FIELD_TEXT)
    };
    static const payload_field_rule focus[] = { RULE_REQUIRED("focused", FIELD_BOOL) };
    static const payload_field_rule request_lines[] = {
        RULE_REQUIRED("view_id", FIELD_TEXT), RULE_REQUIRED("first_line", FIELD_UINT),
        RULE_REQUIRED("line_count", FIELD_UINT), RULE_REQUIRED("version", FIELD_UINT)
    };
    static const payload_field_rule set_position[] = {
        RULE_REQUIRED("view_id", FIELD_TEXT), RULE_REQUIRED("anchor", FIELD_UINT),
        RULE_REQUIRED("caret", FIELD_UINT), RULE_REQUIRED("version", FIELD_UINT)
    };
    static const payload_field_rule command[] = { RULE_REQUIRED("name", FIELD_TEXT), RULE_REQUIRED("arguments", FIELD_MAP) };
    static const payload_field_rule response_clipboard[] = { RULE_REQUIRED("request_id", FIELD_UINT), RULE_REQUIRED("bytes", FIELD_BYTES) };
    static const payload_field_rule response_dialog[] = {
        RULE_REQUIRED("request_id", FIELD_UINT), RULE_REQUIRED("accepted", FIELD_BOOL),
        RULE_OPTIONAL("selected", FIELD_TEXT)
    };
    static const payload_field_rule ack[] = { RULE_REQUIRED("sequence", FIELD_UINT) };
    static const payload_field_rule pane_splitter_resize[] = {
        RULE_REQUIRED("ratio_bp", FIELD_UINT), RULE_REQUIRED("split_id", FIELD_TEXT),
        RULE_REQUIRED("request_id", FIELD_UINT), RULE_REQUIRED("layout_revision", FIELD_UINT)
    };
    static const payload_field_rule pane_splitter_result[] = {
        RULE_REQUIRED("status", FIELD_TEXT), RULE_REQUIRED("request_id", FIELD_UINT),
        RULE_REQUIRED("layout_revision", FIELD_UINT)
    };
    static const payload_field_rule hello_ack[] = {
        RULE_REQUIRED("major", FIELD_UINT), RULE_REQUIRED("minor", FIELD_UINT),
        RULE_REQUIRED("session_id", FIELD_UINT), RULE_REQUIRED("client_id", FIELD_UINT),
        RULE_REQUIRED("driver", FIELD_BOOL), RULE_REQUIRED("accepted_capabilities", FIELD_ARRAY),
        RULE_OPTIONAL("driver_role_epoch", FIELD_UINT)
    };
    static const payload_field_rule driver_role_changed[] = {
        RULE_REQUIRED("epoch", FIELD_UINT), RULE_REQUIRED("driver", FIELD_BOOL)
    };
    static const payload_field_rule revision_theme[] = {
        RULE_REQUIRED("revision", FIELD_UINT), RULE_REQUIRED("theme", FIELD_TEXT),
        RULE_REQUIRED("options", FIELD_MAP)
    };
    static const payload_field_rule style_table[] = { RULE_REQUIRED("revision", FIELD_UINT), RULE_REQUIRED("styles", FIELD_MAP) };
    static const payload_field_rule layout[] = { RULE_REQUIRED("revision", FIELD_UINT), RULE_REQUIRED("layout", FIELD_MAP) };
    static const payload_field_rule lines[] = {
        RULE_REQUIRED("view_id", FIELD_TEXT), RULE_REQUIRED("version", FIELD_UINT),
        RULE_REQUIRED("first_line", FIELD_UINT), RULE_REQUIRED("lines", FIELD_ARRAY),
        RULE_OPTIONAL("total_lines", FIELD_UINT)
    };
    static const payload_field_rule invalidate[] = {
        RULE_REQUIRED("view_id", FIELD_TEXT), RULE_REQUIRED("version", FIELD_UINT),
        RULE_REQUIRED("ranges", FIELD_ARRAY)
    };
    static const payload_field_rule statusline[] = { RULE_REQUIRED("revision", FIELD_UINT), RULE_REQUIRED("status", FIELD_MAP) };
    static const payload_field_rule prompt[] = { RULE_REQUIRED("revision", FIELD_UINT), RULE_REQUIRED("prompt", FIELD_NULL_OR_MAP) };
    static const payload_field_rule panel[] = {
        RULE_REQUIRED("revision", FIELD_UINT), RULE_REQUIRED("panel_id", FIELD_TEXT),
        RULE_REQUIRED("panel", FIELD_NULL_OR_MAP)
    };
    static const payload_field_rule notify[] = {
        RULE_REQUIRED("level", FIELD_TEXT), RULE_REQUIRED("text", FIELD_TEXT),
        RULE_OPTIONAL("notification_id", FIELD_UINT)
    };
    static const payload_field_rule request_clipboard_get[] = { RULE_REQUIRED("request_id", FIELD_UINT) };
    static const payload_field_rule request_clipboard_set[] = { RULE_REQUIRED("request_id", FIELD_UINT), RULE_REQUIRED("bytes", FIELD_BYTES) };
    static const payload_field_rule request_dialog[] = {
        RULE_REQUIRED("request_id", FIELD_UINT), RULE_REQUIRED("kind", FIELD_TEXT), RULE_REQUIRED("options", FIELD_MAP)
    };
    static const payload_field_rule error[] = {
        RULE_REQUIRED("code", FIELD_TEXT), RULE_REQUIRED("message", FIELD_TEXT), RULE_OPTIONAL("request_id", FIELD_UINT)
    };
    static const payload_field_rule cursor[] = {
        RULE_REQUIRED("view_id", FIELD_TEXT), RULE_REQUIRED("version", FIELD_UINT),
        RULE_REQUIRED("position", FIELD_UINT), RULE_OPTIONAL("scroll_anchor", FIELD_UINT)
    };
    static const payload_field_rule selection[] = {
        RULE_REQUIRED("view_id", FIELD_TEXT), RULE_REQUIRED("version", FIELD_UINT),
        RULE_REQUIRED("anchor", FIELD_UINT), RULE_REQUIRED("caret", FIELD_UINT)
    };
    static const payload_field_rule flush[] = {
        RULE_REQUIRED("scope", FIELD_TEXT), RULE_REQUIRED("sequence", FIELD_UINT), RULE_OPTIONAL("revision", FIELD_UINT)
    };
    const payload_field_rule *rules = NULL;
    size_t count = 0u;
    cdin_frontend_protocol_status status;
    status = validate_map_payload(payload, payload_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
#define USE_RULES(array_name) do { rules = array_name; count = sizeof(array_name) / sizeof((array_name)[0]); } while (0)
    switch (type) {
        case CDIN_FRONTEND_PROTOCOL_HELLO: USE_RULES(hello); break;
        case CDIN_FRONTEND_PROTOCOL_ATTACH: USE_RULES(attach); break;
        case CDIN_FRONTEND_PROTOCOL_DETACH: USE_RULES(detach); break;
        case CDIN_FRONTEND_PROTOCOL_INPUT_KEY: USE_RULES(input_key); break;
        case CDIN_FRONTEND_PROTOCOL_INPUT_TEXT: USE_RULES(input_text); break;
        case CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE: USE_RULES(input_mouse); break;
        case CDIN_FRONTEND_PROTOCOL_INPUT_PASTE: USE_RULES(input_paste); break;
        case CDIN_FRONTEND_PROTOCOL_WINDOW_RESIZE: USE_RULES(resize); break;
        case CDIN_FRONTEND_PROTOCOL_FOCUS: USE_RULES(focus); break;
        case CDIN_FRONTEND_PROTOCOL_VIEW_REQUEST_LINES: USE_RULES(request_lines); break;
        case CDIN_FRONTEND_PROTOCOL_VIEW_SET_POSITION: USE_RULES(set_position); break;
        case CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND: USE_RULES(command); break;
        case CDIN_FRONTEND_PROTOCOL_RESPONSE_CLIPBOARD: USE_RULES(response_clipboard); break;
        case CDIN_FRONTEND_PROTOCOL_RESPONSE_DIALOG: USE_RULES(response_dialog); break;
        case CDIN_FRONTEND_PROTOCOL_ACK: USE_RULES(ack); break;
        case CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE: USE_RULES(pane_splitter_resize); break;
        case CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT: USE_RULES(pane_splitter_result); break;
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START: USE_RULES(ime_start); break;
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE: USE_RULES(ime_update); break;
        case CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL: USE_RULES(ime_cancel); break;
        case CDIN_FRONTEND_PROTOCOL_HELLO_ACK: USE_RULES(hello_ack); break;
        case CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED: USE_RULES(driver_role_changed); break;
        case CDIN_FRONTEND_PROTOCOL_UI_OPTIONS: USE_RULES(revision_theme); break;
        case CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE: USE_RULES(style_table); break;
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT: USE_RULES(layout); break;
        case CDIN_FRONTEND_PROTOCOL_VIEW_LINES: USE_RULES(lines); break;
        case CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE: USE_RULES(invalidate); break;
        case CDIN_FRONTEND_PROTOCOL_UI_STATUSLINE: USE_RULES(statusline); break;
        case CDIN_FRONTEND_PROTOCOL_UI_PROMPT: USE_RULES(prompt); break;
        case CDIN_FRONTEND_PROTOCOL_UI_PANEL: USE_RULES(panel); break;
        case CDIN_FRONTEND_PROTOCOL_UI_NOTIFY: USE_RULES(notify); break;
        case CDIN_FRONTEND_PROTOCOL_REQUEST_CLIPBOARD_GET: USE_RULES(request_clipboard_get); break;
        case CDIN_FRONTEND_PROTOCOL_REQUEST_CLIPBOARD_SET: USE_RULES(request_clipboard_set); break;
        case CDIN_FRONTEND_PROTOCOL_REQUEST_DIALOG: USE_RULES(request_dialog); break;
        case CDIN_FRONTEND_PROTOCOL_ERROR: USE_RULES(error); break;
        case CDIN_FRONTEND_PROTOCOL_UI_CURSOR: USE_RULES(cursor); break;
        case CDIN_FRONTEND_PROTOCOL_UI_SELECTION: USE_RULES(selection); break;
        case CDIN_FRONTEND_PROTOCOL_UI_FLUSH: USE_RULES(flush); break;
        case CDIN_FRONTEND_PROTOCOL_DETACH_ACK: count = 0u; break;
        default: return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
#undef USE_RULES
    if (count != 0u) {
        status = validate_payload_rules(payload, payload_length, rules, count);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    }

    if (type == CDIN_FRONTEND_PROTOCOL_INPUT_TEXT) {
        uint64_t composition_id = 0u;
        const uint8_t *raw_text = NULL;
        const uint8_t *committed_text = NULL;
        size_t raw_text_length = 0u;
        size_t text_length = 0u;
        status = cdin_frontend_protocol_payload_get_uint(
            payload, payload_length, "composition_id", &composition_id);
        if (status == CDIN_FRONTEND_PROTOCOL_OK) {
            if (composition_id == 0u ||
                find_value(payload, payload_length, "text",
                           &raw_text, &raw_text_length) != CDIN_FRONTEND_PROTOCOL_OK ||
                parse_text_value(raw_text, raw_text_length,
                                 &committed_text, &text_length) != CDIN_FRONTEND_PROTOCOL_OK ||
                text_length == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
            (void)committed_text;
        } else if (status != CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
    }
    if (type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START) {
        uint64_t id = 0u;
        char view_id[129];
        size_t view_length = 0u;
        if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "id", &id) !=
                CDIN_FRONTEND_PROTOCOL_OK || id == 0u ||
            cdin_frontend_protocol_payload_get_text(payload, payload_length, "view_id",
                view_id, sizeof(view_id), &view_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            view_length == 0u || view_length > 128u) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE) {
        uint64_t id = 0u, cursor_byte = 0u, selection_start = 0u, selection_end = 0u;
        char preedit[4097];
        size_t preedit_length = 0u;
        if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "id", &id) !=
                CDIN_FRONTEND_PROTOCOL_OK || id == 0u ||
            cdin_frontend_protocol_payload_get_text(payload, payload_length, "preedit",
                preedit, sizeof(preedit), &preedit_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            preedit_length > 4096u ||
            !valid_utf8((const uint8_t *)preedit, preedit_length) ||
            cdin_frontend_protocol_payload_get_uint(payload, payload_length, "cursor_byte",
                &cursor_byte) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(payload, payload_length, "selection_start_byte",
                &selection_start) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(payload, payload_length, "selection_end_byte",
                &selection_end) != CDIN_FRONTEND_PROTOCOL_OK ||
            selection_start > selection_end ||
            !ime_utf8_boundary((const uint8_t *)preedit, preedit_length, cursor_byte) ||
            !ime_utf8_boundary((const uint8_t *)preedit, preedit_length, selection_start) ||
            !ime_utf8_boundary((const uint8_t *)preedit, preedit_length, selection_end)) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
    }
    if (type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL) {
        uint64_t id = 0u;
        char reason[32];
        size_t reason_length = 0u;
        if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "id", &id) !=
                CDIN_FRONTEND_PROTOCOL_OK || id == 0u ||
            cdin_frontend_protocol_payload_get_text(payload, payload_length, "reason",
                reason, sizeof(reason), &reason_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            !ime_cancel_reason_valid(reason)) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) {
        uint64_t request_id = 0u, layout_revision = 0u, ratio_bp = 0u;
        char split_id[129];
        size_t split_id_length = 0u;
        if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "request_id", &request_id) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(payload, payload_length, "layout_revision", &layout_revision) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(payload, payload_length, "ratio_bp", &ratio_bp) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_text(payload, payload_length, "split_id", split_id, sizeof(split_id), &split_id_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            request_id == 0u || layout_revision == 0u || ratio_bp < 1000u || ratio_bp > 8999u ||
            split_id_length == 0u || split_id_length > 128u) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT) {
        static const char *const result_statuses[] = {
            "applied", "stale_revision", "not_found", "invalid_ratio",
            "not_driver", "unsupported", "busy"
        };
        char result_status[32];
        size_t result_status_length = 0u, status_index;
        uint64_t request_id = 0u, layout_revision = 0u;
        int known_status = 0;
        if (cdin_frontend_protocol_payload_get_uint(payload, payload_length, "request_id", &request_id) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_uint(payload, payload_length, "layout_revision", &layout_revision) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_text(payload, payload_length, "status", result_status, sizeof(result_status), &result_status_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            request_id == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        for (status_index = 0u; status_index < sizeof(result_statuses) / sizeof(result_statuses[0]); ++status_index) {
            if (strcmp(result_status, result_statuses[status_index]) == 0) known_status = 1;
        }
        if (!known_status) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        (void)layout_revision;
    }
    if (type == CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED) {
        uint64_t epoch = 0u;
        if (cdin_frontend_protocol_payload_get_uint(payload, payload_length,
                                                    "epoch", &epoch) != CDIN_FRONTEND_PROTOCOL_OK ||
            epoch == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (type == CDIN_FRONTEND_PROTOCOL_UI_OPTIONS) {
        const uint8_t *options = NULL;
        size_t options_length = 0u;
        if (find_value(payload, payload_length, "options", &options, &options_length) !=
                CDIN_FRONTEND_PROTOCOL_OK || !validate_options_map(options, options_length))
            return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (type == CDIN_FRONTEND_PROTOCOL_UI_LAYOUT && !validate_layout_payload(payload, payload_length))
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    if (type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH) {
        char scope[16]; size_t scope_length = 0u;
        uint64_t revision = 0u;
        status = cdin_frontend_protocol_payload_get_text(payload, payload_length, "scope", scope, sizeof(scope), &scope_length);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
        status = cdin_frontend_protocol_payload_get_uint(payload, payload_length, "revision", &revision);
        if (scope_length == 8u && memcmp(scope, "snapshot", 8u) == 0) {
            return status == CDIN_FRONTEND_PROTOCOL_OK ? CDIN_FRONTEND_PROTOCOL_OK : CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
        if (scope_length == 8u && memcmp(scope, "response", 8u) == 0) {
            return status == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED ? CDIN_FRONTEND_PROTOCOL_OK : CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
        }
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_uint(
    const uint8_t *payload, size_t payload_length, const char *field, uint64_t *out_value)
{
    const uint8_t *value;
    size_t length;
    cdin_frontend_protocol_status status;
    if (out_value == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = find_value(payload, payload_length, field, &value, &length);
    return status == CDIN_FRONTEND_PROTOCOL_OK ? parse_uint_value(value, length, out_value) : status;
}

cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_text(
    const uint8_t *payload, size_t payload_length, const char *field,
    char *output, size_t output_capacity, size_t *out_length)
{
    const uint8_t *value;
    const uint8_t *text;
    size_t length;
    cdin_frontend_protocol_status status;
    if (output == NULL || output_capacity == 0u || out_length == NULL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    status = find_value(payload, payload_length, field, &value, &length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    status = parse_text_value(value, length, &text, out_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (*out_length >= output_capacity) return CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL;
    memcpy(output, text, *out_length);
    output[*out_length] = '\0';
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_bool(
    const uint8_t *payload, size_t payload_length, const char *field, int *out_value)
{
    const uint8_t *value;
    size_t length;
    cdin_frontend_protocol_status status;
    if (out_value == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = find_value(payload, payload_length, field, &value, &length);
    return status == CDIN_FRONTEND_PROTOCOL_OK ? parse_bool_value(value, length, out_value) : status;
}

cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_bytes(
    const uint8_t *payload, size_t payload_length, const char *field,
    const uint8_t **out_value, size_t *out_value_length)
{
    const uint8_t *value;
    size_t length;
    cdin_frontend_protocol_status status;
    if (out_value == NULL || out_value_length == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = find_value(payload, payload_length, field, &value, &length);
    return status == CDIN_FRONTEND_PROTOCOL_OK
               ? parse_bytes_value(value, length, out_value, out_value_length)
               : status;
}

cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_raw(
    const uint8_t *payload, size_t payload_length, const char *field,
    const uint8_t **out_value, size_t *out_value_length)
{
    return find_value(payload, payload_length, field, out_value, out_value_length);
}

cdin_frontend_protocol_status cdin_frontend_protocol_array_count(
    const uint8_t *array, size_t array_length, size_t *out_count)
{
    cbor_cursor cursor;
    cbor_head head;
    uint64_t index;
    cdin_frontend_protocol_status status;
    if (array == NULL || out_count == NULL || array_length == 0u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    cursor.data = array;
    cursor.length = array_length;
    cursor.position = 0u;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || head.major != 4u || head.value > SIZE_MAX) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (head.value > 4096u) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    for (index = 0u; index < head.value; index++) {
        status = skip_item(&cursor, 1u);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    }
    if (cursor.position != cursor.length) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    *out_count = (size_t)head.value;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_array_get_raw(
    const uint8_t *array, size_t array_length, size_t index,
    const uint8_t **out_value, size_t *out_value_length)
{
    cbor_cursor cursor;
    cbor_head head;
    size_t count;
    uint64_t item;
    cdin_frontend_protocol_status status;
    if (array == NULL || out_value == NULL || out_value_length == NULL || array_length == 0u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    status = cdin_frontend_protocol_array_count(array, array_length, &count);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (index >= count) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    cursor.data = array;
    cursor.length = array_length;
    cursor.position = 0u;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    for (item = 0u; item <= (uint64_t)index; item++) {
        size_t start = cursor.position;
        status = skip_item(&cursor, 1u);
        if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
        if (item == (uint64_t)index) {
            *out_value = array + start;
            *out_value_length = cursor.position - start;
            return CDIN_FRONTEND_PROTOCOL_OK;
        }
    }
    return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
}

cdin_frontend_protocol_status cdin_frontend_protocol_array_get_text(
    const uint8_t *array, size_t array_length, size_t index,
    char *output, size_t output_capacity, size_t *out_length)
{
    const uint8_t *raw;
    const uint8_t *text;
    size_t raw_length;
    cdin_frontend_protocol_status status;
    if (output == NULL || output_capacity == 0u || out_length == NULL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    status = cdin_frontend_protocol_array_get_raw(array, array_length, index,
                                                   &raw, &raw_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    status = parse_text_value(raw, raw_length, &text, out_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    if (*out_length >= output_capacity) return CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL;
    memcpy(output, text, *out_length);
    output[*out_length] = '\0';
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static int is_pane_message(cdin_frontend_protocol_message_type type)
{
    return type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT ||
           type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE;
}

static int is_ime_message(cdin_frontend_protocol_message_type type)
{
    return type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START ||
           type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE ||
           type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL;
}

static size_t uint_encoded_size(uint64_t value)
{
    if (value < 24u) return 1u;
    if (value <= UINT8_MAX) return 2u;
    if (value <= UINT16_MAX) return 3u;
    if (value <= UINT32_MAX) return 5u;
    return 9u;
}

static void write_uint(uint8_t *buffer, size_t *position, uint8_t major, uint64_t value)
{
    size_t count;
    size_t i;
    if (value < 24u) {
        buffer[(*position)++] = (uint8_t)(((unsigned)major << 5u) | (unsigned)(uint8_t)value);
        return;
    }
    if (value <= UINT8_MAX) count = 1u;
    else if (value <= UINT16_MAX) count = 2u;
    else if (value <= UINT32_MAX) count = 4u;
    else count = 8u;
    buffer[(*position)++] = (uint8_t)(((unsigned)major << 5u) |
                                    (count == 1u ? 24u : count == 2u ? 25u :
                                     count == 4u ? 26u : 27u));
    for (i = 0u; i < count; i++) {
        unsigned shift = (unsigned)((count - i - 1u) * 8u);
        buffer[(*position)++] = (uint8_t)(value >> shift);
    }
}

static void write_text(uint8_t *buffer, size_t *position, const char *text, size_t length)
{
    write_uint(buffer, position, 3u, (uint64_t)length);
    memcpy(buffer + *position, text, length);
    *position += length;
}

cdin_frontend_protocol_status cdin_frontend_protocol_encode_frame(
    const cdin_frontend_protocol_message_view *message,
    uint8_t *output, size_t output_capacity, size_t *out_length, size_t max_frame_bytes)
{
    size_t body_position;
    size_t body_length;
    size_t required;
    const char *type_name;
    if (message == NULL || output == NULL || out_length == NULL || message->payload == NULL ||
        message->payload_length == 0u || message->name_space == NULL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (message->major != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR ||
        message->minor != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR) {
        return CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH;
    }
    if ((is_pane_message(message->type) && strcmp(message->name_space, "pane") != 0) ||
        (is_ime_message(message->type) && strcmp(message->name_space, "ime") != 0) ||
        (!is_pane_message(message->type) && !is_ime_message(message->type) &&
         strcmp(message->name_space, "core") != 0)) return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    if (message->sequence == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (cdin_frontend_protocol_validate_payload(message->type,
                                                 message->payload,
                                                 message->payload_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    type_name = cdin_frontend_protocol_message_name(message->type);
    if (strcmp(type_name, "unknown") == 0) return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    if (max_frame_bytes == 0u) max_frame_bytes = CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES;
    if (max_frame_bytes > CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    required = 1u + 2u + 1u + uint_encoded_size(message->major) + uint_encoded_size(message->minor) +
               3u + 5u + 4u + uint_encoded_size(message->sequence) +
               5u + uint_encoded_size((uint64_t)strlen(type_name)) + strlen(type_name) +
               8u + message->payload_length;
    if (required > max_frame_bytes || required > UINT32_MAX ||
        required > SIZE_MAX - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    if (output_capacity < required + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL;
    }
    body_position = CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES;
    output[body_position++] = 0xa5u;
    write_text(output, &body_position, "v", 1u);
    output[body_position++] = 0x82u;
    write_uint(output, &body_position, 0u, message->major);
    write_uint(output, &body_position, 0u, message->minor);
    write_text(output, &body_position, "ns", 2u);
    write_text(output, &body_position, message->name_space, strlen(message->name_space));
    write_text(output, &body_position, "seq", 3u);
    write_uint(output, &body_position, 0u, message->sequence);
    write_text(output, &body_position, "type", 4u);
    write_text(output, &body_position, type_name, strlen(type_name));
    write_text(output, &body_position, "payload", 7u);
    memcpy(output + body_position, message->payload, message->payload_length);
    body_position += message->payload_length;
    body_length = body_position - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES;
    output[0] = (uint8_t)(body_length >> 24u);
    output[1] = (uint8_t)(body_length >> 16u);
    output[2] = (uint8_t)(body_length >> 8u);
    output[3] = (uint8_t)body_length;
    *out_length = body_position;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

static cdin_frontend_protocol_status get_envelope_value(const uint8_t *body,
                                                         size_t body_length,
                                                         const char *field,
                                                         const uint8_t **out_value,
                                                         size_t *out_length)
{
    return find_value(body, body_length, field, out_value, out_length);
}

cdin_frontend_protocol_status cdin_frontend_protocol_decode_frame(
    const uint8_t *frame, size_t frame_length,
    cdin_frontend_protocol_message_view *out_message, size_t max_frame_bytes)
{
    size_t body_length;
    const uint8_t *value;
    const uint8_t *payload;
    const uint8_t *text;
    size_t value_length;
    size_t text_length;
    int pane_namespace = 0;
    int ime_namespace = 0;
    uint64_t major;
    uint64_t minor;
    uint64_t sequence;
    cdin_frontend_protocol_message_type type;
    cbor_cursor cursor;
    cbor_head head;
    cdin_frontend_protocol_status status;
    if (frame == NULL || out_message == NULL || frame_length < 5u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (max_frame_bytes == 0u) max_frame_bytes = CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES;
    if (max_frame_bytes > CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    body_length = ((size_t)frame[0] << 24u) | ((size_t)frame[1] << 16u) |
                  ((size_t)frame[2] << 8u) | (size_t)frame[3];
    if (body_length == 0u || body_length > max_frame_bytes ||
        body_length != frame_length - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    if (validate_map_payload(frame + 4u, body_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    cursor.data = frame + 4u;
    cursor.length = body_length;
    cursor.position = 0u;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || head.major != 5u || head.value != 5u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    status = get_envelope_value(frame + 4u, body_length, "v", &value, &value_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    cursor.data = value; cursor.length = value_length; cursor.position = 0u;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || head.major != 4u || head.value != 2u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || head.major != 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    major = head.value;
    status = read_head(&cursor, &head);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || head.major != 0u || cursor.position != cursor.length) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    minor = head.value;
    status = get_envelope_value(frame + 4u, body_length, "ns", &value, &value_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK ||
        parse_text_value(value, value_length, &text, &text_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    }
    if (text_length == 4u && memcmp(text, "pane", 4u) == 0) pane_namespace = 1;
    else if (text_length == 3u && memcmp(text, "ime", 3u) == 0) ime_namespace = 1;
    else if (text_length != 4u || memcmp(text, "core", 4u) != 0) return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    status = get_envelope_value(frame + 4u, body_length, "seq", &value, &value_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK ||
        parse_uint_value(value, value_length, &sequence) != CDIN_FRONTEND_PROTOCOL_OK || sequence == 0u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    status = get_envelope_value(frame + 4u, body_length, "type", &value, &value_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK ||
        parse_text_value(value, value_length, &text, &text_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    type = message_from_name(text, text_length);
    if ((int)type == 0) return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    if ((is_pane_message(type) != 0) != (pane_namespace != 0) ||
        (is_ime_message(type) != 0) != (ime_namespace != 0)) return CDIN_FRONTEND_PROTOCOL_UNSUPPORTED;
    status = get_envelope_value(frame + 4u, body_length, "payload", &payload, &value_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_validate_payload(type, payload, value_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    out_message->major = major > UINT32_MAX ? UINT32_MAX : (uint32_t)major;
    out_message->minor = minor > UINT32_MAX ? UINT32_MAX : (uint32_t)minor;
    out_message->sequence = sequence;
    out_message->type = type;
    out_message->name_space = pane_namespace != 0 ? "pane" :
                              (ime_namespace != 0 ? "ime" : "core");
    out_message->payload = payload;
    out_message->payload_length = value_length;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_read_frame(
    const cdin_frontend_protocol_io *io, uint8_t *buffer,
    size_t capacity, size_t *out_length, size_t max_frame_bytes)
{
    size_t received = 0u;
    size_t body_length;
    if (io == NULL || io->read == NULL || buffer == NULL || out_length == NULL || capacity < 5u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (max_frame_bytes == 0u) max_frame_bytes = CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES;
    if (max_frame_bytes > CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    while (received < CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        ptrdiff_t count = io->read(io->context, buffer + received,
                                  CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES - received);
        if (count < 0) return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        if (count == 0) return received == 0u ? CDIN_FRONTEND_PROTOCOL_EOF
                                               : CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        if ((size_t)count > CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES - received) {
            return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        }
        received += (size_t)count;
    }
    body_length = ((size_t)buffer[0] << 24u) | ((size_t)buffer[1] << 16u) |
                  ((size_t)buffer[2] << 8u) | (size_t)buffer[3];
    if (body_length == 0u || body_length > max_frame_bytes ||
        body_length > capacity - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        return body_length > max_frame_bytes ? CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED
                                             : CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL;
    }
    while (received < body_length + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        ptrdiff_t count = io->read(io->context, buffer + received,
                                  body_length + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES - received);
        if (count < 0) return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        if (count == 0) return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        if ((size_t)count > body_length + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES - received) {
            return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        }
        received += (size_t)count;
    }
    *out_length = received;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_write_frame(
    const cdin_frontend_protocol_io *io, const uint8_t *frame, size_t frame_length)
{
    size_t written = 0u;
    if (io == NULL || io->write == NULL || frame == NULL || frame_length < 5u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    while (written < frame_length) {
        ptrdiff_t count = io->write(io->context, frame + written, frame_length - written);
        if (count < 0) return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        if (count == 0 || (size_t)count > frame_length - written) {
            return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        }
        written += (size_t)count;
    }
    return CDIN_FRONTEND_PROTOCOL_OK;
}
