#include "cdin/frontend/core/session/session.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct fake_stream {
    uint8_t output[2048];
    size_t length;
} fake_stream;

static ptrdiff_t fake_read(void *opaque, uint8_t *buffer, size_t capacity)
{
    (void)opaque; (void)buffer; (void)capacity;
    return 0;
}

static ptrdiff_t fake_write(void *opaque, const uint8_t *bytes, size_t length)
{
    fake_stream *stream = (fake_stream *)opaque;
    if (length > sizeof(stream->output) - stream->length) return -1;
    memcpy(stream->output + stream->length, bytes, length);
    stream->length += length;
    return (ptrdiff_t)length;
}

int main(void)
{
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    cdin_frontend_session session;
    cdin_frontend_protocol_message_view message;
    const uint8_t *arguments;
    size_t arguments_length, text_length;
    char name[64], path[128];
    uint8_t frame[1024];

    memset(&stream, 0, sizeof(stream));
    memset(&io, 0, sizeof(io));
    io.context = &stream;
    io.read = fake_read;
    io.write = fake_write;
    assert(cdin_frontend_client_init(&client, &io, frame, sizeof(frame), 512u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    cdin_frontend_session_init(&session, &client, NULL, "tui");
    session.state = CDIN_FRONTEND_SESSION_ATTACHED;
    session.driver = 1;

    assert(cdin_frontend_session_request_command(&session, "document.open", "/tmp/cdin.bin") ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_decode_frame(stream.output, stream.length, &message, 512u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND);
    assert(message.sequence == 1u);
    assert(cdin_frontend_protocol_payload_get_text(message.payload, message.payload_length,
        "name", name, sizeof(name), &text_length) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(text_length == strlen("document.open") && strcmp(name, "document.open") == 0);
    assert(cdin_frontend_protocol_payload_get_raw(message.payload, message.payload_length,
        "arguments", &arguments, &arguments_length) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_payload_get_text(arguments, arguments_length,
        "path", path, sizeof(path), &text_length) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(strcmp(path, "/tmp/cdin.bin") == 0);

    stream.length = 0u;
    assert(cdin_frontend_session_request_command(&session, "document.new", NULL) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_decode_frame(stream.output, stream.length, &message, 512u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(message.type == CDIN_FRONTEND_PROTOCOL_REQUEST_COMMAND);
    assert(message.sequence == 2u);
    assert(cdin_frontend_protocol_payload_get_raw(message.payload, message.payload_length,
        "arguments", &arguments, &arguments_length) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(arguments_length == 1u && arguments[0] == 0xa0u);

    session.driver = 0;
    assert(cdin_frontend_session_request_command(&session, "document.new", NULL) ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    assert(cdin_frontend_session_request_command(&session, "document.open", "") ==
           CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    return 0;
}
