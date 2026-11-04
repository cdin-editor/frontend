#include "cdin/frontend/ui/tui/session.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INPUT_CAPACITY 2048u
#define OUTPUT_CAPACITY 4096u
#define FRAME_CAPACITY 260u

typedef struct fake_stream {
    uint8_t input[INPUT_CAPACITY];
    size_t input_length;
    size_t input_offset;
    uint8_t output[OUTPUT_CAPACITY];
    size_t output_length;
    size_t max_read;
    size_t max_write;
} fake_stream;

static ptrdiff_t fake_read(void *opaque, uint8_t *buffer, size_t length)
{
    fake_stream *stream = (fake_stream *)opaque;
    size_t available = stream->input_length - stream->input_offset;
    size_t amount = length;
    if (amount > stream->max_read) amount = stream->max_read;
    if (amount > available) amount = available;
    if (amount == 0u) return 0;
    memcpy(buffer, stream->input + stream->input_offset, amount);
    stream->input_offset += amount;
    return (ptrdiff_t)amount;
}

static ptrdiff_t fake_write(void *opaque, const uint8_t *bytes, size_t length)
{
    fake_stream *stream = (fake_stream *)opaque;
    size_t amount = length;
    if (amount > stream->max_write) amount = stream->max_write;
    if (amount > sizeof(stream->output) - stream->output_length) return -1;
    memcpy(stream->output + stream->output_length, bytes, amount);
    stream->output_length += amount;
    return (ptrdiff_t)amount;
}

static void append_input(fake_stream *stream, uint64_t sequence,
                         cdin_frontend_protocol_message_type type,
                         const uint8_t *payload, size_t payload_length)
{
    cdin_frontend_protocol_message_view message;
    size_t frame_length = 0u;
    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.sequence = sequence;
    message.type = type;
    message.name_space = "core";
    message.payload = payload;
    message.payload_length = payload_length;
    assert(cdin_frontend_protocol_encode_frame(&message,
        stream->input + stream->input_length,
        sizeof(stream->input) - stream->input_length,
        &frame_length, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    stream->input_length += frame_length;
}

static cdin_frontend_protocol_message_type decode_output(fake_stream *stream,
                                                          size_t *offset,
                                                          uint8_t *frame_buffer,
                                                          size_t capacity,
                                                          uint64_t expected_sequence)
{
    cdin_frontend_protocol_message_view message;
    uint32_t body_length;
    size_t frame_length;
    assert(*offset <= stream->output_length);
    assert(stream->output_length - *offset >= CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES);
    body_length = ((uint32_t)stream->output[*offset] << 24u) |
                  ((uint32_t)stream->output[*offset + 1u] << 16u) |
                  ((uint32_t)stream->output[*offset + 2u] << 8u) |
                  (uint32_t)stream->output[*offset + 3u];
    frame_length = (size_t)body_length + CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES;
    assert(frame_length <= capacity);
    assert(frame_length <= stream->output_length - *offset);
    assert(cdin_frontend_protocol_decode_frame(stream->output + *offset,
        frame_length, &message, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.sequence == expected_sequence);
    memcpy(frame_buffer, stream->output + *offset, frame_length);
    *offset += frame_length;
    return message.type;
}

int main(void)
{
    /* Every map uses RFC 8949 deterministic key ordering. */
    static const uint8_t hello_ack[] = {
        0xa6u,
        0x65u, 'm','a','j','o','r', 0x01u,
        0x65u, 'm','i','n','o','r', 0x00u,
        0x66u, 'd','r','i','v','e','r', 0xf5u,
        0x69u, 'c','l','i','e','n','t','_','i','d', 0x01u,
        0x6au, 's','e','s','s','i','o','n','_','i','d', 0x07u,
        0x75u, 'a','c','c','e','p','t','e','d','_','c','a','p','a','b','i','l','i','t','i','e','s', 0x80u
    };
    static const uint8_t options[] = {
        0xa3u,
        0x65u, 't','h','e','m','e', 0x67u, 'd','e','f','a','u','l','t',
        0x67u, 'o','p','t','i','o','n','s', 0xa0u,
        0x68u, 'r','e','v','i','s','i','o','n', 0x05u
    };
    static const uint8_t invalidate[] = {
        0xa3u,
        0x66u, 'r','a','n','g','e','s', 0x81u, 0xa2u,
        0x63u, 'e','n','d', 0x18u, 0x64u,
        0x65u, 's','t','a','r','t', 0x00u,
        0x67u, 'v','e','r','s','i','o','n', 0x05u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1'
    };
    static const uint8_t snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x05u,
        0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    cdin_frontend_session session;
    cdin_frontend_tui_model *model;
    cdin_frontend_protocol_message_view message;
    uint8_t frame_buffer[FRAME_CAPACITY];
    size_t output_offset = 0u;
    int committed = 0;

    memset(&stream, 0, sizeof(stream));
    stream.max_read = 7u;
    stream.max_write = 3u;
    io.context = &stream;
    io.read = fake_read;
    io.write = fake_write;
    append_input(&stream, 1u, CDIN_FRONTEND_PROTOCOL_HELLO_ACK,
                 hello_ack, sizeof(hello_ack));
    append_input(&stream, 2u, CDIN_FRONTEND_PROTOCOL_UI_OPTIONS,
                 options, sizeof(options));
    append_input(&stream, 3u, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                 invalidate, sizeof(invalidate));
    append_input(&stream, 4u, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                 snapshot_flush, sizeof(snapshot_flush));

    assert(cdin_frontend_client_init(&client, &io, frame_buffer,
        sizeof(frame_buffer), 256u) == CDIN_FRONTEND_PROTOCOL_OK);
    cdin_frontend_session_init(&session, &client, NULL, "tui");
    model = cdin_frontend_tui_model_create("view-1", 24u, NULL);
    assert(model != NULL);

    assert(cdin_frontend_session_hello(&session) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_HELLO_ACK);
    assert(cdin_frontend_session_attach(&session, "view-1", "cells", 80u, 24u) ==
           CDIN_FRONTEND_PROTOCOL_OK);

    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_UI_OPTIONS);
    assert(cdin_frontend_tui_receive(&session, model, &message, &committed) ==
           CDIN_FRONTEND_TUI_MODEL_OK && committed == 0);

    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE);
    assert(cdin_frontend_tui_receive(&session, model, &message, &committed) ==
           CDIN_FRONTEND_TUI_MODEL_OK && committed == 0);

    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH);
    assert(session.state == CDIN_FRONTEND_SESSION_ATTACHED);
    assert(cdin_frontend_tui_receive(&session, model, &message, &committed) ==
           CDIN_FRONTEND_TUI_MODEL_OK && committed == 1);
    assert(cdin_frontend_tui_model_revision(model) == 5u);
    assert(!cdin_frontend_tui_model_needs_lines(model));

    /* A single atomic snapshot results in ACK then a version-pinned pull. */
    assert(decode_output(&stream, &output_offset, frame_buffer,
        sizeof(frame_buffer), 1u) == CDIN_FRONTEND_PROTOCOL_HELLO);
    assert(decode_output(&stream, &output_offset, frame_buffer,
        sizeof(frame_buffer), 2u) == CDIN_FRONTEND_PROTOCOL_ATTACH);
    assert(decode_output(&stream, &output_offset, frame_buffer,
        sizeof(frame_buffer), 3u) == CDIN_FRONTEND_PROTOCOL_ACK);
    assert(decode_output(&stream, &output_offset, frame_buffer,
        sizeof(frame_buffer), 4u) == CDIN_FRONTEND_PROTOCOL_VIEW_REQUEST_LINES);
    assert(output_offset == stream.output_length);

    cdin_frontend_tui_model_destroy(model);
    return 0;
}
