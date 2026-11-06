#ifndef CDIN_FRONTEND_GUI_SDL_MODEL_H
#define CDIN_FRONTEND_GUI_SDL_MODEL_H

#include "cdin/frontend/core/protocol/wire.h"

#include <stddef.h>
#include <stdint.h>

#define CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES 64u
#define CDIN_FRONTEND_GUI_SDL_MAX_LINE_BYTES 8192u

typedef enum cdin_frontend_gui_sdl_model_status {
    CDIN_FRONTEND_GUI_SDL_MODEL_OK = 0,
    CDIN_FRONTEND_GUI_SDL_MODEL_STALE,
    CDIN_FRONTEND_GUI_SDL_MODEL_INVALID,
    CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT
} cdin_frontend_gui_sdl_model_status;

typedef struct cdin_frontend_gui_sdl_line {
    uint8_t bytes[CDIN_FRONTEND_GUI_SDL_MAX_LINE_BYTES];
    size_t length;
} cdin_frontend_gui_sdl_line;

typedef struct cdin_frontend_gui_sdl_line_window {
    uint64_t first_line;
    size_t count;
    cdin_frontend_gui_sdl_line lines[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
} cdin_frontend_gui_sdl_line_window;

typedef struct cdin_frontend_gui_sdl_snapshot_state {
    uint64_t version;
    uint64_t cursor_position;
    uint64_t layout_revision;
    int has_cursor;
    int has_layout;
    int needs_lines;
    char theme[64];
    char mode[32];
    char status[128];
    char notification[128];
} cdin_frontend_gui_sdl_snapshot_state;

typedef struct cdin_frontend_gui_sdl_model {
    char view_id[64];
    uint64_t version;
    uint64_t snapshot_revision;
    int has_snapshot_revision;
    uint64_t cursor_position;
    uint64_t layout_revision;
    int has_cursor;
    int has_layout;
    int needs_lines;
    cdin_frontend_gui_sdl_snapshot_state staged_snapshot;
    int snapshot_pending_valid;
    cdin_frontend_gui_sdl_line_window visible;
    cdin_frontend_gui_sdl_line_window pending;
    int pending_lines_valid;
    uint64_t requested_first_line;
    uint64_t requested_line_count;
    uint64_t requested_version;
    int has_line_request;
    size_t visible_line_capacity;
    char theme[64];
    char mode[32];
    char status[128];
    char notification[128];
} cdin_frontend_gui_sdl_model;

typedef struct cdin_frontend_gui_sdl_model_action {
    uint64_t acknowledge_sequence;
    int request_lines;
    uint64_t first_line;
    uint64_t line_count;
    uint64_t version;
} cdin_frontend_gui_sdl_model_action;

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_init(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    size_t visible_line_capacity);

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_expect_lines(
    cdin_frontend_gui_sdl_model *model,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version);

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_apply(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_model_action *out_action);

#endif
