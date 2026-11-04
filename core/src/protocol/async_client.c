/* FE-019: incremental framing for a readiness-driven frontend event loop. */
#include "cdin/frontend/core/protocol/async_client.h"

#include <stdint.h>
#include <string.h>

static void async_log(cdin_frontend_protocol_async_client *client,
                      const char *operation,
                      cdin_frontend_protocol_status status)
{
    if (client != NULL && client->logger != NULL) {
        CDIN_FRONTEND_ERROR(client->logger, "PROTOCOL", "async %s: %s",
                            operation, cdin_frontend_protocol_status_name(status));
    }
}

static size_t frame_total(const cdin_frontend_protocol_async_client *client)
{
    return client->rx_expected == 0u ? CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES
        : client->rx_expected + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES;
}

cdin_frontend_protocol_status cdin_frontend_protocol_async_client_init(
    cdin_frontend_protocol_async_client *client,
    const cdin_frontend_protocol_async_io *io,
    uint8_t *rx_buffer, size_t rx_capacity,
    uint8_t *tx_buffer, size_t tx_capacity,
    size_t max_frame_bytes)
{
    size_t required;
    uintptr_t rx_start, tx_start;
    if (client == NULL || io == NULL || io->read == NULL || io->write == NULL ||
        rx_buffer == NULL || tx_buffer == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (max_frame_bytes == 0u) max_frame_bytes = CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES;
    if (max_frame_bytes > CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES ||
        max_frame_bytes > SIZE_MAX - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    required = max_frame_bytes + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES;
    if (rx_capacity < required || tx_capacity < required) {
        return CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL;
    }
    rx_start = (uintptr_t)rx_buffer;
    tx_start = (uintptr_t)tx_buffer;
    if ((rx_start <= tx_start && tx_start - rx_start < rx_capacity) ||
        (tx_start < rx_start && rx_start - tx_start < tx_capacity)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    memset(client, 0, sizeof(*client));
    client->io = *io;
    client->rx_buffer = rx_buffer;
    client->rx_capacity = rx_capacity;
    client->tx_buffer = tx_buffer;
    client->tx_capacity = tx_capacity;
    client->max_frame_bytes = max_frame_bytes;
    client->next_tx_sequence = 1u;
    client->initialized = 1;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

void cdin_frontend_protocol_async_client_set_logger(
    cdin_frontend_protocol_async_client *client, cdin_frontend_logger *logger)
{
    if (client != NULL && client->initialized) client->logger = logger;
}

cdin_frontend_protocol_status cdin_frontend_protocol_async_queue(
    cdin_frontend_protocol_async_client *client,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload, size_t payload_length)
{
    cdin_frontend_protocol_message_view message;
    cdin_frontend_protocol_status status;
    size_t encoded = 0u;
    if (client == NULL || !client->initialized || payload == NULL || payload_length == 0u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (client->tx_length != 0u) return CDIN_FRONTEND_PROTOCOL_AGAIN;
    if (!cdin_frontend_protocol_message_is_inbound(type)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (client->next_tx_sequence == UINT64_MAX) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    status = cdin_frontend_protocol_validate_payload(type, payload, payload_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) return status;
    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.sequence = client->next_tx_sequence;
    message.type = type;
    message.name_space = "core";
    message.payload = payload;
    message.payload_length = payload_length;
    status = cdin_frontend_protocol_encode_frame(&message, client->tx_buffer,
        client->tx_capacity, &encoded, client->max_frame_bytes);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        async_log(client, "encode", status);
        return status;
    }
    client->tx_length = encoded;
    client->tx_offset = 0u;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_async_flush(
    cdin_frontend_protocol_async_client *client)
{
    size_t count = 0u;
    cdin_frontend_protocol_io_result result;
    if (client == NULL || !client->initialized) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (client->tx_length == 0u) return CDIN_FRONTEND_PROTOCOL_OK;
    result = client->io.write(client->io.context, client->tx_buffer + client->tx_offset,
                              client->tx_length - client->tx_offset, &count);
    if (result == CDIN_FRONTEND_PROTOCOL_IO_AGAIN) {
        if (count != 0u) return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        return CDIN_FRONTEND_PROTOCOL_AGAIN;
    }
    if (result == CDIN_FRONTEND_PROTOCOL_IO_EOF || result == CDIN_FRONTEND_PROTOCOL_IO_FAILURE ||
        (result == CDIN_FRONTEND_PROTOCOL_IO_PROGRESS &&
         (count == 0u || count > client->tx_length - client->tx_offset))) {
        async_log(client, "write", CDIN_FRONTEND_PROTOCOL_IO_ERROR);
        return result == CDIN_FRONTEND_PROTOCOL_IO_EOF ? CDIN_FRONTEND_PROTOCOL_EOF
                                                       : CDIN_FRONTEND_PROTOCOL_IO_ERROR;
    }
    if (result != CDIN_FRONTEND_PROTOCOL_IO_PROGRESS) return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
    client->tx_offset += count;
    if (client->tx_offset < client->tx_length) return CDIN_FRONTEND_PROTOCOL_AGAIN;
    client->tx_length = 0u;
    client->tx_offset = 0u;
    client->next_tx_sequence++;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_protocol_async_receive(
    cdin_frontend_protocol_async_client *client,
    cdin_frontend_protocol_message_view *out_message)
{
    size_t count = 0u;
    size_t target;
    cdin_frontend_protocol_io_result result;
    cdin_frontend_protocol_status status;
    cdin_frontend_protocol_message_view message;
    if (client == NULL || !client->initialized || out_message == NULL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    target = frame_total(client);
    result = client->io.read(client->io.context, client->rx_buffer + client->rx_used,
                             target - client->rx_used, &count);
    if (result == CDIN_FRONTEND_PROTOCOL_IO_AGAIN) {
        if (count != 0u) return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
        return CDIN_FRONTEND_PROTOCOL_AGAIN;
    }
    if (result == CDIN_FRONTEND_PROTOCOL_IO_EOF) {
        status = client->rx_used == 0u ? CDIN_FRONTEND_PROTOCOL_EOF
                                      : CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
        async_log(client, "read EOF", status);
        return status;
    }
    if (result != CDIN_FRONTEND_PROTOCOL_IO_PROGRESS || count == 0u || count > target - client->rx_used) {
        async_log(client, "read", CDIN_FRONTEND_PROTOCOL_IO_ERROR);
        return CDIN_FRONTEND_PROTOCOL_IO_ERROR;
    }
    client->rx_used += count;
    if (client->rx_expected == 0u && client->rx_used == CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        client->rx_expected = ((size_t)client->rx_buffer[0] << 24u) |
            ((size_t)client->rx_buffer[1] << 16u) |
            ((size_t)client->rx_buffer[2] << 8u) | (size_t)client->rx_buffer[3];
        if (client->rx_expected == 0u || client->rx_expected > client->max_frame_bytes ||
            client->rx_expected > client->rx_capacity - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
            status = client->rx_expected > client->max_frame_bytes
                ? CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED : CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
            client->rx_used = 0u;
            client->rx_expected = 0u;
            async_log(client, "frame length", status);
            return status;
        }
    }
    if (client->rx_expected == 0u || client->rx_used < frame_total(client)) {
        return CDIN_FRONTEND_PROTOCOL_AGAIN;
    }
    status = cdin_frontend_protocol_decode_frame(client->rx_buffer, client->rx_used,
                                                  &message, client->max_frame_bytes);
    if (status == CDIN_FRONTEND_PROTOCOL_OK &&
        (message.major != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR ||
         message.minor != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR)) {
        status = CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH;
    }
    if (status == CDIN_FRONTEND_PROTOCOL_OK && cdin_frontend_protocol_message_is_inbound(message.type)) {
        status = CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (status == CDIN_FRONTEND_PROTOCOL_OK &&
        (client->last_rx_sequence == UINT64_MAX || message.sequence != client->last_rx_sequence + 1u)) {
        status = CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        async_log(client, "decode", status);
        client->rx_used = 0u;
        client->rx_expected = 0u;
        return status;
    }
    client->last_rx_sequence = message.sequence;
    *out_message = message;
    client->rx_used = 0u;
    client->rx_expected = 0u;
    return CDIN_FRONTEND_PROTOCOL_OK;
}
