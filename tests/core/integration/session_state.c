#include "cdin/frontend/core/session/session.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct fake_stream {
    uint8_t input[1024];
    size_t input_length;
    size_t input_offset;
    uint8_t output[2048];
    size_t output_length;
    size_t max_read;
    size_t max_write;
} fake_stream;

static ptrdiff_t fake_read(void *opaque, uint8_t *buffer, size_t length)
{
    fake_stream *stream = (fake_stream *)opaque;
    size_t amount = length;
    size_t available = stream->input_length - stream->input_offset;
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

static size_t append_message(fake_stream *stream, uint64_t sequence,
                             cdin_frontend_protocol_message_type type,
                             const uint8_t *payload, size_t payload_length)
{
    cdin_frontend_protocol_message_view message;
    size_t length = 0u;
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
        &length, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    stream->input_length += length;
    return length;
}

int main(void)
{
    static const uint8_t hello_ack[] = {
        0xa7u,
        0x65u, 'm','a','j','o','r', 0x01u,
        0x65u, 'm','i','n','o','r', 0x00u,
        0x66u, 'd','r','i','v','e','r', 0xf4u,
        0x69u, 'c','l','i','e','n','t','_','i','d', 0x02u,
        0x6au, 's','e','s','s','i','o','n','_','i','d', 0x07u,
        0x71u, 'd','r','i','v','e','r','_','r','o','l','e','_','e','p','o','c','h', 0x01u,
        0x75u, 'a','c','c','e','p','t','e','d','_','c','a','p','a','b','i','l','i','t','i','e','s',
        0x82u, 0x65u, 'm','o','u','s','e',
        0x74u, 'd','r','i','v','e','r','-','r','o','l','e','-','e','v','e','n','t','s','/','1'
    };
    static const uint8_t snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u,
        0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    static const uint8_t promoted_role[] = {
        0xa2u,
        0x65u, 'e','p','o','c','h', 0x04u,
        0x66u, 'd','r','i','v','e','r', 0xf5u
    };
    static const uint8_t revoked_role[] = {
        0xa2u,
        0x65u, 'e','p','o','c','h', 0x03u,
        0x66u, 'd','r','i','v','e','r', 0xf4u
    };
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    cdin_frontend_session session;
    cdin_frontend_protocol_message_view message;
    cdin_frontend_input_event input_event;
    uint8_t frame_buffer[512];
    uint8_t paste[] = { 'a', 0u, 0xffu };
    static const uint8_t persian_text[] = { 0xd8u, 0xb3u, 0xd9u, 0x84u, 0xd8u, 0xa7u, 0xd9u, 0x85u };

    memset(&stream, 0, sizeof(stream));
    stream.max_read = 5u;
    stream.max_write = 3u;
    io.context = &stream;
    io.read = fake_read;
    io.write = fake_write;
    (void)append_message(&stream, 1u, CDIN_FRONTEND_PROTOCOL_HELLO_ACK, hello_ack, sizeof(hello_ack));
    (void)append_message(&stream, 2u, CDIN_FRONTEND_PROTOCOL_UI_FLUSH, snapshot_flush, sizeof(snapshot_flush));
    (void)append_message(&stream, 3u, CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED,
                         promoted_role, sizeof(promoted_role));
    (void)append_message(&stream, 4u, CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED,
                         revoked_role, sizeof(revoked_role));

    assert(cdin_frontend_client_init(&client, &io, frame_buffer, sizeof(frame_buffer), 256u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    cdin_frontend_session_init(&session, &client, NULL, "tui");
    assert(cdin_frontend_session_set_capabilities(
        &session, CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE) |
                  CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS)) ==
        CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_hello(&session) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_set_capabilities(&session, 0u) == CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(session.state == CDIN_FRONTEND_SESSION_HELLO_SENT);
    assert(cdin_frontend_session_attach(&session, "view-1", "cells", 80u, 24u) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);

    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.state == CDIN_FRONTEND_SESSION_NEGOTIATED);
    assert(session.session_id == 7u && session.client_id == 2u && session.driver == 0);
    assert(!cdin_frontend_session_is_driver(&session));
    assert(cdin_frontend_session_accepted_capabilities(&session) ==
           (CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE) |
            CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS)));
    assert(cdin_frontend_session_attach(&session, "view-1", "cells", 80u, 24u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.state == CDIN_FRONTEND_SESSION_ATTACH_SENT);
    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.state == CDIN_FRONTEND_SESSION_ATTACHED);
    assert(session.last_flush_sequence == 1u);
    assert(!cdin_frontend_session_is_driver(&session));
    assert(cdin_frontend_session_send_key(&session, "x", 0u, "press") ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.driver == 1);
    assert(cdin_frontend_session_is_driver(&session));

    assert(cdin_frontend_session_ack(&session, 1u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_request_lines(&session, "view-1", 0u, 40u, 0u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_send_key(&session, "c", CDIN_FRONTEND_MOD_CTRL, "press") ==
           CDIN_FRONTEND_PROTOCOL_OK);
    memset(&input_event, 0, sizeof(input_event));
    input_event.kind = CDIN_FRONTEND_INPUT_KEY;
    (void)snprintf(input_event.key, sizeof(input_event.key), "%s", "up");
    assert(cdin_frontend_session_send_input_event(&session, &input_event) == CDIN_FRONTEND_PROTOCOL_OK);
    input_event.kind = CDIN_FRONTEND_INPUT_TEXT;
    memcpy(input_event.text, persian_text, sizeof(persian_text));
    input_event.text_length = sizeof(persian_text);
    assert(cdin_frontend_session_send_input_event(&session, &input_event) == CDIN_FRONTEND_PROTOCOL_OK);
    input_event.kind = CDIN_FRONTEND_INPUT_UNKNOWN;
    assert(cdin_frontend_session_send_input_event(&session, &input_event) == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
    assert(cdin_frontend_session_send_text(&session, persian_text, sizeof(persian_text)) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_send_text(&session, paste, sizeof(paste)) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_send_paste(&session, paste, sizeof(paste)) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_resize(&session, 100u, 30u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_focus(&session, 1) == CDIN_FRONTEND_PROTOCOL_OK);

    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.driver == 0);
    assert(!cdin_frontend_session_is_driver(&session));
    assert(cdin_frontend_session_send_key(&session, "x", 0u, "press") ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_send_paste(&session, paste, sizeof(paste)) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_detach(&session) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.state == CDIN_FRONTEND_SESSION_DETACH_SENT);
    return 0;
}
