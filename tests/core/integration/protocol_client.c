#include "cdin/frontend/core/protocol/client.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct fake_stream {
    uint8_t input[256];
    size_t input_length;
    size_t input_offset;
    uint8_t output[256];
    size_t output_length;
    size_t max_read;
    size_t max_write;
} fake_stream;

static ptrdiff_t fake_read(void *context, uint8_t *buffer, size_t length)
{
    fake_stream *stream = (fake_stream *)context;
    size_t remaining = stream->input_length - stream->input_offset;
    size_t amount = length;
    if (amount > stream->max_read) amount = stream->max_read;
    if (amount > remaining) amount = remaining;
    if (amount == 0u) return 0;
    memcpy(buffer, stream->input + stream->input_offset, amount);
    stream->input_offset += amount;
    return (ptrdiff_t)amount;
}

static ptrdiff_t fake_write(void *context, const uint8_t *buffer, size_t length)
{
    fake_stream *stream = (fake_stream *)context;
    size_t amount = length;
    if (amount > stream->max_write) amount = stream->max_write;
    if (amount > sizeof(stream->output) - stream->output_length) return -1;
    memcpy(stream->output + stream->output_length, buffer, amount);
    stream->output_length += amount;
    return (ptrdiff_t)amount;
}

int main(void)
{
    static const uint8_t ack_payload[] = { 0xa1u, 0x68u, 's','e','q','u','e','n','c','e', 0x01u };
    static const uint8_t notify_payload[] = {
        0xa2u,
        0x64u, 't','e','x','t', 0x61u, 'x',
        0x65u, 'l','e','v','e','l', 0x64u, 'i','n','f','o'
    };
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    uint8_t frame_buffer[512];
    cdin_frontend_protocol_message_view wire;
    cdin_frontend_protocol_message_view received;
    size_t encoded_length = 0u;
    uint64_t value = 0u;

    memset(&stream, 0, sizeof(stream));
    stream.max_read = 3u;
    stream.max_write = 2u;
    io.context = &stream;
    io.read = fake_read;
    io.write = fake_write;

    memset(&wire, 0, sizeof(wire));
    wire.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    wire.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    wire.sequence = 1u;
    wire.type = CDIN_FRONTEND_PROTOCOL_UI_NOTIFY;
    wire.name_space = "core";
    wire.payload = notify_payload;
    wire.payload_length = sizeof(notify_payload);
    assert(cdin_frontend_protocol_encode_frame(&wire, stream.input, sizeof(stream.input),
                                               &encoded_length, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    stream.input_length = encoded_length;
    assert(cdin_frontend_client_init(&client, &io, frame_buffer, sizeof(frame_buffer), 256u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_client_receive(&client, &received) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(received.type == CDIN_FRONTEND_PROTOCOL_UI_NOTIFY);
    assert(cdin_frontend_protocol_payload_get_text(received.payload, received.payload_length,
                                                   "text", (char *)frame_buffer, sizeof(frame_buffer),
                                                   &encoded_length) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(strcmp((char *)frame_buffer, "x") == 0);

    assert(cdin_frontend_client_send(&client, CDIN_FRONTEND_PROTOCOL_ACK,
                                     ack_payload, sizeof(ack_payload)) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(stream.output_length > CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES);
    assert(cdin_frontend_protocol_decode_frame(stream.output, stream.output_length,
                                               &wire, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(wire.type == CDIN_FRONTEND_PROTOCOL_ACK);
    assert(wire.sequence == 1u);
    assert(cdin_frontend_protocol_payload_get_uint(wire.payload, wire.payload_length,
                                                   "sequence", &value) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(value == 1u);
    assert(cdin_frontend_client_send(&client, CDIN_FRONTEND_PROTOCOL_UI_NOTIFY,
                                     notify_payload, sizeof(notify_payload)) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);

    /* Sequence gaps and wrong message direction are rejected before dispatch. */
    wire.sequence = 3u;
    wire.type = CDIN_FRONTEND_PROTOCOL_UI_NOTIFY;
    wire.payload = notify_payload;
    wire.payload_length = sizeof(notify_payload);
    assert(cdin_frontend_protocol_encode_frame(&wire, stream.input, sizeof(stream.input),
                                               &encoded_length, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    stream.input_length = encoded_length;
    stream.input_offset = 0u;
    assert(cdin_frontend_client_receive(&client, &received) == CDIN_FRONTEND_PROTOCOL_INVALID_FRAME);
    return 0;
}
