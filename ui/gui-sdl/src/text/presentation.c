#include "presentation.h"

#include <string.h>

typedef struct cbor_cursor {
    const uint8_t *bytes;
    size_t length;
    size_t offset;
} cbor_cursor;

static int read_head(cbor_cursor *cursor, uint8_t *out_major, uint64_t *out_value)
{
    uint8_t initial;
    uint8_t additional;
    uint64_t value = 0u;
    size_t width = 0u;
    size_t index;

    if (cursor == NULL || out_major == NULL || out_value == NULL ||
        cursor->offset >= cursor->length) {
        return 0;
    }
    initial = cursor->bytes[cursor->offset++];
    *out_major = (uint8_t)(initial >> 5u);
    additional = (uint8_t)(initial & 0x1fu);
    if (additional < 24u) {
        value = additional;
    } else if (additional == 24u) {
        width = 1u;
    } else if (additional == 25u) {
        width = 2u;
    } else if (additional == 26u) {
        width = 4u;
    } else if (additional == 27u) {
        width = 8u;
    } else {
        /* Indefinite-length items and reserved additional values are forbidden. */
        return 0;
    }
    if (width != 0u) {
        if (width > cursor->length - cursor->offset) return 0;
        for (index = 0u; index < width; ++index) {
            value = (value << 8u) | cursor->bytes[cursor->offset++];
        }
        /* Enforce shortest-form integer/length encoding. */
        if (*out_major != 7u &&
            ((width == 1u && value < 24u) ||
             (width == 2u && value <= 0xffu) ||
             (width == 4u && value <= 0xffffu) ||
             (width == 8u && value <= 0xffffffffu))) {
            return 0;
        }
    }
    *out_value = value;
    return 1;
}

static int skip_item(cbor_cursor *cursor, unsigned depth)
{
    uint8_t major;
    uint64_t value;
    uint64_t index;
    if (depth > 32u || !read_head(cursor, &major, &value)) return 0;
    switch (major) {
        case 0u:
        case 1u:
        case 7u:
            return 1;
        case 2u:
        case 3u:
            if (value > (uint64_t)(cursor->length - cursor->offset)) return 0;
            cursor->offset += (size_t)value;
            return 1;
        case 4u:
            for (index = 0u; index < value; ++index) {
                if (!skip_item(cursor, depth + 1u)) return 0;
            }
            return 1;
        case 5u:
            if (value > UINT64_MAX / 2u) return 0;
            for (index = 0u; index < value * 2u; ++index) {
                if (!skip_item(cursor, depth + 1u)) return 0;
            }
            return 1;
        case 6u:
            return skip_item(cursor, depth + 1u);
        default:
            return 0;
    }
}

static int read_text_key(
    cbor_cursor *cursor,
    const uint8_t **out_text,
    size_t *out_length)
{
    uint8_t major;
    uint64_t length;
    if (!read_head(cursor, &major, &length) || major != 3u ||
        length > (uint64_t)(cursor->length - cursor->offset)) {
        return 0;
    }
    *out_text = cursor->bytes + cursor->offset;
    *out_length = (size_t)length;
    cursor->offset += (size_t)length;
    return 1;
}

/* Returns 1 when found, 0 when absent, and -1 for a malformed map. */
static int map_find(
    const uint8_t *encoded,
    size_t encoded_length,
    const char *field,
    const uint8_t **out_value,
    size_t *out_value_length)
{
    cbor_cursor cursor;
    uint8_t major;
    uint64_t count;
    uint64_t index;
    size_t field_length;
    if (encoded == NULL || field == NULL || out_value == NULL ||
        out_value_length == NULL) return -1;
    cursor.bytes = encoded;
    cursor.length = encoded_length;
    cursor.offset = 0u;
    if (!read_head(&cursor, &major, &count) || major != 5u) return -1;
    field_length = strlen(field);
    for (index = 0u; index < count; ++index) {
        const uint8_t *key;
        size_t key_length;
        size_t value_start;
        if (!read_text_key(&cursor, &key, &key_length)) return -1;
        value_start = cursor.offset;
        if (!skip_item(&cursor, 1u)) return -1;
        if (key_length == field_length &&
            memcmp(key, field, field_length) == 0) {
            *out_value = encoded + value_start;
            *out_value_length = cursor.offset - value_start;
            return 1;
        }
    }
    return cursor.offset == cursor.length ? 0 : -1;
}

static int read_unsigned(const uint8_t *encoded, size_t encoded_length,
                         uint64_t *out_value)
{
    cbor_cursor cursor;
    uint8_t major;
    uint64_t value;
    if (encoded == NULL || out_value == NULL) return 0;
    cursor.bytes = encoded;
    cursor.length = encoded_length;
    cursor.offset = 0u;
    if (!read_head(&cursor, &major, &value) || major != 0u ||
        cursor.offset != cursor.length) return 0;
    *out_value = value;
    return 1;
}

static int read_boolean(const uint8_t *encoded, size_t encoded_length,
                        uint8_t *out_value)
{
    if (encoded == NULL || out_value == NULL || encoded_length != 1u) return 0;
    if (encoded[0] == 0xf4u) {
        *out_value = 0u;
        return 1;
    }
    if (encoded[0] == 0xf5u) {
        *out_value = 1u;
        return 1;
    }
    return 0;
}

static int role_id_valid(const uint8_t *bytes, size_t length)
{
    size_t index;
    if (bytes == NULL || length == 0u ||
        length > CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLE_ID_BYTES) return 0;
    for (index = 0u; index < length; ++index) {
        uint8_t value = bytes[index];
        int alphanumeric =
            (value >= (uint8_t)'A' && value <= (uint8_t)'Z') ||
            (value >= (uint8_t)'a' && value <= (uint8_t)'z') ||
            (value >= (uint8_t)'0' && value <= (uint8_t)'9');
        if (index == 0u && !alphanumeric) return 0;
        if (!alphanumeric && value != (uint8_t)'.' &&
            value != (uint8_t)'_' && value != (uint8_t)'-') return 0;
    }
    return 1;
}

static int read_rgba(const uint8_t *encoded, size_t encoded_length,
                     uint8_t out_rgba[4])
{
    static const char *const names[4] = {"r", "g", "b", "a"};
    uint8_t rgba[4];
    size_t index;
    for (index = 0u; index < 4u; ++index) {
        const uint8_t *value = NULL;
        size_t value_length = 0u;
        uint64_t channel;
        int found = map_find(encoded, encoded_length, names[index],
                             &value, &value_length);
        if (found != 1 ||
            !read_unsigned(value, value_length, &channel) || channel > 255u) {
            return 0;
        }
        rgba[index] = (uint8_t)channel;
    }
    memcpy(out_rgba, rgba, sizeof(rgba));
    return 1;
}

static int optional_rgba(const uint8_t *style, size_t style_length,
                         const char *field, uint8_t color[4], uint8_t *has_color)
{
    const uint8_t *value = NULL;
    size_t value_length = 0u;
    int found = map_find(style, style_length, field, &value, &value_length);
    if (found < 0) return 0;
    if (found == 0) {
        *has_color = 0u;
        return 1;
    }
    if (!read_rgba(value, value_length, color)) return 0;
    *has_color = 1u;
    return 1;
}

static int optional_boolean(const uint8_t *style, size_t style_length,
                            const char *field, uint8_t *out_value)
{
    const uint8_t *value = NULL;
    size_t value_length = 0u;
    int found = map_find(style, style_length, field, &value, &value_length);
    if (found < 0) return 0;
    if (found == 0) {
        *out_value = 0u;
        return 1;
    }
    return read_boolean(value, value_length, out_value);
}

static int decode_style(const uint8_t *encoded, size_t encoded_length,
                        cdin_frontend_gui_sdl_style_role *out_style)
{
    cdin_frontend_gui_sdl_style_role style;
    uint8_t major;
    uint64_t count;
    cbor_cursor cursor;
    if (encoded == NULL || out_style == NULL) return 0;
    cursor.bytes = encoded;
    cursor.length = encoded_length;
    cursor.offset = 0u;
    if (!read_head(&cursor, &major, &count) || major != 5u) return 0;
    (void)count;
    memset(&style, 0, sizeof(style));
    if (!optional_rgba(encoded, encoded_length, "foreground",
                       style.foreground, &style.has_foreground) ||
        !optional_rgba(encoded, encoded_length, "background",
                       style.background, &style.has_background) ||
        !optional_boolean(encoded, encoded_length, "bold", &style.bold) ||
        !optional_boolean(encoded, encoded_length, "italic", &style.italic) ||
        !optional_boolean(encoded, encoded_length, "underline", &style.underline) ||
        !optional_boolean(encoded, encoded_length, "strikethrough",
                          &style.strikethrough) ||
        !optional_boolean(encoded, encoded_length, "dim", &style.dim) ||
        !optional_boolean(encoded, encoded_length, "inverse", &style.inverse)) {
        return 0;
    }
    *out_style = style;
    return 1;
}

cdin_frontend_gui_sdl_style_status cdin_frontend_gui_sdl_style_table_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    cdin_frontend_gui_sdl_style_table *out_table)
{
    cdin_frontend_gui_sdl_style_table decoded;
    cbor_cursor cursor;
    uint8_t major;
    uint64_t count;
    uint64_t index;
    if (encoded == NULL || encoded_length == 0u || out_table == NULL) {
        return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
    }
    cursor.bytes = encoded;
    cursor.length = encoded_length;
    cursor.offset = 0u;
    if (!read_head(&cursor, &major, &count) || major != 5u) {
        return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
    }
    if (count > CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLES) {
        return CDIN_FRONTEND_GUI_SDL_STYLE_LIMIT;
    }
    memset(&decoded, 0, sizeof(decoded));
    for (index = 0u; index < count; ++index) {
        const uint8_t *role_id;
        size_t role_id_length;
        const uint8_t *value;
        size_t value_start;
        size_t value_length;
        cdin_frontend_gui_sdl_style_role role;
        size_t prior;
        if (!read_text_key(&cursor, &role_id, &role_id_length) ||
            !role_id_valid(role_id, role_id_length)) {
            return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
        }
        value_start = cursor.offset;
        if (!skip_item(&cursor, 1u)) {
            return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
        }
        value = encoded + value_start;
        value_length = cursor.offset - value_start;
        if (!decode_style(value, value_length, &role)) {
            return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
        }
        for (prior = 0u; prior < decoded.role_count; ++prior) {
            if (strlen(decoded.roles[prior].id) == role_id_length &&
                memcmp(decoded.roles[prior].id, role_id, role_id_length) == 0) {
                return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
            }
        }
        memcpy(role.id, role_id, role_id_length);
        role.id[role_id_length] = '\0';
        decoded.roles[decoded.role_count++] = role;
    }
    if (cursor.offset != cursor.length) {
        return CDIN_FRONTEND_GUI_SDL_STYLE_INVALID;
    }
    *out_table = decoded;
    return CDIN_FRONTEND_GUI_SDL_STYLE_OK;
}

const cdin_frontend_gui_sdl_style_role *cdin_frontend_gui_sdl_style_table_find(
    const cdin_frontend_gui_sdl_style_table *table,
    const char *role_id)
{
    size_t index;
    if (table == NULL || role_id == NULL) return NULL;
    for (index = 0u; index < table->role_count; ++index) {
        if (strcmp(table->roles[index].id, role_id) == 0) {
            return &table->roles[index];
        }
    }
    return NULL;
}
