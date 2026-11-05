#ifndef CDIN_FRONTEND_SESSION_SESSION_H
#define CDIN_FRONTEND_SESSION_SESSION_H

#include "cdin/frontend/core/protocol/client.h"
#include "cdin/frontend/core/capabilities.h"
#include "cdin/frontend/core/input/event.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cdin_frontend_session_state {
    CDIN_FRONTEND_SESSION_INITIAL = 0,
    CDIN_FRONTEND_SESSION_HELLO_SENT,
    CDIN_FRONTEND_SESSION_NEGOTIATED,
    CDIN_FRONTEND_SESSION_ATTACH_SENT,
    CDIN_FRONTEND_SESSION_ATTACHED,
    CDIN_FRONTEND_SESSION_DETACH_SENT,
    CDIN_FRONTEND_SESSION_DETACHED,
    CDIN_FRONTEND_SESSION_FAILED
} cdin_frontend_session_state;

typedef struct cdin_frontend_session {
    cdin_frontend_client *client;
    cdin_frontend_logger *logger;
    cdin_frontend_session_state state;
    uint64_t session_id;
    uint64_t client_id;
    uint64_t last_flush_sequence;
    cdin_frontend_capability_mask requested_capabilities;
    cdin_frontend_capability_mask accepted_capabilities;
    int driver;
    char frontend_name[32];
    char view_id[64];
    uint32_t width;
    uint32_t height;
    const char *units;
} cdin_frontend_session;

void cdin_frontend_session_init(cdin_frontend_session *session,
                                cdin_frontend_client *client,
                                cdin_frontend_logger *logger,
                                const char *frontend_name);
/* Must be called before hello. The mask describes capabilities actually
 * implemented by this adapter, not capabilities it merely desires. */
cdin_frontend_protocol_status cdin_frontend_session_set_capabilities(
    cdin_frontend_session *session, cdin_frontend_capability_mask supported);
cdin_frontend_capability_mask cdin_frontend_session_accepted_capabilities(
    const cdin_frontend_session *session);
int cdin_frontend_session_is_driver(const cdin_frontend_session *session);
cdin_frontend_protocol_status cdin_frontend_session_hello(cdin_frontend_session *session);
cdin_frontend_protocol_status cdin_frontend_session_receive(
    cdin_frontend_session *session,
    cdin_frontend_protocol_message_view *out_message);
cdin_frontend_protocol_status cdin_frontend_session_attach(
    cdin_frontend_session *session,
    const char *view_id,
    const char *units,
    uint32_t width,
    uint32_t height);
cdin_frontend_protocol_status cdin_frontend_session_ack(cdin_frontend_session *session,
                                                        uint64_t flush_sequence);
cdin_frontend_protocol_status cdin_frontend_session_request_lines(
    cdin_frontend_session *session,
    const char *view_id,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version);
/* FE-016 / PROTO-041: send a typed Runtime command request. The Frontend
 * does not interpret the command; Runtime owns authorization and execution.
 * `path` is an optional UTF-8 argument for document lifecycle commands. */
cdin_frontend_protocol_status cdin_frontend_session_request_command(
    cdin_frontend_session *session, const char *name, const char *path);
cdin_frontend_protocol_status cdin_frontend_session_send_key(
    cdin_frontend_session *session,
    const char *key,
    uint32_t modifiers,
    const char *action);
cdin_frontend_protocol_status cdin_frontend_session_send_text(
    cdin_frontend_session *session,
    const uint8_t *text,
    size_t text_length);
cdin_frontend_protocol_status cdin_frontend_session_send_paste(
    cdin_frontend_session *session,
    const uint8_t *bytes,
    size_t length);
/* Forward one decoded platform event without interpreting editor keymaps. */
cdin_frontend_protocol_status cdin_frontend_session_send_input_event(
    cdin_frontend_session *session,
    const cdin_frontend_input_event *event);
cdin_frontend_protocol_status cdin_frontend_session_resize(
    cdin_frontend_session *session,
    uint32_t width,
    uint32_t height);
cdin_frontend_protocol_status cdin_frontend_session_focus(
    cdin_frontend_session *session,
    int focused);
cdin_frontend_protocol_status cdin_frontend_session_detach(cdin_frontend_session *session);

#ifdef __cplusplus
}
#endif
#endif
