#ifndef CDIN_FRONTEND_TUI_MODEL_H
#define CDIN_FRONTEND_TUI_MODEL_H

#include "cdin/frontend/core/protocol/wire.h"
#include "cdin/frontend/ui/tui/tui.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cdin_frontend_tui_model cdin_frontend_tui_model;

typedef enum cdin_frontend_tui_model_status {
    CDIN_FRONTEND_TUI_MODEL_OK = 0,
    CDIN_FRONTEND_TUI_MODEL_INVALID_ARGUMENT,
    CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE,
    CDIN_FRONTEND_TUI_MODEL_STALE,
    CDIN_FRONTEND_TUI_MODEL_LIMIT_EXCEEDED,
    CDIN_FRONTEND_TUI_MODEL_NO_MEMORY
} cdin_frontend_tui_model_status;

/* The TUI caches only the currently visible line window. Every cache is
 * disposable and version-pinned; Backend/Runtime remain authoritative. */
cdin_frontend_tui_model *cdin_frontend_tui_model_create(
    const char *view_id,
    size_t visible_line_capacity,
    cdin_frontend_logger *logger);
void cdin_frontend_tui_model_destroy(cdin_frontend_tui_model *model);

/* Messages are staged until ui.flush. The committed model changes only after
 * an entire snapshot/response group has been validated and applied. */
cdin_frontend_tui_model_status cdin_frontend_tui_model_apply_message(
    cdin_frontend_tui_model *model,
    const cdin_frontend_protocol_message_view *message,
    int *out_committed);

/* Returns a borrowed render frame. Its pointers remain valid until the next
 * apply_message(), get_frame(), or destroy() call. */
cdin_frontend_tui_model_status cdin_frontend_tui_model_get_frame(
    cdin_frontend_tui_model *model,
    cdin_frontend_tui_frame *out_frame);

int cdin_frontend_tui_model_needs_lines(const cdin_frontend_tui_model *model);
cdin_frontend_tui_model_status cdin_frontend_tui_model_request_range(
    cdin_frontend_tui_model *model,
    uint64_t *out_first_line,
    uint64_t *out_line_count,
    uint64_t *out_version);
void cdin_frontend_tui_model_mark_request_pending(cdin_frontend_tui_model *model);
void cdin_frontend_tui_model_request_failed(cdin_frontend_tui_model *model);
uint64_t cdin_frontend_tui_model_revision(const cdin_frontend_tui_model *model);
uint64_t cdin_frontend_tui_model_last_flush(const cdin_frontend_tui_model *model);

#ifdef __cplusplus
}
#endif
#endif
