#ifndef CDIN_FRONTEND_GUI_SDL_PRESENTATION_H
#define CDIN_FRONTEND_GUI_SDL_PRESENTATION_H

#include <stddef.h>
#include <stdint.h>

#define CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLES 256u
#define CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLE_ID_BYTES 64u

typedef enum cdin_frontend_gui_sdl_style_status {
    CDIN_FRONTEND_GUI_SDL_STYLE_OK = 0,
    CDIN_FRONTEND_GUI_SDL_STYLE_INVALID,
    CDIN_FRONTEND_GUI_SDL_STYLE_LIMIT
} cdin_frontend_gui_sdl_style_status;

typedef struct cdin_frontend_gui_sdl_style_role {
    char id[CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLE_ID_BYTES + 1u];
    uint8_t foreground[4];
    uint8_t background[4];
    uint8_t has_foreground;
    uint8_t has_background;
    uint8_t bold;
    uint8_t italic;
    uint8_t underline;
    uint8_t strikethrough;
    uint8_t dim;
    uint8_t inverse;
} cdin_frontend_gui_sdl_style_role;

typedef struct cdin_frontend_gui_sdl_style_table {
    size_t role_count;
    cdin_frontend_gui_sdl_style_role roles[CDIN_FRONTEND_GUI_SDL_MAX_STYLE_ROLES];
} cdin_frontend_gui_sdl_style_table;

/* Decode the accepted ADR-0026 style-role map. On failure, out_table is
 * unchanged. The input is one complete CBOR map from ui.style_table.styles. */
cdin_frontend_gui_sdl_style_status cdin_frontend_gui_sdl_style_table_decode(
    const uint8_t *encoded,
    size_t encoded_length,
    cdin_frontend_gui_sdl_style_table *out_table);

const cdin_frontend_gui_sdl_style_role *cdin_frontend_gui_sdl_style_table_find(
    const cdin_frontend_gui_sdl_style_table *table,
    const char *role_id);

#endif
