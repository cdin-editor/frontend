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
        0x84u, 0x65u, 'm','o','u','s','e',
        0x74u, 'd','r','i','v','e','r','-','r','o','l','e','-','e','v','e','n','t','s','/','1',
        0x76u, 'p','a','n','e','-','s','p','l','i','t','t','e','r','-','r','e','s','i','z','e','/','1',
        0x71u, 'i','m','e','-','c','o','m','p','o','s','i','t','i','o','n','/','1'
    };
    static const uint8_t snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u,
        0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    static const uint8_t promoted_role[] = {
        0xa2u,
        0x65u, 'e','p','o','c','h', 0x03u,
        0x66u, 'd','r','i','v','e','r', 0xf5u
    };
    static const uint8_t revoked_role[] = {
        0xa2u,
        0x65u, 'e','p','o','c','h', 0x04u,
        0x66u, 'd','r','i','v','e','r', 0xf4u
    };
    static const uint8_t mismatched_epoch_role[] = {
        0xa2u,
        0x65u, 'e','p','o','c','h', 0x04u,
        0x66u, 'd','r','i','v','e','r', 0xf5u
    };
    static const uint8_t detach_ack[] = { 0xa0u };
    static const uint8_t reattach_promoted_role[] = {
        0xa2u,
        0x65u, 'e','p','o','c','h', 0x07u,
        0x66u, 'd','r','i','v','e','r', 0xf5u
    };
    static const uint8_t reattach_snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x02u,
        0x68u, 's','e','q','u','e','n','c','e', 0x02u
    };
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    cdin_frontend_session session;
    cdin_frontend_session incomplete_session;
    cdin_frontend_protocol_message_view message;
    cdin_frontend_input_event input_event;
    uint8_t frame_buffer[512];
    uint8_t paste[] = { 'a', 0u, 0xffu };
    size_t output_mark;
    const uint8_t *mouse_position = NULL;
    const uint8_t *mouse_modifiers = NULL;
    size_t mouse_position_length = 0u;
    size_t mouse_modifiers_length = 0u;
    size_t mouse_modifier_count = 0u;
    uint64_t mouse_x = 0u;
    uint64_t mouse_y = 0u;
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
    (void)append_message(&stream, 5u, CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED,
                         mismatched_epoch_role, sizeof(mismatched_epoch_role));

    assert(cdin_frontend_client_init(&client, &io, frame_buffer, sizeof(frame_buffer), 256u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    cdin_frontend_session_init(&incomplete_session, &client, NULL, "incomplete");
    incomplete_session.state = CDIN_FRONTEND_SESSION_ATTACHED;
    incomplete_session.driver = 1;
    assert(cdin_frontend_session_resize(&incomplete_session, 800u, 600u) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    cdin_frontend_session_init(&session, &client, NULL, "tui");
    assert(cdin_frontend_session_set_capabilities(
        &session, CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE) |
                  CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS) |
                  CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE) |
                  CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_IME_COMPOSITION)) ==
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
            CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_DRIVER_ROLE_EVENTS) |
            CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE) |
            CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_IME_COMPOSITION)));
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
    {
        int was_driver = session.driver;
        session.driver = 0;
        assert(cdin_frontend_session_send_pane_splitter_resize(
            &session, 99u, "split-root", 1u, 5000u) ==
            CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
        session.driver = was_driver;
    }
    {
        cdin_frontend_capability_mask accepted = session.accepted_capabilities;
        session.accepted_capabilities &= ~CDIN_FRONTEND_CAP_BIT(
            CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE);
        assert(cdin_frontend_session_send_pane_splitter_resize(
            &session, 1u, "split-root", 1u, 5000u) ==
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
        session.accepted_capabilities = accepted;
    }
    output_mark = stream.output_length;
    assert(cdin_frontend_session_send_pane_splitter_resize(
        &session, 1u, "split-root", 1u, 5000u) == CDIN_FRONTEND_PROTOCOL_OK);
    {
        cdin_frontend_protocol_message_view pane_message;
        size_t frame_length = stream.output_length - output_mark;
        uint64_t ratio_bp = 0u;
        assert(cdin_frontend_protocol_decode_frame(
            stream.output + output_mark, frame_length, &pane_message, 1024u) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(pane_message.type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE);
        assert(strcmp(pane_message.name_space, "pane") == 0);
        assert(cdin_frontend_protocol_payload_get_uint(
            pane_message.payload, pane_message.payload_length, "ratio_bp",
            &ratio_bp) == CDIN_FRONTEND_PROTOCOL_OK);
        assert(ratio_bp == 5000u);
    }

    assert(cdin_frontend_session_ack(&session, 1u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_request_lines(&session, "view-1", 0u, 40u, 0u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_send_key(&session, "c", CDIN_FRONTEND_MOD_CTRL, "press") ==
           CDIN_FRONTEND_PROTOCOL_OK);
    output_mark = stream.output_length;
    assert(cdin_frontend_session_send_mouse(
        &session, "press", "left", CDIN_FRONTEND_MOD_CTRL | CDIN_FRONTEND_MOD_SHIFT,
        NULL, 20u, 30u, 1) == CDIN_FRONTEND_PROTOCOL_OK);
    {
        cdin_frontend_protocol_message_view mouse_message;
        size_t frame_length = stream.output_length - output_mark;
        assert(cdin_frontend_protocol_decode_frame(
            stream.output + output_mark, frame_length, &mouse_message, 1024u) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(mouse_message.type == CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE);
        assert(cdin_frontend_protocol_payload_get_text(
            mouse_message.payload, mouse_message.payload_length,
            "action", (char *)frame_buffer, sizeof(frame_buffer), &mouse_position_length) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(strcmp((char *)frame_buffer, "press") == 0);
        assert(cdin_frontend_protocol_payload_get_raw(
            mouse_message.payload, mouse_message.payload_length,
            "position", &mouse_position, &mouse_position_length) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_protocol_payload_get_uint(
            mouse_position, mouse_position_length, "x", &mouse_x) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_protocol_payload_get_uint(
            mouse_position, mouse_position_length, "y", &mouse_y) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(mouse_x == 20u && mouse_y == 30u);
        assert(cdin_frontend_protocol_payload_get_raw(
            mouse_message.payload, mouse_message.payload_length,
            "modifiers", &mouse_modifiers, &mouse_modifiers_length) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_protocol_array_count(
            mouse_modifiers, mouse_modifiers_length, &mouse_modifier_count) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(mouse_modifier_count == 2u);
    }
    assert(cdin_frontend_session_send_mouse(
        &session, "drag", "left", 0u, NULL, 1u, 1u, 1) ==
        CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    {
        cdin_frontend_capability_mask accepted = session.accepted_capabilities;
        session.accepted_capabilities &= ~CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE);
        assert(cdin_frontend_session_send_mouse(
            &session, "press", "left", 0u, NULL, 1u, 1u, 1) ==
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
        session.accepted_capabilities = accepted;
    }
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
    {
        static const uint8_t preedit[] = { 0xc3u, 0xa9u };
        static const uint8_t committed[] = { 0xd8u, 0xb3u, 0xd9u, 0x84u };
        uint64_t composition_id;
        uint64_t payload_id = 0u;
        uint64_t sequence_before;
        cdin_frontend_protocol_message_view sent;
        composition_id = cdin_frontend_session_next_ime_composition_id(&session);
        assert(composition_id != 0u);
        output_mark = stream.output_length;
        assert(cdin_frontend_session_ime_start(
            &session, composition_id, "view-1", 1u) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_protocol_decode_frame(
            stream.output + output_mark, stream.output_length - output_mark,
            &sent, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
        assert(sent.type == CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START);
        assert(sent.sequence == composition_id);
        assert(cdin_frontend_protocol_payload_get_uint(
            sent.payload, sent.payload_length, "id", &payload_id) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(payload_id == composition_id);
        sequence_before = cdin_frontend_session_next_ime_composition_id(&session);
        assert(sequence_before > composition_id);
        assert(cdin_frontend_session_ime_start(
            &session, composition_id, "view-1", 1u) ==
            CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
        assert(cdin_frontend_session_ime_update(
            &session, composition_id, preedit, sizeof(preedit),
            sizeof(preedit), 0u, sizeof(preedit)) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        output_mark = stream.output_length;
        assert(cdin_frontend_session_ime_commit(
            &session, composition_id, committed, sizeof(committed)) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_protocol_decode_frame(
            stream.output + output_mark, stream.output_length - output_mark,
            &sent, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
        assert(sent.type == CDIN_FRONTEND_PROTOCOL_INPUT_TEXT);
        assert(cdin_frontend_protocol_payload_get_uint(
            sent.payload, sent.payload_length, "composition_id", &payload_id) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(payload_id == composition_id);
        composition_id = cdin_frontend_session_next_ime_composition_id(&session);
        assert(composition_id > payload_id);
        assert(cdin_frontend_session_ime_start(
            &session, composition_id, "view-1", 1u) ==
            CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_session_ime_cancel(
            &session, composition_id, "ime_cancelled") ==
            CDIN_FRONTEND_PROTOCOL_OK);
        sequence_before = client.next_tx_sequence;
        client.next_tx_sequence = UINT64_MAX;
        assert(cdin_frontend_session_next_ime_composition_id(&session) == 0u);
        client.next_tx_sequence = sequence_before;
    }
    assert(cdin_frontend_session_resize(&session, 100u, 30u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_session_focus(&session, 1) == CDIN_FRONTEND_PROTOCOL_OK);

    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.driver == 0);
    assert(!cdin_frontend_session_is_driver(&session));
    assert(cdin_frontend_session_send_key(&session, "x", 0u, "press") ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_send_paste(&session, paste, sizeof(paste)) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_receive(&session, &message) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_FRAME);
    assert(session.driver == 0);
    assert(cdin_frontend_session_detach(&session) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.state == CDIN_FRONTEND_SESSION_DETACH_SENT);
    (void)append_message(&stream, 6u, CDIN_FRONTEND_PROTOCOL_DETACH_ACK, detach_ack, sizeof(detach_ack));
    (void)append_message(&stream, 7u, CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED,
                         reattach_promoted_role, sizeof(reattach_promoted_role));
    (void)append_message(&stream, 8u, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         reattach_snapshot_flush, sizeof(reattach_snapshot_flush));
    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_DETACH_ACK);
    assert(session.state == CDIN_FRONTEND_SESSION_DETACHED);
    assert(!cdin_frontend_session_is_driver(&session));
    assert(cdin_frontend_session_attach(&session, "view-1", "cells", 100u, 30u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(session.state == CDIN_FRONTEND_SESSION_ATTACH_SENT);
    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED);
    assert(session.state == CDIN_FRONTEND_SESSION_ATTACH_SENT);
    assert(cdin_frontend_session_is_driver(&session));
    assert(cdin_frontend_session_receive(&session, &message) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH);
    assert(session.state == CDIN_FRONTEND_SESSION_ATTACHED);
    assert(session.last_flush_sequence == 2u);
    assert(cdin_frontend_session_send_key(&session, "x", 0u, "press") ==
           CDIN_FRONTEND_PROTOCOL_OK);
    return 0;
}
