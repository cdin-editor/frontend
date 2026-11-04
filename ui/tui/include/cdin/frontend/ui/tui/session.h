#ifndef CDIN_FRONTEND_TUI_SESSION_H
#define CDIN_FRONTEND_TUI_SESSION_H

#include "cdin/frontend/core/session/session.h"
#include "cdin/frontend/ui/tui/model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward a decoded terminal event as raw Protocol input. Runtime retains all
 * command and keymap policy. */
cdin_frontend_protocol_status cdin_frontend_tui_send_input(
    cdin_frontend_session *session,
    const cdin_frontend_input_event *event);

/* Apply one received Backend message. A committed flush is ACKed only after
 * the complete UI group has been validated and applied. The active visible
 * line window is requested after invalidation, with its document version. */
cdin_frontend_tui_model_status cdin_frontend_tui_receive(
    cdin_frontend_session *session,
    cdin_frontend_tui_model *model,
    const cdin_frontend_protocol_message_view *message,
    int *out_committed);

#ifdef __cplusplus
}
#endif
#endif
