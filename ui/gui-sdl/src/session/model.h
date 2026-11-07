#ifndef CDIN_FRONTEND_GUI_SDL_MODEL_H
#define CDIN_FRONTEND_GUI_SDL_MODEL_H

#include "cdin/frontend/core/protocol/wire.h"
#include "layout.h"

#include <stddef.h>
#include <stdint.h>

#define CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES 64u
#define CDIN_FRONTEND_GUI_SDL_MAX_LINE_BYTES 8192u
#define CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES
#define CDIN_FRONTEND_GUI_SDL_MAX_VIEWS CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS
#define CDIN_FRONTEND_GUI_SDL_MAX_CACHE_BYTES (4u * 1024u * 1024u)

typedef enum cdin_frontend_gui_sdl_model_status {
    CDIN_FRONTEND_GUI_SDL_MODEL_OK = 0,
    CDIN_FRONTEND_GUI_SDL_MODEL_STALE,
    CDIN_FRONTEND_GUI_SDL_MODEL_INVALID,
    CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT,
    CDIN_FRONTEND_GUI_SDL_MODEL_NO_MEMORY
} cdin_frontend_gui_sdl_model_status;

typedef struct cdin_frontend_gui_sdl_line {
    uint8_t *bytes;
    size_t length;
} cdin_frontend_gui_sdl_line;

typedef struct cdin_frontend_gui_sdl_line_window {
    uint64_t first_line;
    size_t count;
    size_t bytes;
    cdin_frontend_gui_sdl_line lines[CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES];
} cdin_frontend_gui_sdl_line_window;

/* Semantic state is copied into the staged snapshot. It owns no allocations. */
typedef struct cdin_frontend_gui_sdl_view_snapshot {
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    uint64_t version;
    uint64_t cursor_position;
    uint64_t selection_anchor;
    uint64_t selection_caret;
    uint64_t scroll_anchor;
    int has_cursor;
    int has_selection;
    int has_scroll_anchor;
    int in_layout;
    int visible;
    int focused;
    int needs_lines;
} cdin_frontend_gui_sdl_view_snapshot;

/* The committed view owns its line-window allocations and one outstanding
 * version-pinned request. Line caches are disposable and globally bounded. */
typedef struct cdin_frontend_gui_sdl_view {
    cdin_frontend_gui_sdl_view_snapshot semantic;
    cdin_frontend_gui_sdl_line_window visible;
    cdin_frontend_gui_sdl_line_window pending;
    uint64_t requested_first_line;
    uint64_t requested_line_count;
    uint64_t requested_version;
    uint64_t last_used;
    int has_line_request;
    int pending_lines_valid;
} cdin_frontend_gui_sdl_view;

typedef struct cdin_frontend_gui_sdl_snapshot_state {
    uint64_t layout_revision;
    cdin_frontend_gui_sdl_layout layout;
    int has_layout;
    size_t view_count;
    cdin_frontend_gui_sdl_view_snapshot views[CDIN_FRONTEND_GUI_SDL_MAX_VIEWS];
    char theme[64];
    char mode[32];
    char status[128];
    char notification[128];
} cdin_frontend_gui_sdl_snapshot_state;

typedef struct cdin_frontend_gui_sdl_view_request {
    char view_id[CDIN_FRONTEND_GUI_SDL_MAX_VIEW_ID_BYTES + 1u];
    uint64_t first_line;
    uint64_t line_count;
    uint64_t version;
} cdin_frontend_gui_sdl_view_request;

typedef struct cdin_frontend_gui_sdl_model {
    cdin_frontend_gui_sdl_view views[CDIN_FRONTEND_GUI_SDL_MAX_VIEWS];
    size_t view_count;
    uint64_t snapshot_revision;
    int has_snapshot_revision;
    uint64_t layout_revision;
    cdin_frontend_gui_sdl_layout layout;
    int has_layout;
    cdin_frontend_gui_sdl_snapshot_state staged_snapshot;
    int snapshot_pending_valid;
    size_t visible_line_capacity;
    size_t cache_bytes;
    uint64_t access_clock;
    char theme[64];
    char mode[32];
    char status[128];
    char notification[128];
} cdin_frontend_gui_sdl_model;

typedef struct cdin_frontend_gui_sdl_model_action {
    uint64_t acknowledge_sequence;
    size_t request_count;
    cdin_frontend_gui_sdl_view_request requests[CDIN_FRONTEND_GUI_SDL_MAX_VIEWS];
} cdin_frontend_gui_sdl_model_action;

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_init(
    cdin_frontend_gui_sdl_model *model,
    const char *attach_view_id,
    size_t visible_line_capacity);

void cdin_frontend_gui_sdl_model_destroy(cdin_frontend_gui_sdl_model *model);

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_expect_lines(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version);

void cdin_frontend_gui_sdl_model_cancel_line_request(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id);

cdin_frontend_gui_sdl_view *cdin_frontend_gui_sdl_model_find_view(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id);

const cdin_frontend_gui_sdl_view *cdin_frontend_gui_sdl_model_find_view_const(
    const cdin_frontend_gui_sdl_model *model,
    const char *view_id);

cdin_frontend_gui_sdl_view *cdin_frontend_gui_sdl_model_focused_view(
    cdin_frontend_gui_sdl_model *model);

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_apply(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_model_action *out_action);

#endif
