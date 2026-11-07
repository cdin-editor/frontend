#ifndef CDIN_FRONTEND_GUI_SDL_LAYOUT_H
#define CDIN_FRONTEND_GUI_SDL_LAYOUT_H

#include "cdin/frontend/core/protocol/wire.h"

#include <stddef.h>
#include <stdint.h>

#define CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS 32u
#define CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES 255u
#define CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES 128u

typedef enum cdin_frontend_gui_sdl_layout_node_kind {
    CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF = 1,
    CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT = 2
} cdin_frontend_gui_sdl_layout_node_kind;

typedef enum cdin_frontend_gui_sdl_layout_axis {
    CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL = 1,
    CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL = 2
} cdin_frontend_gui_sdl_layout_axis;

typedef enum cdin_frontend_gui_sdl_layout_status {
    CDIN_FRONTEND_GUI_SDL_LAYOUT_OK = 0,
    CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID,
    CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT
} cdin_frontend_gui_sdl_layout_status;

typedef struct cdin_frontend_gui_sdl_layout_node {
    cdin_frontend_gui_sdl_layout_node_kind kind;
    char id[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES + 1u];
    size_t first_child;
    size_t second_child;
    uint16_t ratio_bp;
    cdin_frontend_gui_sdl_layout_axis axis;
    size_t view_count;
    uint8_t view_indices[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS];
    uint8_t active_view_index;
    int has_active_view;
} cdin_frontend_gui_sdl_layout_node;

typedef struct cdin_frontend_gui_sdl_layout {
    uint64_t revision;
    size_t root_index;
    size_t active_leaf_index;
    size_t node_count;
    size_t leaf_count;
    size_t view_count;
    int has_root;
    int has_active_leaf;
    char view_ids[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS]
                 [CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES + 1u];
    cdin_frontend_gui_sdl_layout_node nodes[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
} cdin_frontend_gui_sdl_layout;

/* Parses and validates the complete Protocol ui.layout payload into a
 * bounded, allocation-free tree. The output is unchanged on failure. */
cdin_frontend_gui_sdl_layout_status cdin_frontend_gui_sdl_layout_parse(
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_layout *out_layout);

#endif
