/* TUI adapter between terminal events, the generic Protocol session and model. */
#include "cdin/frontend/ui/tui/session.h"

#include "cdin/frontend/core/diagnostics/log.h"

cdin_frontend_protocol_status cdin_frontend_tui_send_input(
    cdin_frontend_session *session,
    const cdin_frontend_input_event *event)
{
    return cdin_frontend_session_send_input_event(session, event);
}

cdin_frontend_tui_model_status cdin_frontend_tui_receive(
    cdin_frontend_session *session,
    cdin_frontend_tui_model *model,
    const cdin_frontend_protocol_message_view *message,
    int *out_committed)
{
    cdin_frontend_tui_model_status status;
    uint64_t first_line;
    uint64_t line_count;
    uint64_t version;
    if (session == NULL || model == NULL || message == NULL || out_committed == NULL) {
        return CDIN_FRONTEND_TUI_MODEL_INVALID_ARGUMENT;
    }
    status = cdin_frontend_tui_model_apply_message(model, message, out_committed);
    if (status != CDIN_FRONTEND_TUI_MODEL_OK || !*out_committed ||
        message->type != CDIN_FRONTEND_PROTOCOL_UI_FLUSH) return status;

    if (session->last_flush_sequence == 0u ||
        cdin_frontend_session_ack(session, session->last_flush_sequence) != CDIN_FRONTEND_PROTOCOL_OK) {
        if (session->logger != NULL) {
            CDIN_FRONTEND_ERROR(session->logger, "TUI", "could not acknowledge committed UI flush");
        }
        return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
    }
    if (cdin_frontend_tui_model_needs_lines(model)) {
        if (cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) !=
            CDIN_FRONTEND_TUI_MODEL_OK) return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        if (cdin_frontend_session_request_lines(session, session->view_id,
                                                first_line, line_count, version) == CDIN_FRONTEND_PROTOCOL_OK) {
            cdin_frontend_tui_model_mark_request_pending(model);
        } else {
            cdin_frontend_tui_model_request_failed(model);
            return CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE;
        }
    }
    return CDIN_FRONTEND_TUI_MODEL_OK;
}
