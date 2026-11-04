#ifndef CDIN_FRONTEND_PROTOCOL_CLIENT_H
#define CDIN_FRONTEND_PROTOCOL_CLIENT_H

#include "cdin/frontend/core/diagnostics/log.h"
#include "cdin/frontend/core/protocol/wire.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cdin_frontend_client {
    cdin_frontend_protocol_io io;
    uint8_t *frame_buffer;
    size_t frame_capacity;
    size_t max_frame_bytes;
    uint64_t next_tx_sequence;
    uint64_t last_rx_sequence;
    cdin_frontend_logger *logger;
    int initialized;
} cdin_frontend_client;

/* The caller owns frame_buffer for the client lifetime; capacity must be
 * max_frame_bytes + the four-byte frame prefix. */
cdin_frontend_protocol_status cdin_frontend_client_init(
    cdin_frontend_client *client,
    const cdin_frontend_protocol_io *io,
    uint8_t *frame_buffer,
    size_t frame_capacity,
    size_t max_frame_bytes);

/* The logger is caller-owned and must outlive the client. No hidden fallback
 * logger is created by the Frontend. */
void cdin_frontend_client_set_logger(cdin_frontend_client *client,
                                     cdin_frontend_logger *logger);

/* payload is a validated, definite-length CBOR map and remains caller-owned. */
cdin_frontend_protocol_status cdin_frontend_client_send(
    cdin_frontend_client *client,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload,
    size_t payload_length);

/* The returned payload slice borrows frame_buffer until the next receive. */
cdin_frontend_protocol_status cdin_frontend_client_receive(
    cdin_frontend_client *client,
    cdin_frontend_protocol_message_view *out_message);

#ifdef __cplusplus
}
#endif
#endif
