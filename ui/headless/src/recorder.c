#include "cdin/frontend/ui/headless/recorder.h"

#include <string.h>

static const uint8_t trace_magic[4] = {'C', 'D', 'H', 'R'};

static void put_u16(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value >> 8u);
    out[1] = (uint8_t)value;
}

static void put_u32(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value >> 24u);
    out[1] = (uint8_t)(value >> 16u);
    out[2] = (uint8_t)(value >> 8u);
    out[3] = (uint8_t)value;
}

static void put_u64(uint8_t *out, uint64_t value)
{
    size_t i;
    for (i = 0u; i < 8u; ++i) {
        out[i] = (uint8_t)(value >> (56u - (unsigned)(i * 8u)));
    }
}

static uint16_t get_u16(const uint8_t *in)
{
    return (uint16_t)(((uint16_t)in[0] << 8u) | (uint16_t)in[1]);
}

static uint32_t get_u32(const uint8_t *in)
{
    return ((uint32_t)in[0] << 24u) | ((uint32_t)in[1] << 16u) |
           ((uint32_t)in[2] << 8u) | (uint32_t)in[3];
}

static uint64_t get_u64(const uint8_t *in)
{
    uint64_t value = 0u;
    size_t i;
    for (i = 0u; i < 8u; ++i) value = (value << 8u) | (uint64_t)in[i];
    return value;
}

static int valid_direction(cdin_frontend_headless_direction direction)
{
    return direction == CDIN_FRONTEND_HEADLESS_BACKEND_TO_FRONTEND ||
           direction == CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND;
}

cdin_frontend_headless_status cdin_frontend_headless_recorder_init(
    cdin_frontend_headless_recorder *recorder,
    uint8_t *storage,
    size_t capacity)
{
    if (recorder == NULL || storage == NULL) return CDIN_FRONTEND_HEADLESS_INVALID_ARGUMENT;
    if (capacity < CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES) {
        return CDIN_FRONTEND_HEADLESS_BUFFER_TOO_SMALL;
    }
    memset(recorder, 0, sizeof(*recorder));
    recorder->storage = storage;
    recorder->capacity = capacity;
    memcpy(storage, trace_magic, sizeof(trace_magic));
    put_u16(storage + 4u, (uint16_t)CDIN_FRONTEND_HEADLESS_TRACE_VERSION);
    put_u16(storage + 6u, 0u);
    recorder->length = CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES;
    recorder->initialized = 1;
    return CDIN_FRONTEND_HEADLESS_OK;
}

cdin_frontend_headless_status cdin_frontend_headless_record(
    cdin_frontend_headless_recorder *recorder,
    cdin_frontend_headless_direction direction,
    const cdin_frontend_protocol_message_view *message)
{
    size_t required;
    uint8_t *header;
    cdin_frontend_protocol_status protocol_status;
    if (recorder == NULL || !recorder->initialized || message == NULL ||
        message->payload == NULL || message->payload_length == 0u ||
        !valid_direction(direction) || message->name_space == NULL ||
        strcmp(message->name_space, "core") != 0) {
        return CDIN_FRONTEND_HEADLESS_INVALID_ARGUMENT;
    }
    if (message->major != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR ||
        message->minor != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR || message->sequence == 0u ||
        (direction == CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND &&
         !cdin_frontend_protocol_message_is_inbound(message->type)) ||
        (direction == CDIN_FRONTEND_HEADLESS_BACKEND_TO_FRONTEND &&
         cdin_frontend_protocol_message_is_inbound(message->type))) {
        return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
    }
    if (message->payload_length > UINT32_MAX ||
        message->payload_length > SIZE_MAX - CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES) {
        return CDIN_FRONTEND_HEADLESS_BUFFER_TOO_SMALL;
    }
    protocol_status = cdin_frontend_protocol_validate_payload(
        message->type, message->payload, message->payload_length);
    if (protocol_status != CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
    required = CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES + message->payload_length;
    if (recorder->length > recorder->capacity || required > recorder->capacity - recorder->length) {
        return CDIN_FRONTEND_HEADLESS_BUFFER_TOO_SMALL;
    }
    header = recorder->storage + recorder->length;
    header[0] = (uint8_t)direction;
    header[1] = 0u;
    put_u16(header + 2u, (uint16_t)message->major);
    put_u16(header + 4u, (uint16_t)message->minor);
    put_u16(header + 6u, (uint16_t)message->type);
    put_u64(header + 8u, message->sequence);
    put_u32(header + 16u, (uint32_t)message->payload_length);
    memcpy(header + CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES,
           message->payload, message->payload_length);
    recorder->length += required;
    return CDIN_FRONTEND_HEADLESS_OK;
}

cdin_frontend_headless_status cdin_frontend_headless_replay(
    const uint8_t *trace,
    size_t trace_length,
    cdin_frontend_headless_direction direction_filter,
    cdin_frontend_headless_replay_fn callback,
    void *context,
    size_t *out_replayed)
{
    size_t offset;
    size_t replayed = 0u;
    if (trace == NULL || callback == NULL || out_replayed == NULL ||
        (direction_filter != 0 && !valid_direction(direction_filter))) {
        return CDIN_FRONTEND_HEADLESS_INVALID_ARGUMENT;
    }
    *out_replayed = 0u;
    if (trace_length < CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES ||
        memcmp(trace, trace_magic, sizeof(trace_magic)) != 0 || get_u16(trace + 6u) != 0u) {
        return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
    }
    if (get_u16(trace + 4u) != CDIN_FRONTEND_HEADLESS_TRACE_VERSION) {
        return CDIN_FRONTEND_HEADLESS_UNSUPPORTED_VERSION;
    }
    offset = CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES;
    while (offset < trace_length) {
        cdin_frontend_protocol_message_view message;
        cdin_frontend_headless_direction direction;
        uint32_t payload_length;
        size_t record_length;
        cdin_frontend_protocol_status protocol_status;
        if (trace_length - offset < CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES) {
            return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
        }
        direction = (cdin_frontend_headless_direction)trace[offset];
        payload_length = get_u32(trace + offset + 16u);
        if (!valid_direction(direction) || trace[offset + 1u] != 0u ||
            payload_length == 0u) {
            return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
        }
#if SIZE_MAX <= UINT32_MAX
        if (payload_length > SIZE_MAX - CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES) {
            return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
        }
#endif
        record_length = CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES + (size_t)payload_length;
        if (record_length > trace_length - offset) return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
        memset(&message, 0, sizeof(message));
        message.major = get_u16(trace + offset + 2u);
        message.minor = get_u16(trace + offset + 4u);
        message.type = (cdin_frontend_protocol_message_type)get_u16(trace + offset + 6u);
        message.sequence = get_u64(trace + offset + 8u);
        message.name_space = "core";
        message.payload = trace + offset + CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES;
        message.payload_length = (size_t)payload_length;
        if (message.major != CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR ||
            message.minor != CDIN_FRONTEND_PROTOCOL_VERSION_MINOR || message.sequence == 0u ||
            (direction == CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND &&
             !cdin_frontend_protocol_message_is_inbound(message.type)) ||
            (direction == CDIN_FRONTEND_HEADLESS_BACKEND_TO_FRONTEND &&
             cdin_frontend_protocol_message_is_inbound(message.type))) {
            return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
        }
        protocol_status = cdin_frontend_protocol_validate_payload(
            message.type, message.payload, message.payload_length);
        if (protocol_status != CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_HEADLESS_INVALID_TRACE;
        if (direction_filter == 0 || direction_filter == direction) {
            if (callback(context, direction, &message) != 0) {
                *out_replayed = replayed;
                return CDIN_FRONTEND_HEADLESS_CALLBACK_ERROR;
            }
            replayed++;
        }
        offset += record_length;
    }
    *out_replayed = replayed;
    return CDIN_FRONTEND_HEADLESS_OK;
}

size_t cdin_frontend_headless_recorder_size(
    const cdin_frontend_headless_recorder *recorder)
{
    return recorder != NULL && recorder->initialized ? recorder->length : 0u;
}
