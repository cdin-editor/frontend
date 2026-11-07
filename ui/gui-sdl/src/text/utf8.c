#include "utf8.h"

#define CDIN_FRONTEND_GUI_SDL_UTF8_REPLACEMENT 0xfffdu

static int is_continuation(uint8_t byte)
{
    return (byte & 0xc0u) == 0x80u;
}

/* Invalid encodings consume one source byte to keep byte mapping reversible. */
static void decode_one(
    const uint8_t *bytes,
    size_t remaining,
    uint32_t *out_codepoint,
    size_t *out_width,
    int *out_valid)
{
    const uint8_t lead = bytes[0];
    uint32_t codepoint;

    *out_codepoint = CDIN_FRONTEND_GUI_SDL_UTF8_REPLACEMENT;
    *out_width = 1u;
    *out_valid = 0;

    if (lead <= 0x7fu) {
        *out_codepoint = (uint32_t)lead;
        *out_valid = 1;
        return;
    }

    if (lead >= 0xc2u && lead <= 0xdfu) {
        if (remaining < 2u || !is_continuation(bytes[1])) return;
        codepoint = ((uint32_t)(lead & 0x1fu) << 6u)
            | (uint32_t)(bytes[1] & 0x3fu);
        *out_codepoint = codepoint;
        *out_width = 2u;
        *out_valid = 1;
        return;
    }

    if (lead >= 0xe0u && lead <= 0xefu) {
        if (remaining < 3u || !is_continuation(bytes[2])) return;
        if (!is_continuation(bytes[1])) return;
        if (lead == 0xe0u && bytes[1] < 0xa0u) return;
        if (lead == 0xedu && bytes[1] >= 0xa0u) return;
        codepoint = ((uint32_t)(lead & 0x0fu) << 12u)
            | ((uint32_t)(bytes[1] & 0x3fu) << 6u)
            | (uint32_t)(bytes[2] & 0x3fu);
        *out_codepoint = codepoint;
        *out_width = 3u;
        *out_valid = 1;
        return;
    }

    if (lead >= 0xf0u && lead <= 0xf4u) {
        if (remaining < 4u || !is_continuation(bytes[2]) ||
            !is_continuation(bytes[3]) || !is_continuation(bytes[1])) return;
        if (lead == 0xf0u && bytes[1] < 0x90u) return;
        if (lead == 0xf4u && bytes[1] > 0x8fu) return;
        codepoint = ((uint32_t)(lead & 0x07u) << 18u)
            | ((uint32_t)(bytes[1] & 0x3fu) << 12u)
            | ((uint32_t)(bytes[2] & 0x3fu) << 6u)
            | (uint32_t)(bytes[3] & 0x3fu);
        *out_codepoint = codepoint;
        *out_width = 4u;
        *out_valid = 1;
    }
}

static size_t count_scalars(const uint8_t *bytes, size_t byte_length)
{
    size_t byte_offset = 0u;
    size_t count = 0u;

    while (byte_offset < byte_length) {
        uint32_t codepoint;
        size_t width;
        int valid;
        decode_one(bytes + byte_offset, byte_length - byte_offset,
                   &codepoint, &width, &valid);
        (void)codepoint;
        (void)valid;
        byte_offset += width;
        count++;
    }
    return count;
}

cdin_frontend_gui_sdl_utf8_status cdin_frontend_gui_sdl_utf8_decode(
    const uint8_t *bytes,
    size_t byte_length,
    cdin_frontend_gui_sdl_utf8_scalar *out_scalars,
    size_t out_capacity,
    size_t *out_count)
{
    size_t required;
    size_t byte_offset = 0u;
    size_t scalar_index = 0u;

    if (out_count != NULL) *out_count = 0u;
    if (out_count == NULL || (bytes == NULL && byte_length != 0u) ||
        (out_scalars == NULL && out_capacity != 0u)) {
        return CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT;
    }

    required = count_scalars(bytes, byte_length);
    *out_count = required;
    if (out_capacity < required || (required != 0u && out_scalars == NULL)) {
        return CDIN_FRONTEND_GUI_SDL_UTF8_BUFFER_TOO_SMALL;
    }

    while (byte_offset < byte_length) {
        uint32_t codepoint;
        size_t width;
        int valid;
        decode_one(bytes + byte_offset, byte_length - byte_offset,
                   &codepoint, &width, &valid);
        out_scalars[scalar_index].codepoint = codepoint;
        out_scalars[scalar_index].byte_offset = byte_offset;
        out_scalars[scalar_index].byte_length = (uint8_t)width;
        out_scalars[scalar_index].valid = (uint8_t)(valid != 0);
        byte_offset += width;
        scalar_index++;
    }

    return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
}

cdin_frontend_gui_sdl_utf8_status cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
    const uint8_t *bytes,
    size_t byte_length,
    size_t byte_offset,
    size_t *out_scalar_index)
{
    size_t current_offset = 0u;
    size_t scalar_index = 0u;

    if (out_scalar_index != NULL) *out_scalar_index = 0u;
    if (out_scalar_index == NULL || (bytes == NULL && byte_length != 0u)) {
        return CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT;
    }
    if (byte_offset > byte_length) {
        return CDIN_FRONTEND_GUI_SDL_UTF8_OUT_OF_RANGE;
    }
    if (byte_offset == 0u) return CDIN_FRONTEND_GUI_SDL_UTF8_OK;

    while (current_offset < byte_length) {
        uint32_t codepoint;
        size_t width;
        int valid;
        decode_one(bytes + current_offset, byte_length - current_offset,
                   &codepoint, &width, &valid);
        (void)codepoint;
        (void)valid;
        current_offset += width;
        scalar_index++;
        if (current_offset == byte_offset) {
            *out_scalar_index = scalar_index;
            return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
        }
        if (current_offset > byte_offset) {
            return CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_BOUNDARY;
        }
    }

    *out_scalar_index = scalar_index;
    return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
}
