#include "cdin/frontend/core/session/session.h"

#include <string.h>

typedef struct command_writer {
    uint8_t bytes[1024];
    size_t length;
    int failed;
} command_writer;

static void write_head(command_writer *writer, uint8_t major, uint64_t value)
{
    size_t width, index;
    if (writer->failed) return;
    if (value < 24u) {
        if (writer->length >= sizeof(writer->bytes)) { writer->failed = 1; return; }
        writer->bytes[writer->length++] = (uint8_t)(((uint32_t)major << 5u) | (uint32_t)value);
        return;
    }
    width = value <= UINT8_MAX ? 1u : value <= UINT16_MAX ? 2u :
            value <= UINT32_MAX ? 4u : 8u;
    if (width + 1u > sizeof(writer->bytes) - writer->length) {
        writer->failed = 1; return;
    }
    writer->bytes[writer->length++] = (uint8_t)(((uint32_t)major << 5u) |
        (width == 1u ? 24u : width == 2u ? 25u : width == 4u ? 26u : 27u));
    for (index = 0u; index < width; index++) {
        unsigned shift = (unsigned)((width - index - 1u) * 8u);
        writer->bytes[writer->length++] = (uint8_t)(value >> shift);
    }
}

static void write_text(command_writer *writer, const char *text, size_t length)
{
    if (text == NULL && length != 0u) { writer->failed = 1; return; }
    write_head(writer, 3u, (uint64_t)length);
    if (writer->failed || length > sizeof(writer->bytes) - writer->length) {
        writer->failed = 1; return;
    }
    if (length != 0u) memcpy(writer->bytes + writer->length, text, length);
    writer->length += length;
}

static int valid_utf8(const uint8_t *bytes, size_t length)
{
    size_t offset = 0u;
    while (offset < length) {
        uint8_t first = bytes[offset];
        size_t continuation, index;
        uint32_t codepoint, minimum;
        if (first < 0x80u) { offset++; continue; }
        if (first >= 0xc2u && first <= 0xdfu) {
            continuation = 1u; codepoint = first & 0x1fu; minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            continuation = 2u; codepoint = first & 0x0fu; minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            continuation = 3u; codepoint = first & 0x07u; minimum = 0x10000u;
        } else return 0;
        if (continuation > length - offset - 1u) return 0;
        for (index = 1u; index <= continuation; index++) {
            uint8_t next = bytes[offset + index];
            if ((next & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6u) | (uint32_t)(next & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) return 0;
        offset += continuation + 1u;
    }
    return 1;
}

/*
 * FE-016 / PROTO-041: request.command passes a command name and optional path
 * argument across the public Protocol session. The Frontend never evaluates
 * the command or resolves keymaps; Runtime remains authoritative.
 * Spec: https://github.com/cdin-editor/spec/blob/main/docs/components/frontend.md
 * Spec: https://github.com/cdin-editor/spec/blob/main/docs/architecture/frontend-protocol.md
 */
cdin_frontend_protocol_status cdin_frontend_session_request_command(
    cdin_frontend_session *session, const char *name, const char *path)
{
    command_writer writer;
    size_t name_length, path_length = 0u;
    if (session == NULL || session->client == NULL ||
        session->state != CDIN_FRONTEND_SESSION_ATTACHED || !session->driver ||
        name == NULL || name[0] == '\0') {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    name_length = strlen(name);
    if (name_length > 128u || !valid_utf8((const uint8_t *)name, name_length)) {
        return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    }
    if (path != NULL) {
        path_length = strlen(path);
        if (path_length == 0u || path_length > 512u ||
            !valid_utf8((const uint8_t *)path, path_length)) {
            return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
        }
    }

    memset(&writer, 0, sizeof(writer));
    /* Deterministic CBOR: "name" sorts before the longer "arguments" key. */
    write_head(&writer, 5u, 2u);
    write_text(&writer, "name", 4u);
    write_text(&writer, name, name_length);
    write_text(&writer, "arguments", 9u);
    write_head(&writer, 5u, path != NULL ? 1u : 0u);
    if (path != NULL) {
        write_text(&writer, "path", 4u);
        write_text(&writer, path, path_length);
    }
    if (writer.failed) return CDIN_FRONTEND_PROTOCOL_LIMIT_EXCEEDED;
    return cdin_frontend_client_send(session->client,
        CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND, writer.bytes, writer.length);
}
