#include "utf8.h"

#include <string.h>

#define CDIN_FRONTEND_GUI_SDL_UTF8_REPLACEMENT 0xfffdu

static int is_continuation(uint8_t value)
{
    return value >= 0x80u && value <= 0xbfu;
}

static void emit_replacement(
    size_t start,
    size_t *inout_offset,
    cdin_frontend_gui_sdl_utf8_scalar *out_scalar)
{
    out_scalar->value = CDIN_FRONTEND_GUI_SDL_UTF8_REPLACEMENT;
    out_scalar->byte_start = start;
    out_scalar->byte_end = start + 1u;
    out_scalar->valid_utf8 = 0;
    *inout_offset = start + 1u;
}

cdin_frontend_gui_sdl_utf8_status cdin_frontend_gui_sdl_utf8_decode_next(
    const uint8_t *bytes,
    size_t length,
    size_t *inout_offset,
    cdin_frontend_gui_sdl_utf8_scalar *out_scalar)
{
    size_t start;
    size_t available;
    uint8_t first;
    uint32_t scalar;
    size_t width;

    if (inout_offset == NULL || out_scalar == NULL ||
        (bytes == NULL && length != 0u) || *inout_offset > length) {
        return CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT;
    }

    memset(out_scalar, 0, sizeof(*out_scalar));
    start = *inout_offset;
    if (start == length) {
        return CDIN_FRONTEND_GUI_SDL_UTF8_END;
    }

    first = bytes[start];
    available = length - start;
    if (first <= 0x7fu) {
        out_scalar->value = (uint32_t)first;
        width = 1u;
    } else if (first >= 0xc2u && first <= 0xdfu) {
        width = 2u;
        if (available < width || !is_continuation(bytes[start + 1u])) {
            emit_replacement(start, inout_offset, out_scalar);
            return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
        }
        scalar = ((uint32_t)(first & 0x1fu) << 6u) |
                 (uint32_t)(bytes[start + 1u] & 0x3fu);
        out_scalar->value = scalar;
    } else if (first >= 0xe0u && first <= 0xefu) {
        width = 3u;
        if (available < width ||
            !is_continuation(bytes[start + 1u]) ||
            !is_continuation(bytes[start + 2u]) ||
            (first == 0xe0u && bytes[start + 1u] < 0xa0u) ||
            (first == 0xedu && bytes[start + 1u] > 0x9fu)) {
            emit_replacement(start, inout_offset, out_scalar);
            return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
        }
        scalar = ((uint32_t)(first & 0x0fu) << 12u) |
                 ((uint32_t)(bytes[start + 1u] & 0x3fu) << 6u) |
                 (uint32_t)(bytes[start + 2u] & 0x3fu);
        out_scalar->value = scalar;
    } else if (first >= 0xf0u && first <= 0xf4u) {
        width = 4u;
        if (available < width ||
            !is_continuation(bytes[start + 1u]) ||
            !is_continuation(bytes[start + 2u]) ||
            !is_continuation(bytes[start + 3u]) ||
            (first == 0xf0u && bytes[start + 1u] < 0x90u) ||
            (first == 0xf4u && bytes[start + 1u] > 0x8fu)) {
            emit_replacement(start, inout_offset, out_scalar);
            return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
        }
        scalar = ((uint32_t)(first & 0x07u) << 18u) |
                 ((uint32_t)(bytes[start + 1u] & 0x3fu) << 12u) |
                 ((uint32_t)(bytes[start + 2u] & 0x3fu) << 6u) |
                 (uint32_t)(bytes[start + 3u] & 0x3fu);
        out_scalar->value = scalar;
    } else {
        emit_replacement(start, inout_offset, out_scalar);
        return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
    }

    out_scalar->byte_start = start;
    out_scalar->byte_end = start + width;
    out_scalar->valid_utf8 = 1;
    *inout_offset = start + width;
    return CDIN_FRONTEND_GUI_SDL_UTF8_OK;
}
