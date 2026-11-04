#ifndef CDIN_FRONTEND_TUI_H
#define CDIN_FRONTEND_TUI_H

#include "cdin/frontend/core/diagnostics/log.h"
#include "cdin/frontend/ui/tui/platform/terminal.h"
#include "cdin/frontend/core/input/event.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_TUI_MAX_VISIBLE_LINES 512u
#define CDIN_FRONTEND_TUI_MAX_COLUMNS 512u
#define CDIN_FRONTEND_TUI_OUTPUT_CAPACITY (256u * 1024u)

typedef enum cdin_frontend_tui_status {
    CDIN_FRONTEND_TUI_OK = 0,
    CDIN_FRONTEND_TUI_INVALID_ARGUMENT,
    CDIN_FRONTEND_TUI_LIMIT_EXCEEDED,
    CDIN_FRONTEND_TUI_IO_ERROR
} cdin_frontend_tui_status;

typedef struct cdin_frontend_tui_line {
    const uint8_t *bytes;
    size_t length;
} cdin_frontend_tui_line;

typedef struct cdin_frontend_tui_frame {
    const char *title;
    const char *mode;
    const char *status;
    const char *prompt;
    const cdin_frontend_tui_line *lines;
    size_t line_count;
    uint64_t cursor_line;
    uint64_t cursor_byte;
    uint64_t top_line;
    uint64_t first_line_byte_offset;
    uint64_t selection_anchor;
    uint64_t selection_caret;
    uint64_t document_version;
    int selection_known;
    int show_line_numbers;
} cdin_frontend_tui_frame;

/* Draws a read-only semantic frame using terminal cells. This module owns only
 * presentation state; the caller remains responsible for document/model data. */
cdin_frontend_tui_status cdin_frontend_tui_render(
    cdin_frontend_terminal *terminal,
    const cdin_frontend_tui_frame *frame,
    cdin_frontend_logger *logger);

/* Convert raw terminal bytes into a protocol-ready input event. The function
 * translates physical keys only; it never interprets editor key bindings. */
cdin_frontend_tui_status cdin_frontend_tui_decode_input(
    const uint8_t *bytes,
    size_t length,
    cdin_frontend_input_event *out_event);

#ifdef __cplusplus
}
#endif
#endif
