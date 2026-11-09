/*
 * Frontend Protocol client: direction, version, sequence and frame handling.
 * The transport callbacks are supplied by the composition root; no Backend
 * or Kernel implementation headers are included here.
 */
#include "cdin/frontend/core/protocol/client.h"

#include <string.h>

static void log_failure(cdin_frontend_client *client,
                        const char *operation,
                        cdin_frontend_protocol_status status)
{
    if (client != NULL && client->logger != NULL) {
        CDIN_FRONTEND_ERROR(client->logger, "PROTOCOL", "%s failed: %s",
                            operation, cdin_frontend_protocol_status_name(status));
    }
}

cdin_frontend_protocol_status cdin_frontend_client_init(
    cdin_frontend_client *client,
    const cdin_frontend_protocol_io *io,
    uint8_t *frame_buffer,
    size_t frame_capacity,
    size_t max_frame_bytes)
{
    size_t required_capacity;
    if (client == NULL || io == NULL || io->read == NULL || io->write == NULL ||
        frame_buffer == NULL) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    if (max_frame_bytes == 0u) max_frame_bytes = CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES;
    if (max_frame_bytes > CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES ||
        max_frame_bytes > SIZE_MAX - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    required_capacity = max_frame_bytes + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES;
    if (frame_capacity < required_capacity) return CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL;

    memset(client, 0, sizeof(*client));
    client->io = *io;
    client->frame_buffer = frame_buffer;
    client->frame_capacity = frame_capacity;
    client->max_frame_bytes = max_frame_bytes;
    client->next_tx_sequence = 1u;
    client->initialized = 1;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

void cdin_frontend_client_set_logger(cdin_frontend_client *client,
                                     cdin_frontend_logger *logger)
{
    if (client != NULL && client->initialized) client->logger = logger;
}

cdin_frontend_protocol_status cdin_frontend_client_send_namespaced(
    cdin_frontend_client *client,
    const char *name_space,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload,
    size_t payload_length)
{
    cdin_frontend_protocol_message_view message;
    cdin_frontend_protocol_status status;
    size_t frame_length = 0u;
    if (client == NULL || !client->initialized || name_space == NULL ||
        payload == NULL || payload_length == 0u) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (!cdin_frontend_protocol_message_is_inbound(type)) {
        log_failure(client, "outbound message direction", CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (client->next_tx_sequence == UINT64_MAX) {
        log_failure(client, "outbound sequence", CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED);
        return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    }
    status = cdin_frontend_protocol_validate_payload(type, payload, payload_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        log_failure(client, "outbound payload validation", status);
        return status;
    }
    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.sequence = client->next_tx_sequence;
    message.type = type;
    message.name_space = name_space;
    message.payload = payload;
    message.payload_length = payload_length;

    status = cdin_frontend_protocol_encode_frame(&message, client->frame_buffer,
                                                  client->frame_capacity, &frame_length,
                                                  client->max_frame_bytes);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        log_failure(client, "encode outbound frame", status);
        return status;
    }
    status = cdin_frontend_protocol_write_frame(&client->io,
                                                 client->frame_buffer, frame_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        log_failure(client, "write outbound frame", status);
        return status;
    }
    if (client->logger != NULL) {
        CDIN_FRONTEND_TRACE(client->logger, "PROTOCOL", "sent %s seq=%llu",
                            cdin_frontend_protocol_message_name(type),
                            (unsigned long long)message.sequence);
    }
    client->next_tx_sequence++;
    return CDIN_FRONTEND_PROTOCOL_OK;
}

cdin_frontend_protocol_status cdin_frontend_client_send(
    cdin_frontend_client *client,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload,
    size_t payload_length)
{
    return cdin_frontend_client_send_namespaced(
        client, "core", type, payload, payload_length);
}

cdin_frontend_protocol_status cdin_frontend_client_receive(
    cdin_frontend_client *client,
    cdin_frontend_protocol_message_view *out_message)
{
    cdin_frontend_protocol_message_view message;
    cdin_frontend_protocol_status status;
    size_t frame_length = 0u;
    if (client == NULL || !client->initialized || out_message == NULL) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    status = cdin_frontend_protocol_read_frame(&client->io, client->frame_buffer,
                                                client->frame_capacity, &frame_length,
                                                client->max_frame_bytes);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        log_failure(client, "read inbound frame", status);
        return status;
    }
    status = cdin_frontend_protocol_decode_frame(client->frame_buffer, frame_length,
                                                  &message, client->max_frame_bytes);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        log_failure(client, "decode inbound frame", status);
        return status;
    }
    if (message.major != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR ||
        message.minor != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR) {
        log_failure(client, "protocol version", CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH);
        return CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH;
    }
    if (cdin_frontend_protocol_message_is_inbound(message.type)) {
        log_failure(client, "inbound message direction", CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);
        return CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA;
    }
    if (client->last_rx_sequence == UINT64_MAX ||
        message.sequence != client->last_rx_sequence + 1u) {
        log_failure(client, "inbound sequence", CDIN_FRONTEND_PROTOCOL_INVALID_FRAME);
        return CDIN_FRONTEND_PROTOCOL_INVALID_FRAME;
    }
    client->last_rx_sequence = message.sequence;
    *out_message = message;
    if (client->logger != NULL) {
        CDIN_FRONTEND_TRACE(client->logger, "PROTOCOL", "received %s seq=%llu",
                            cdin_frontend_protocol_message_name(message.type),
                            (unsigned long long)message.sequence);
    }
    return CDIN_FRONTEND_PROTOCOL_OK;
}
