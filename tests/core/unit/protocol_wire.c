#include "cdin/frontend/core/protocol/wire.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    static const uint8_t pane_resize_payload[] = {
        0xa4u,
        0x68u, 'r','a','t','i','o','_','b','p', 0x19u, 0x13u, 0x88u,
        0x68u, 's','p','l','i','t','_','i','d', 0x6au, 's','p','l','i','t','-','r','o','o','t',
        0x6au, 'r','e','q','u','e','s','t','_','i','d', 0x01u,
        0x6fu, 'l','a','y','o','u','t','_','r','e','v','i','s','i','o','n', 0x01u
    };
    static const uint8_t pane_resize_bad_ratio[] = {
        0xa4u,
        0x68u, 'r','a','t','i','o','_','b','p', 0x19u, 0x03u, 0xe7u,
        0x68u, 's','p','l','i','t','_','i','d', 0x6au, 's','p','l','i','t','-','r','o','o','t',
        0x6au, 'r','e','q','u','e','s','t','_','i','d', 0x01u,
        0x6fu, 'l','a','y','o','u','t','_','r','e','v','i','s','i','o','n', 0x01u
    };
    static const uint8_t pane_result_payload[] = {
        0xa3u,
        0x66u, 's','t','a','t','u','s', 0x67u, 'a','p','p','l','i','e','d',
        0x6au, 'r','e','q','u','e','s','t','_','i','d', 0x01u,
        0x6fu, 'l','a','y','o','u','t','_','r','e','v','i','s','i','o','n', 0x02u
    };
    static const uint8_t ack_payload[] = { 0xa1u, 0x68u, 's','e','q','u','e','n','c','e', 0x01u };
    static const uint8_t invalid_payload[] = { 0xa1u, 0x68u, 's','e','q','u','e','n','c','e', 0x18u, 0x01u };
    static const uint8_t ui_options_v1[] = {
        0xa3u,
        0x65u, 't','h','e','m','e', 0x64u, 'd','a','r','k',
        0x67u, 'o','p','t','i','o','n','s', 0xa0u,
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u
    };
    static const uint8_t ui_options_bad_tab_width[] = {
        0xa3u,
        0x65u, 't','h','e','m','e', 0x64u, 'd','a','r','k',
        0x67u, 'o','p','t','i','o','n','s', 0xa1u,
            0x69u, 't','a','b','_','w','i','d','t','h', 0x00u,
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u
    };
    static const uint8_t nullable_active_view_layout[] = {
        0xa2u, 0x66u, 'l','a','y','o','u','t', 0xa2u,
        0x64u, 'r','o','o','t', 0xa4u,
        0x62u, 'i','d', 0x66u, 'l','e','a','f','-','1',
        0x64u, 'k','i','n','d', 0x64u, 'l','e','a','f',
        0x65u, 'v','i','e','w','s', 0x80u,
        0x6bu, 'a','c','t','i','v','e','_','v','i','e','w', 0xf6u,
        0x6bu, 'a','c','t','i','v','e','_','l','e','a','f', 0x66u, 'l','e','a','f','-','1',
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u
    };
    uint8_t frame[128];
    size_t frame_length = 0u;
    cdin_frontend_protocol_message_view input;
    cdin_frontend_protocol_message_view output;
    uint64_t sequence = 0u;
    uint64_t ratio_bp = 0u;

    memset(&input, 0, sizeof(input));
    input.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    input.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    input.sequence = 1u;
    input.type = CDIN_FRONTEND_PROTOCOL_ACK;
    input.name_space = "core";
    input.payload = ack_payload;
    input.payload_length = sizeof(ack_payload);
    assert(cdin_frontend_protocol_validate_payload(CDIN_FRONTEND_PROTOCOL_UI_OPTIONS,
        ui_options_v1, sizeof(ui_options_v1)) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_validate_payload(CDIN_FRONTEND_PROTOCOL_UI_OPTIONS,
        ui_options_bad_tab_width, sizeof(ui_options_bad_tab_width)) ==
        CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);
    assert(cdin_frontend_protocol_validate_payload(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
        nullable_active_view_layout, sizeof(nullable_active_view_layout)) ==
        CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_validate_payload(input.type, input.payload, input.payload_length) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame), &frame_length, 1024u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(frame_length > CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES);
    assert(((size_t)frame[0] << 24u | (size_t)frame[1] << 16u |
            (size_t)frame[2] << 8u | (size_t)frame[3]) ==
           frame_length - CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES);
    assert(cdin_frontend_protocol_decode_frame(frame, frame_length, &output, 1024u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(output.type == CDIN_FRONTEND_PROTOCOL_ACK);
    assert(output.sequence == 1u);
    assert(cdin_frontend_protocol_payload_get_uint(output.payload, output.payload_length,
                                                   "sequence", &sequence) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(sequence == 1u);

    input.payload = invalid_payload;
    input.payload_length = sizeof(invalid_payload);
    assert(cdin_frontend_protocol_validate_payload(input.type, input.payload, input.payload_length) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_FRAME);
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame), &frame_length, 1024u) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);

    input.payload = ack_payload;
    input.payload_length = sizeof(ack_payload);
    input.name_space = "experimental";
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame), &frame_length, 1024u) ==
           CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
    input.name_space = "core";
    input.sequence = 0u;
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame), &frame_length, 1024u) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);

    assert(cdin_frontend_protocol_message_is_inbound(
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE));
    assert(!cdin_frontend_protocol_message_is_inbound(
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT));
    assert(cdin_frontend_protocol_message_requires_driver(
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE));
    assert(cdin_frontend_protocol_validate_payload(
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE,
        pane_resize_payload, sizeof(pane_resize_payload)) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_validate_payload(
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE,
        pane_resize_bad_ratio, sizeof(pane_resize_bad_ratio)) ==
        CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);
    {
        uint8_t boundary_payload[sizeof(pane_resize_payload)];
        memcpy(boundary_payload, pane_resize_payload, sizeof(boundary_payload));
        boundary_payload[11] = 0x03u;
        boundary_payload[12] = 0xe8u; /* 1000: inclusive minimum */
        assert(cdin_frontend_protocol_validate_payload(
            CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE,
            boundary_payload, sizeof(boundary_payload)) == CDIN_FRONTEND_PROTOCOL_OK);
        boundary_payload[11] = 0x23u;
        boundary_payload[12] = 0x27u; /* 8999: inclusive maximum */
        assert(cdin_frontend_protocol_validate_payload(
            CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE,
            boundary_payload, sizeof(boundary_payload)) == CDIN_FRONTEND_PROTOCOL_OK);
        boundary_payload[12] = 0x28u; /* 9000: rejected */
        assert(cdin_frontend_protocol_validate_payload(
            CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE,
            boundary_payload, sizeof(boundary_payload)) ==
            CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA);
    }
    assert(cdin_frontend_protocol_validate_payload(
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT,
        pane_result_payload, sizeof(pane_result_payload)) == CDIN_FRONTEND_PROTOCOL_OK);

    input.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    input.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    input.sequence = 2u;
    input.type = CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE;
    input.name_space = "pane";
    input.payload = pane_resize_payload;
    input.payload_length = sizeof(pane_resize_payload);
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame),
        &frame_length, 1024u) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_decode_frame(frame, frame_length, &output, 1024u) ==
        CDIN_FRONTEND_PROTOCOL_OK);
    assert(output.type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE);
    assert(strcmp(output.name_space, "pane") == 0);
    assert(cdin_frontend_protocol_payload_get_uint(output.payload, output.payload_length,
        "ratio_bp", &ratio_bp) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(ratio_bp == 5000u);

    input.name_space = "core";
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame),
        &frame_length, 1024u) == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
    input.type = CDIN_FRONTEND_PROTOCOL_ACK;
    input.payload = ack_payload;
    input.payload_length = sizeof(ack_payload);
    input.name_space = "pane";
    assert(cdin_frontend_protocol_encode_frame(&input, frame, sizeof(frame),
        &frame_length, 1024u) == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
    return 0;
}
