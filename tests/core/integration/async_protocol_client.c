#include "cdin/frontend/core/protocol/async_client.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct async_stream {
    uint8_t input[256];
    size_t input_length, input_offset;
    uint8_t output[256];
    size_t output_length;
    unsigned read_calls, write_calls;
} async_stream;

static cdin_frontend_protocol_io_result try_read(
    void *context, uint8_t *buffer, size_t capacity, size_t *out_read)
{
    async_stream *stream = (async_stream *)context;
    size_t remaining = stream->input_length - stream->input_offset;
    size_t amount = capacity;
    *out_read = 0u;
    stream->read_calls++;
    if ((stream->read_calls % 2u) == 1u) return CDIN_FRONTEND_PROTOCOL_IO_AGAIN;
    if (amount > 2u) amount = 2u;
    if (amount > remaining) amount = remaining;
    if (amount == 0u) return CDIN_FRONTEND_PROTOCOL_IO_EOF;
    memcpy(buffer, stream->input + stream->input_offset, amount);
    stream->input_offset += amount;
    *out_read = amount;
    return CDIN_FRONTEND_PROTOCOL_IO_PROGRESS;
}

static cdin_frontend_protocol_io_result try_write(
    void *context, const uint8_t *buffer, size_t length, size_t *out_written)
{
    async_stream *stream = (async_stream *)context;
    size_t amount = length;
    *out_written = 0u;
    stream->write_calls++;
    if ((stream->write_calls % 2u) == 1u) return CDIN_FRONTEND_PROTOCOL_IO_AGAIN;
    if (amount > 2u) amount = 2u;
    if (amount > sizeof(stream->output) - stream->output_length) {
        return CDIN_FRONTEND_PROTOCOL_IO_FAILURE;
    }
    memcpy(stream->output + stream->output_length, buffer, amount);
    stream->output_length += amount;
    *out_written = amount;
    return CDIN_FRONTEND_PROTOCOL_IO_PROGRESS;
}

int main(void)
{
    static const uint8_t notify[] = {
        0xa2u, 0x64u, 't','e','x','t', 0x61u, 'x',
        0x65u, 'l','e','v','e','l', 0x64u, 'i','n','f','o'
    };
    static const uint8_t ack[] = {
        0xa1u, 0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    async_stream stream;
    cdin_frontend_protocol_async_io io;
    cdin_frontend_protocol_async_client client;
    cdin_frontend_protocol_message_view envelope, received;
    uint8_t rx[512], tx[512];
    size_t encoded = 0u;
    cdin_frontend_protocol_status status;
    unsigned attempts;

    memset(&stream, 0, sizeof(stream));
    memset(&envelope, 0, sizeof(envelope));
    envelope.major = 1u;
    envelope.minor = 0u;
    envelope.sequence = 1u;
    envelope.type = CDIN_FRONTEND_PROTOCOL_UI_NOTIFY;
    envelope.name_space = "core";
    envelope.payload = notify;
    envelope.payload_length = sizeof(notify);
    assert(cdin_frontend_protocol_encode_frame(&envelope, stream.input,
        sizeof(stream.input), &encoded, 256u) == CDIN_FRONTEND_PROTOCOL_OK);
    stream.input_length = encoded;
    io.context = &stream;
    io.read = try_read;
    io.write = try_write;
    assert(cdin_frontend_protocol_async_client_init(&client, &io, rx, sizeof(rx),
        tx, sizeof(tx), 256u) == CDIN_FRONTEND_PROTOCOL_OK);

    status = CDIN_FRONTEND_PROTOCOL_AGAIN;
    for (attempts = 0u; attempts < 300u && status == CDIN_FRONTEND_PROTOCOL_AGAIN; attempts++) {
        status = cdin_frontend_protocol_async_receive(&client, &received);
    }
    assert(status == CDIN_FRONTEND_PROTOCOL_OK);
    assert(received.type == CDIN_FRONTEND_PROTOCOL_UI_NOTIFY);
    assert(received.sequence == 1u);

    assert(cdin_frontend_protocol_async_queue(&client, CDIN_FRONTEND_PROTOCOL_ACK,
        ack, sizeof(ack)) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_async_queue(&client, CDIN_FRONTEND_PROTOCOL_ACK,
        ack, sizeof(ack)) == CDIN_FRONTEND_PROTOCOL_AGAIN);
    status = CDIN_FRONTEND_PROTOCOL_AGAIN;
    for (attempts = 0u; attempts < 300u && status == CDIN_FRONTEND_PROTOCOL_AGAIN; attempts++) {
        status = cdin_frontend_protocol_async_flush(&client);
    }
    assert(status == CDIN_FRONTEND_PROTOCOL_OK);
    assert(stream.output_length > 4u);
    assert(cdin_frontend_protocol_decode_frame(stream.output, stream.output_length,
        &envelope, 256u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(envelope.type == CDIN_FRONTEND_PROTOCOL_ACK);
    assert(envelope.sequence == 1u);

    /* EOF after a partial prefix is a truncated frame, never a clean EOF. */
    memset(&stream, 0, sizeof(stream));
    stream.input[0] = 0u;
    stream.input[1] = 0u;
    stream.input_length = 2u;
    assert(cdin_frontend_protocol_async_client_init(&client, &io, rx, sizeof(rx),
        tx, sizeof(tx), 256u) == CDIN_FRONTEND_PROTOCOL_OK);
    status = CDIN_FRONTEND_PROTOCOL_AGAIN;
    for (attempts = 0u; attempts < 8u && status == CDIN_FRONTEND_PROTOCOL_AGAIN; attempts++) {
        status = cdin_frontend_protocol_async_receive(&client, &received);
    }
    assert(status == CDIN_FRONTEND_PROTOCOL_INVALID_FRAME);

    /* Oversized length is rejected immediately after the four-byte prefix. */
    memset(&stream, 0, sizeof(stream));
    stream.input[0] = 0u;
    stream.input[1] = 0u;
    stream.input[2] = 1u;
    stream.input[3] = 1u; /* 257 > configured 256-byte body cap. */
    stream.input_length = 4u;
    assert(cdin_frontend_protocol_async_client_init(&client, &io, rx, sizeof(rx),
        tx, sizeof(tx), 256u) == CDIN_FRONTEND_PROTOCOL_OK);
    status = CDIN_FRONTEND_PROTOCOL_AGAIN;
    for (attempts = 0u; attempts < 8u && status == CDIN_FRONTEND_PROTOCOL_AGAIN; attempts++) {
        status = cdin_frontend_protocol_async_receive(&client, &received);
    }
    assert(status == CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED);
    return 0;
}
