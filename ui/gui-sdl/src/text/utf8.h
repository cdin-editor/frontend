#ifndef CDIN_FRONTEND_GUI_SDL_UTF8_H
#define CDIN_FRONTEND_GUI_SDL_UTF8_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cdin_frontend_gui_sdl_utf8_scalar {
    uint32_t codepoint;
    size_t byte_offset;
    uint8_t byte_length;
    uint8_t valid;
} cdin_frontend_gui_sdl_utf8_scalar;

typedef enum cdin_frontend_gui_sdl_utf8_status {
    CDIN_FRONTEND_GUI_SDL_UTF8_OK = 0,
    CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_ARGUMENT,
    CDIN_FRONTEND_GUI_SDL_UTF8_BUFFER_TOO_SMALL,
    CDIN_FRONTEND_GUI_SDL_UTF8_INVALID_BOUNDARY,
    CDIN_FRONTEND_GUI_SDL_UTF8_OUT_OF_RANGE
} cdin_frontend_gui_sdl_utf8_status;

/*
 * Decode a byte string into Unicode scalar records for presentation.
 *
 * Valid UTF-8 sequences produce their Unicode scalar value and exact original
 * byte range. An invalid byte produces U+FFFD and consumes exactly one byte,
 * so every source byte remains addressable and the original document is never
 * rewritten. This is a display mapping only: it does not normalize text or
 * perform grapheme segmentation, shaping, bidi reordering or line breaking.
 *
 * The function performs a sizing pass before writing. On BUFFER_TOO_SMALL the
 * required scalar count is returned through out_count and out_scalars remains
 * unchanged. Empty input succeeds with a NULL output buffer and zero capacity.
 * The input bytes, output records and out_count storage must not overlap.
 */
cdin_frontend_gui_sdl_utf8_status cdin_frontend_gui_sdl_utf8_decode(
    const uint8_t *bytes,
    size_t byte_length,
    cdin_frontend_gui_sdl_utf8_scalar *out_scalars,
    size_t out_capacity,
    size_t *out_count);

/*
 * Map a document byte offset to a scalar boundary index. Offsets inside a
 * valid multi-byte UTF-8 sequence are rejected; offsets at scalar starts and
 * at the end of the byte string are accepted. Invalid source bytes each form
 * their own one-byte replacement scalar and therefore remain addressable.
 */
cdin_frontend_gui_sdl_utf8_status cdin_frontend_gui_sdl_utf8_byte_offset_to_index(
    const uint8_t *bytes,
    size_t byte_length,
    size_t byte_offset,
    size_t *out_scalar_index);

#ifdef __cplusplus
}
#endif

#endif
