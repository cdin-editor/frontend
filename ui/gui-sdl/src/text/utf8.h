#ifndef CDIN_FRONTEND_GUI_SDL_UTF8_H
#define CDIN_FRONTEND_GUI_SDL_UTF8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cdin_frontend_gui_sdl_utf8_status {
    CDIN_FRONTEND_GUI_SDL_UTF8_OK = 0,
    CDIN_FRONTEND_GUI_SDL_UTF8_END,
    CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT
} cdin_frontend_gui_sdl_utf8_status;

/* One decoded Unicode scalar and its exact half-open source byte range.
 * Invalid source bytes produce U+FFFD with valid_utf8 == 0; the input is never
 * modified, and the byte range still identifies the original byte. */
typedef struct cdin_frontend_gui_sdl_utf8_scalar {
    uint32_t value;
    size_t byte_start;
    size_t byte_end;
    int valid_utf8;
} cdin_frontend_gui_sdl_utf8_scalar;

/*
 * Decode one scalar from a length-delimited byte sequence.
 *
 * The offset is in/out and is advanced by the exact source byte range. A
 * malformed or truncated sequence consumes exactly one byte and returns a
 * replacement scalar, allowing callers to preserve reversible byte mapping.
 * Embedded NUL is an ordinary valid U+0000 scalar.
 *
 * END is returned when *inout_offset == length. No allocation or global state
 * is used. This is a scalar decoder only: it does not segment grapheme
 * clusters, shape glyphs, reorder bidi text or normalize source bytes.
 */
cdin_frontend_gui_sdl_utf8_status cdin_frontend_gui_sdl_utf8_decode_next(
    const uint8_t *bytes,
    size_t length,
    size_t *inout_offset,
    cdin_frontend_gui_sdl_utf8_scalar *out_scalar);

#ifdef __cplusplus
}
#endif

#endif
