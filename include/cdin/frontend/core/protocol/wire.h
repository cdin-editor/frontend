#ifndef CDIN_FRONTEND_PROTOCOL_WIRE_H
#define CDIN_FRONTEND_PROTOCOL_WIRE_H

/*
 * Frontend-owned protocol wire types and deterministic-CBOR frame API.
 * This header implements the shared CDIN Frontend Protocol 1.0.0 contract.
 * The implementation is self-contained and does not link the Backend codec.
 * Normative authority: cdin-editor/spec/docs/architecture/protocol-wire.md.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR 1u
#define CDIN_FRONTEND_PROTOCOL_VERSION_MINOR 0u
/* Local package metadata only; wire envelope `v` carries MAJOR.MINOR. */
#define CDIN_FRONTEND_PROTOCOL_VERSION_PATCH 0u
#define CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES 4u
#define CDIN_FRONTEND_PROTOCOL_DEFAULT_MAX_FRAME_BYTES (1024u * 1024u)
#define CDIN_FRONTEND_PROTOCOL_MAX_DEPTH 32u

typedef enum cdin_frontend_protocol_status {
    CDIN_FRONTEND_PROTOCOL_OK = 0,
    CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT,
    CDIN_FRONTEND_PROTOCOL_INVALID_FRAME,
    CDIN_FRONTEND_PROTOCOL_INVALID_SCHEMA,
    CDIN_FRONTEND_PROTOCOL_VERSION_MISMATCH,
    CDIN_FRONTEND_PROTOCOL_UNSUPPORTED,
    CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED,
    CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL,
    CDIN_FRONTEND_PROTOCOL_IO_ERROR,
    CDIN_FRONTEND_PROTOCOL_EOF,
    /* Appended to preserve the numeric values of the Foundation 1.0.0 ABI. */
    CDIN_FRONTEND_PROTOCOL_AGAIN
} cdin_frontend_protocol_status;

typedef enum cdin_frontend_protocol_message_type {
    CDIN_FRONTEND_PROTOCOL_UI_OPTIONS = 1,
    CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE = 2,
    CDIN_FRONTEND_PROTOCOL_UI_LAYOUT = 3,
    CDIN_FRONTEND_PROTOCOL_VIEW_LINES = 4,
    CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE = 5,
    CDIN_FRONTEND_PROTOCOL_UI_STATUSLINE = 6,
    CDIN_FRONTEND_PROTOCOL_UI_PROMPT = 7,
    CDIN_FRONTEND_PROTOCOL_UI_PANEL = 8,
    CDIN_FRONTEND_PROTOCOL_UI_NOTIFY = 9,
    CDIN_FRONTEND_PROTOCOL_REQUEST_CLIPBOARD_GET = 10,
    CDIN_FRONTEND_PROTOCOL_REQUEST_CLIPBOARD_SET = 11,
    CDIN_FRONTEND_PROTOCOL_REQUEST_DIALOG = 12,
    CDIN_FRONTEND_PROTOCOL_ERROR = 13,
    CDIN_FRONTEND_PROTOCOL_UI_CURSOR = 14,
    CDIN_FRONTEND_PROTOCOL_UI_SELECTION = 15,
    CDIN_FRONTEND_PROTOCOL_UI_FLUSH = 16,
    CDIN_FRONTEND_PROTOCOL_HELLO_ACK = 17,
    CDIN_FRONTEND_PROTOCOL_DETACH_ACK = 18,
    CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED = 19,
    CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT = 20,

    CDIN_FRONTEND_PROTOCOL_HELLO = 64,
    CDIN_FRONTEND_PROTOCOL_ATTACH = 65,
    CDIN_FRONTEND_PROTOCOL_DETACH = 66,
    CDIN_FRONTEND_PROTOCOL_INPUT_KEY = 67,
    CDIN_FRONTEND_PROTOCOL_INPUT_TEXT = 68,
    CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE = 69,
    CDIN_FRONTEND_PROTOCOL_INPUT_PASTE = 70,
    CDIN_FRONTEND_PROTOCOL_WINDOW_RESIZE = 71,
    CDIN_FRONTEND_PROTOCOL_FOCUS = 72,
    CDIN_FRONTEND_PROTOCOL_VIEW_REQUEST_LINES = 73,
    CDIN_FRONTEND_PROTOCOL_VIEW_SET_POSITION = 74,
    CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND = 75,
    CDIN_FRONTEND_PROTOCOL_RESPONSE_CLIPBOARD = 76,
    CDIN_FRONTEND_PROTOCOL_RESPONSE_DIALOG = 77,
    CDIN_FRONTEND_PROTOCOL_ACK = 78,
    CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE = 79,
    CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START = 80,
    CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE = 81,
    CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL = 82
} cdin_frontend_protocol_message_type;

typedef struct cdin_frontend_protocol_message_view {
    uint32_t major;
    uint32_t minor;
    uint64_t sequence;
    cdin_frontend_protocol_message_type type;
    const char *name_space;
    const uint8_t *payload;
    size_t payload_length;
} cdin_frontend_protocol_message_view;

typedef ptrdiff_t (*cdin_frontend_protocol_read_fn)(void *context,
                                                     uint8_t *buffer,
                                                     size_t length);
typedef ptrdiff_t (*cdin_frontend_protocol_write_fn)(void *context,
                                                      const uint8_t *buffer,
                                                      size_t length);

typedef struct cdin_frontend_protocol_io {
    void *context;
    cdin_frontend_protocol_read_fn read;
    cdin_frontend_protocol_write_fn write;
} cdin_frontend_protocol_io;

const char *cdin_frontend_protocol_status_name(cdin_frontend_protocol_status status);
const char *cdin_frontend_protocol_message_name(cdin_frontend_protocol_message_type type);
int cdin_frontend_protocol_message_is_inbound(cdin_frontend_protocol_message_type type);
int cdin_frontend_protocol_message_requires_driver(cdin_frontend_protocol_message_type type);

cdin_frontend_protocol_status cdin_frontend_protocol_validate_payload(
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload,
    size_t payload_length);
cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_uint(
    const uint8_t *payload, size_t payload_length, const char *field, uint64_t *out_value);
cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_text(
    const uint8_t *payload, size_t payload_length, const char *field,
    char *output, size_t output_capacity, size_t *out_length);
cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_bool(
    const uint8_t *payload, size_t payload_length, const char *field, int *out_value);
cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_bytes(
    const uint8_t *payload, size_t payload_length, const char *field,
    const uint8_t **out_value, size_t *out_value_length);
cdin_frontend_protocol_status cdin_frontend_protocol_payload_get_raw(
    const uint8_t *payload, size_t payload_length, const char *field,
    const uint8_t **out_value, size_t *out_value_length);
cdin_frontend_protocol_status cdin_frontend_protocol_array_count(
    const uint8_t *array, size_t array_length, size_t *out_count);
cdin_frontend_protocol_status cdin_frontend_protocol_array_get_raw(
    const uint8_t *array, size_t array_length, size_t index,
    const uint8_t **out_value, size_t *out_value_length);
cdin_frontend_protocol_status cdin_frontend_protocol_array_get_text(
    const uint8_t *array, size_t array_length, size_t index,
    char *output, size_t output_capacity, size_t *out_length);

cdin_frontend_protocol_status cdin_frontend_protocol_encode_frame(
    const cdin_frontend_protocol_message_view *message,
    uint8_t *output, size_t output_capacity, size_t *out_length, size_t max_frame_bytes);
cdin_frontend_protocol_status cdin_frontend_protocol_decode_frame(
    const uint8_t *frame, size_t frame_length,
    cdin_frontend_protocol_message_view *out_message, size_t max_frame_bytes);
cdin_frontend_protocol_status cdin_frontend_protocol_read_frame(
    const cdin_frontend_protocol_io *io, uint8_t *buffer,
    size_t capacity, size_t *out_length, size_t max_frame_bytes);
cdin_frontend_protocol_status cdin_frontend_protocol_write_frame(
    const cdin_frontend_protocol_io *io, const uint8_t *frame, size_t frame_length);

#ifdef __cplusplus
}
#endif
#endif
