#ifndef CDIN_FRONTEND_CORE_PROTOCOL_ASYNC_CLIENT_H
#define CDIN_FRONTEND_CORE_PROTOCOL_ASYNC_CLIENT_H

/*
 * FE-001/FE-002/FE-019: readiness-driven Protocol I/O.
 * Spec: https://github.com/cdin-editor/spec/blob/main/docs/architecture/frontend-protocol.md
 * Spec: https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md
 *
 * This additive API leaves the Foundation 1.0.0 blocking client ABI intact.
 * The composition root owns readiness; callbacks must never block. The caller
 * owns independent RX and TX buffers for the entire client lifetime.
 */
#include "cdin/frontend/core/diagnostics/log.h"
#include "cdin/frontend/core/protocol/wire.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cdin_frontend_protocol_io_result {
    CDIN_FRONTEND_PROTOCOL_IO_PROGRESS = 0,
    CDIN_FRONTEND_PROTOCOL_IO_AGAIN,
    CDIN_FRONTEND_PROTOCOL_IO_EOF,
    CDIN_FRONTEND_PROTOCOL_IO_FAILURE
} cdin_frontend_protocol_io_result;

typedef cdin_frontend_protocol_io_result (*cdin_frontend_protocol_try_read_fn)(
    void *context, uint8_t *buffer, size_t capacity, size_t *out_read);
typedef cdin_frontend_protocol_io_result (*cdin_frontend_protocol_try_write_fn)(
    void *context, const uint8_t *buffer, size_t length, size_t *out_written);

typedef struct cdin_frontend_protocol_async_io {
    void *context;
    cdin_frontend_protocol_try_read_fn read;
    cdin_frontend_protocol_try_write_fn write;
} cdin_frontend_protocol_async_io;

typedef struct cdin_frontend_protocol_async_client {
    cdin_frontend_protocol_async_io io;
    uint8_t *rx_buffer;
    size_t rx_capacity;
    size_t rx_used;
    size_t rx_expected;
    uint8_t *tx_buffer;
    size_t tx_capacity;
    size_t tx_length;
    size_t tx_offset;
    size_t max_frame_bytes;
    uint64_t next_tx_sequence;
    uint64_t last_rx_sequence;
    cdin_frontend_logger *logger;
    int initialized;
} cdin_frontend_protocol_async_client;

/*
 * Both buffers must hold max_frame_bytes + the four-byte prefix. RX and TX
 * storage must not overlap because one read and one queued write may coexist.
 */
cdin_frontend_protocol_status cdin_frontend_protocol_async_client_init(
    cdin_frontend_protocol_async_client *client,
    const cdin_frontend_protocol_async_io *io,
    uint8_t *rx_buffer, size_t rx_capacity,
    uint8_t *tx_buffer, size_t tx_capacity,
    size_t max_frame_bytes);

void cdin_frontend_protocol_async_client_set_logger(
    cdin_frontend_protocol_async_client *client, cdin_frontend_logger *logger);

/* Queue exactly one validated outbound frame; AGAIN means a frame is pending. */
cdin_frontend_protocol_status cdin_frontend_protocol_async_queue(
    cdin_frontend_protocol_async_client *client,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload, size_t payload_length);

/* Flush pending bytes. AGAIN preserves offsets; OK means the whole frame sent. */
cdin_frontend_protocol_status cdin_frontend_protocol_async_flush(
    cdin_frontend_protocol_async_client *client);

/*
 * Advance one receive operation. AGAIN preserves partial prefix/body state.
 * On OK, out_message borrows rx_buffer until the next receive call.
 */
cdin_frontend_protocol_status cdin_frontend_protocol_async_receive(
    cdin_frontend_protocol_async_client *client,
    cdin_frontend_protocol_message_view *out_message);

#ifdef __cplusplus
}
#endif
#endif
