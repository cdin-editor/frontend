#include "cdin/frontend/ui/gui_sdl/gui_sdl.h"
#ifdef CDIN_FRONTEND_GUI_SDL_TESTING
#include "gui_sdl_test.h"
#endif

#include <SDL3/SDL.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void trace_step(const char *name)
{
    (void)fprintf(stderr, "[gui-sdl-smoke] %s\\n", name);
    (void)fflush(stderr);
}

typedef struct fake_stream {
    uint8_t input[8192];
    size_t input_length;
    size_t input_offset;
    uint8_t output[8192];
    size_t output_length;
} fake_stream;

static ptrdiff_t fake_read(void *opaque, uint8_t *buffer, size_t length)
{
    fake_stream *stream = (fake_stream *)opaque;
    size_t available;
    size_t amount;
    if (stream == NULL || buffer == NULL) return -1;
    available = stream->input_length - stream->input_offset;
    if (available == 0u) return 0;
    amount = length < available ? length : available;
    memcpy(buffer, stream->input + stream->input_offset, amount);
    stream->input_offset += amount;
    return (ptrdiff_t)amount;
}

static ptrdiff_t fake_write(void *opaque, const uint8_t *bytes, size_t length)
{
    fake_stream *stream = (fake_stream *)opaque;
    if (stream == NULL || bytes == NULL ||
        length > sizeof(stream->output) - stream->output_length) return -1;
    memcpy(stream->output + stream->output_length, bytes, length);
    stream->output_length += length;
    return (ptrdiff_t)length;
}

static int output_contains_type(
    const fake_stream *stream,
    cdin_frontend_protocol_message_type expected_type)
{
    size_t offset = 0u;
    if (stream == NULL) return 0;
    while (offset <= stream->output_length &&
           stream->output_length - offset >= CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        const uint8_t *frame = stream->output + offset;
        uint32_t body_length = ((uint32_t)frame[0] << 24u) |
                               ((uint32_t)frame[1] << 16u) |
                               ((uint32_t)frame[2] << 8u) |
                               (uint32_t)frame[3];
        size_t frame_length;
        cdin_frontend_protocol_message_view message;
        if (body_length == 0u ||
            (size_t)body_length > stream->output_length - offset -
                                  CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) return 0;
        frame_length = CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES + (size_t)body_length;
        if (cdin_frontend_protocol_decode_frame(
                frame, frame_length, &message, 4096u) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
        if (message.type == expected_type) return 1;
        offset += frame_length;
    }
    return 0;
}

static size_t output_count_type(
    const fake_stream *stream,
    cdin_frontend_protocol_message_type expected_type)
{
    size_t offset = 0u;
    size_t count = 0u;
    if (stream == NULL) return 0u;
    while (offset <= stream->output_length &&
           stream->output_length - offset >= CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        const uint8_t *frame = stream->output + offset;
        uint32_t body_length = ((uint32_t)frame[0] << 24u) |
                               ((uint32_t)frame[1] << 16u) |
                               ((uint32_t)frame[2] << 8u) |
                               (uint32_t)frame[3];
        size_t frame_length;
        cdin_frontend_protocol_message_view message;
        if (body_length == 0u ||
            (size_t)body_length > stream->output_length - offset -
                                  CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) return count;
        frame_length = CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES + (size_t)body_length;
        if (cdin_frontend_protocol_decode_frame(
                frame, frame_length, &message, 4096u) != CDIN_FRONTEND_PROTOCOL_OK) {
            return count;
        }
        if (message.type == expected_type) count++;
        offset += frame_length;
    }
    return count;
}


static int output_find_message(
    const fake_stream *stream,
    cdin_frontend_protocol_message_type expected_type,
    size_t occurrence,
    cdin_frontend_protocol_message_view *out_message)
{
    size_t offset = 0u;
    size_t seen = 0u;
    if (stream == NULL || out_message == NULL) return 0;
    while (offset <= stream->output_length &&
           stream->output_length - offset >= CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        const uint8_t *frame = stream->output + offset;
        uint32_t body_length = ((uint32_t)frame[0] << 24u) |
                               ((uint32_t)frame[1] << 16u) |
                               ((uint32_t)frame[2] << 8u) |
                               (uint32_t)frame[3];
        size_t frame_length;
        cdin_frontend_protocol_message_view message;
        if (body_length == 0u ||
            (size_t)body_length > stream->output_length - offset -
                                  CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) return 0;
        frame_length = CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES + (size_t)body_length;
        if (cdin_frontend_protocol_decode_frame(
                frame, frame_length, &message, 4096u) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
        if (message.type == expected_type) {
            if (seen == occurrence) {
                *out_message = message;
                return 1;
            }
            seen++;
        }
        offset += frame_length;
    }
    return 0;
}

typedef struct smoke_cbor_writer {
    uint8_t *data;
    size_t capacity;
    size_t length;
} smoke_cbor_writer;

static void smoke_cbor_head(
    smoke_cbor_writer *writer, uint8_t major, uint64_t value)
{
    size_t width;
    size_t index;
    assert(writer != NULL);
    if (value < 24u) {
        assert(writer->length + 1u <= writer->capacity);
        writer->data[writer->length++] =
            (uint8_t)((uint8_t)(major << 5u) | (uint8_t)value);
        return;
    }
    if (value <= UINT8_MAX) width = 1u;
    else if (value <= UINT16_MAX) width = 2u;
    else if (value <= UINT32_MAX) width = 4u;
    else width = 8u;
    assert(writer->length + 1u + width <= writer->capacity);
    writer->data[writer->length++] = (uint8_t)(
        (uint8_t)(major << 5u) | (width == 1u ? 24u :
        width == 2u ? 25u : width == 4u ? 26u : 27u));
    for (index = 0u; index < width; ++index) {
        size_t shift = (width - index - 1u) * 8u;
        writer->data[writer->length++] = (uint8_t)(value >> shift);
    }
}

static void smoke_cbor_uint(smoke_cbor_writer *writer, uint64_t value)
{
    smoke_cbor_head(writer, 0u, value);
}

static void smoke_cbor_map(smoke_cbor_writer *writer, size_t count)
{
    smoke_cbor_head(writer, 5u, (uint64_t)count);
}

static void smoke_cbor_text(smoke_cbor_writer *writer, const char *value)
{
    size_t length = strlen(value);
    smoke_cbor_head(writer, 3u, (uint64_t)length);
    assert(writer->length + length <= writer->capacity);
    memcpy(writer->data + writer->length, value, length);
    writer->length += length;
}

static void smoke_cbor_leaf(
    smoke_cbor_writer *writer, const char *node_id, const char *view_id)
{
    smoke_cbor_map(writer, 4u);
    smoke_cbor_text(writer, "id");
    smoke_cbor_text(writer, node_id);
    smoke_cbor_text(writer, "kind");
    smoke_cbor_text(writer, "leaf");
    smoke_cbor_text(writer, "views");
    smoke_cbor_head(writer, 4u, 1u);
    smoke_cbor_text(writer, view_id);
    smoke_cbor_text(writer, "active_view");
    smoke_cbor_text(writer, view_id);
}

static size_t smoke_two_view_layout(
    uint8_t *output, size_t capacity, uint64_t ratio_bp, uint64_t revision)
{
    smoke_cbor_writer writer = { output, capacity, 0u };
    smoke_cbor_map(&writer, 2u);
    smoke_cbor_text(&writer, "layout");
    smoke_cbor_map(&writer, 2u);
    smoke_cbor_text(&writer, "root");
    smoke_cbor_map(&writer, 6u);
    smoke_cbor_text(&writer, "id");
    smoke_cbor_text(&writer, "split-main");
    smoke_cbor_text(&writer, "axis");
    smoke_cbor_text(&writer, "horizontal");
    smoke_cbor_text(&writer, "kind");
    smoke_cbor_text(&writer, "split");
    smoke_cbor_text(&writer, "first");
    smoke_cbor_leaf(&writer, "leaf-left", "view-left");
    smoke_cbor_text(&writer, "second");
    smoke_cbor_leaf(&writer, "leaf-right", "view-right");
    smoke_cbor_text(&writer, "ratio_bp");
    smoke_cbor_uint(&writer, ratio_bp);
    smoke_cbor_text(&writer, "active_leaf");
    smoke_cbor_text(&writer, "leaf-left");
    smoke_cbor_text(&writer, "revision");
    smoke_cbor_uint(&writer, revision);
    return writer.length;
}

static size_t smoke_view_cursor(
    uint8_t *output, size_t capacity, const char *view_id,
    uint64_t version, uint64_t position)
{
    smoke_cbor_writer writer = { output, capacity, 0u };
    smoke_cbor_map(&writer, 3u);
    smoke_cbor_text(&writer, "version");
    smoke_cbor_uint(&writer, version);
    smoke_cbor_text(&writer, "view_id");
    smoke_cbor_text(&writer, view_id);
    smoke_cbor_text(&writer, "position");
    smoke_cbor_uint(&writer, position);
    return writer.length;
}

static size_t smoke_snapshot_flush(
    uint8_t *output, size_t capacity, uint64_t revision, uint64_t sequence)
{
    smoke_cbor_writer writer = { output, capacity, 0u };
    smoke_cbor_map(&writer, 3u);
    smoke_cbor_text(&writer, "scope");
    smoke_cbor_text(&writer, "snapshot");
    smoke_cbor_text(&writer, "revision");
    smoke_cbor_uint(&writer, revision);
    smoke_cbor_text(&writer, "sequence");
    smoke_cbor_uint(&writer, sequence);
    return writer.length;
}

static cdin_frontend_protocol_message_view smoke_message(
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload,
    size_t payload_length)
{
    cdin_frontend_protocol_message_view message;
    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.type = type;
    message.name_space = "core";
    message.payload = payload;
    message.payload_length = payload_length;
    return message;
}

static void smoke_queue_inbound(
    fake_stream *stream,
    cdin_frontend_protocol_message_view *message)
{
    uint8_t frame[1024];
    size_t frame_length = 0u;
    assert(stream != NULL && message != NULL);
    assert(cdin_frontend_protocol_encode_frame(
        message, frame, sizeof(frame), &frame_length, 4096u) ==
        CDIN_FRONTEND_PROTOCOL_OK);
    assert(frame_length <= sizeof(stream->input) - stream->input_length);
    memcpy(stream->input + stream->input_length, frame, frame_length);
    stream->input_length += frame_length;
}


static int smoke_output_find_type(
    const fake_stream *stream,
    cdin_frontend_protocol_message_type expected_type,
    cdin_frontend_protocol_message_view *out_message)
{
    size_t offset = 0u;
    if (stream == NULL || out_message == NULL) return 0;
    while (offset <= stream->output_length &&
           stream->output_length - offset >= CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        const uint8_t *frame = stream->output + offset;
        uint32_t body_length = ((uint32_t)frame[0] << 24u) |
                               ((uint32_t)frame[1] << 16u) |
                               ((uint32_t)frame[2] << 8u) |
                               (uint32_t)frame[3];
        size_t frame_length;
        cdin_frontend_protocol_message_view message;
        if (body_length == 0u ||
            (size_t)body_length > stream->output_length - offset -
                                  CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) return 0;
        frame_length = CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES + (size_t)body_length;
        if (cdin_frontend_protocol_decode_frame(
                frame, frame_length, &message, 4096u) != CDIN_FRONTEND_PROTOCOL_OK) return 0;
        if (message.type == expected_type) {
            *out_message = message;
            return 1;
        }
        offset += frame_length;
    }
    return 0;
}

static size_t smoke_pane_result_payload(uint8_t *output, size_t capacity)
{
    smoke_cbor_writer writer = { output, capacity, 0u };
    smoke_cbor_map(&writer, 3u);
    smoke_cbor_text(&writer, "status");
    smoke_cbor_text(&writer, "applied");
    smoke_cbor_text(&writer, "request_id");
    smoke_cbor_uint(&writer, 1u);
    smoke_cbor_text(&writer, "layout_revision");
    smoke_cbor_uint(&writer, 2u);
    return writer.length;
}

static void smoke_apply_inbound(
    cdin_frontend_gui_sdl *gui,
    cdin_frontend_session *session,
    fake_stream *stream,
    cdin_frontend_protocol_message_type type,
    const char *name_space,
    const uint8_t *payload,
    size_t payload_length,
    uint64_t envelope_sequence,
    int expected_committed)
{
    cdin_frontend_protocol_message_view message = smoke_message(
        type, payload, payload_length);
    cdin_frontend_protocol_message_view received;
    int committed = 0;
    message.name_space = name_space;
    message.sequence = envelope_sequence;
    smoke_queue_inbound(stream, &message);
    assert(cdin_frontend_session_receive(session, &received) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(received.type == type);
    assert(strcmp(received.name_space, name_space) == 0);
    assert(cdin_frontend_gui_sdl_apply_message(gui, &received, &committed) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(committed == expected_committed);
}

static void smoke_push_mouse_button(
    SDL_EventType type, uint8_t button, SDL_MouseID which, float x, float y)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.button.type = type;
    event.button.button = button;
    event.button.which = which;
    event.button.down = type == SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.x = x;
    event.button.y = y;
    assert(SDL_PushEvent(&event));
}

static void smoke_push_mouse_motion(SDL_MouseID which, float x, float y)
{
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.which = which;
    event.motion.x = x;
    event.motion.y = y;
    assert(SDL_PushEvent(&event));
}

static void smoke_test_no_splitter_capability(
    cdin_frontend_gui_sdl *gui,
    fake_stream *stream)
{
    int quit_requested = 0;
    size_t pane_count;
    if (cdin_frontend_capability_has(
            cdin_frontend_gui_sdl_supported_capabilities(),
            CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE)) return;
    pane_count = output_count_type(
        stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE);
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                            (SDL_MouseID)41, 400.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_motion((SDL_MouseID)41, 460.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                            (SDL_MouseID)41, 460.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count);
}

static void smoke_test_splitter_lifecycle(
    cdin_frontend_gui_sdl *gui,
    cdin_frontend_session *session,
    fake_stream *stream)
{
    SDL_Event event;
    cdin_frontend_protocol_message_view request;
    size_t pane_count;
    uint8_t payload[512];
    size_t payload_length;
    uint64_t request_id = 0u;
    uint64_t revision = 0u;
    uint64_t ratio_bp = 0u;
    char split_id[129];
    size_t split_id_length = 0u;
    int quit_requested = 0;

    if (!cdin_frontend_capability_has(
            cdin_frontend_gui_sdl_supported_capabilities(),
            CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE)) return;

    pane_count = output_count_type(
        stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE);

    /* The divider begins at x=400 in the 800-pixel drawable viewport. */
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                            (SDL_MouseID)42, 400.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count);

    smoke_push_mouse_motion((SDL_MouseID)42, 480.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count);
    assert(cdin_frontend_gui_sdl_render(gui) == CDIN_FRONTEND_GUI_SDL_OK);

    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                            (SDL_MouseID)42, 480.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count + 1u);
    assert(smoke_output_find_type(
        stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE, &request));
    assert(strcmp(request.name_space, "pane") == 0);
    assert(cdin_frontend_protocol_payload_get_uint(
        request.payload, request.payload_length, "request_id",
        &request_id) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_payload_get_uint(
        request.payload, request.payload_length, "layout_revision",
        &revision) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_payload_get_uint(
        request.payload, request.payload_length, "ratio_bp",
        &ratio_bp) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_protocol_payload_get_text(
        request.payload, request.payload_length, "split_id",
        split_id, sizeof(split_id), &split_id_length) ==
        CDIN_FRONTEND_PROTOCOL_OK);
    assert(request_id == 1u && revision == 1u && ratio_bp == 6000u);
    assert(split_id_length == 10u && strcmp(split_id, "split-main") == 0);

    /* Result alone does not commit the local model. Commit requires the
     * following complete semantic layout and matching snapshot flush. */
    payload_length = smoke_pane_result_payload(payload, sizeof(payload));
    smoke_apply_inbound(gui, session, stream,
        CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT, "pane",
        payload, payload_length, 3u, 0);
    payload_length = smoke_two_view_layout(payload, sizeof(payload), 6000u, 2u);
    smoke_apply_inbound(gui, session, stream,
        CDIN_FRONTEND_PROTOCOL_UI_LAYOUT, "core",
        payload, payload_length, 4u, 0);
    payload_length = smoke_snapshot_flush(payload, sizeof(payload), 2u, 2u);
    smoke_apply_inbound(gui, session, stream,
        CDIN_FRONTEND_PROTOCOL_UI_FLUSH, "core",
        payload, payload_length, 5u, 1);

    /* Escape cancels preview and must prevent a second pane request. */
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                            (SDL_MouseID)43, 480.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_motion((SDL_MouseID)43, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_ESCAPE;
    event.key.down = true;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                            (SDL_MouseID)43, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count + 1u);

    /* Focus loss cancels the next drag; regain focus before continuing. */
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                            (SDL_MouseID)44, 480.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_motion((SDL_MouseID)44, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    event.window.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                            (SDL_MouseID)44, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    event.window.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count + 1u);

    /* SDL touch cancellation maps to the same local cancel transition. */
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                            (SDL_MouseID)46, 480.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_motion((SDL_MouseID)46, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_FINGER_CANCELED;
    event.tfinger.type = SDL_EVENT_FINGER_CANCELED;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                            (SDL_MouseID)46, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count + 1u);

    /* A changed committed revision cancels an in-progress preview. */
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                            (SDL_MouseID)45, 480.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    smoke_push_mouse_motion((SDL_MouseID)45, 520.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    payload_length = smoke_two_view_layout(payload, sizeof(payload), 5500u, 3u);
    smoke_apply_inbound(gui, session, stream,
        CDIN_FRONTEND_PROTOCOL_UI_LAYOUT, "core",
        payload, payload_length, 6u, 0);
    payload_length = smoke_snapshot_flush(payload, sizeof(payload), 3u, 3u);
    smoke_apply_inbound(gui, session, stream,
        CDIN_FRONTEND_PROTOCOL_UI_FLUSH, "core",
        payload, payload_length, 7u, 1);
    smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                            (SDL_MouseID)45, 440.0f, 100.0f);
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
           pane_count + 1u);

#ifdef CDIN_FRONTEND_GUI_SDL_TESTING
    /* If the platform cannot capture the pointer, the adapter must not send
     * a speculative resize; it falls back to the ordinary mouse stream. */
    {
        size_t mouse_before = output_count_type(
            stream, CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE);
#ifdef CDIN_FRONTEND_GUI_SDL_TESTING
        cdin_frontend_gui_sdl_test_set_capture_available(0);
#endif
        smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_DOWN, SDL_BUTTON_LEFT,
                                (SDL_MouseID)46, 440.0f, 100.0f);
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        smoke_push_mouse_motion((SDL_MouseID)46, 460.0f, 100.0f);
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        smoke_push_mouse_button(SDL_EVENT_MOUSE_BUTTON_UP, SDL_BUTTON_LEFT,
                                (SDL_MouseID)46, 460.0f, 100.0f);
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESIZE) ==
               pane_count + 1u);
        assert(output_count_type(stream, CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE) >=
               mouse_before + 3u);
#ifdef CDIN_FRONTEND_GUI_SDL_TESTING
        cdin_frontend_gui_sdl_test_set_capture_available(1);
#endif
    }
#endif
}

static void smoke_apply_two_view_snapshot(
    cdin_frontend_gui_sdl *gui,
    cdin_frontend_session *session,
    fake_stream *stream)
{
    uint8_t payload[512];
    size_t payload_length;
    cdin_frontend_protocol_message_view message;
    cdin_frontend_protocol_message_view received;
    int committed = 0;

    payload_length = smoke_two_view_layout(payload, sizeof(payload), 5000u, 1u);
    message = smoke_message(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                            payload, payload_length);
    message.sequence = 1u;
    smoke_queue_inbound(stream, &message);

    payload_length = smoke_view_cursor(
        payload, sizeof(payload), "view-left", 1u, 0u);
    message = smoke_message(CDIN_FRONTEND_PROTOCOL_UI_CURSOR,
                            payload, payload_length);
    message.sequence = 2u;
    smoke_queue_inbound(stream, &message);

    payload_length = smoke_snapshot_flush(payload, sizeof(payload), 1u, 1u);
    message = smoke_message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                            payload, payload_length);
    message.sequence = 3u;
    smoke_queue_inbound(stream, &message);

    assert(cdin_frontend_session_receive(session, &received) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(received.type == CDIN_FRONTEND_PROTOCOL_UI_LAYOUT);
    assert(cdin_frontend_gui_sdl_apply_message(gui, &received, &committed) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(committed == 0);

    assert(cdin_frontend_session_receive(session, &received) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(received.type == CDIN_FRONTEND_PROTOCOL_UI_CURSOR);
    assert(cdin_frontend_gui_sdl_apply_message(gui, &received, &committed) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(committed == 0);

    assert(cdin_frontend_session_receive(session, &received) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(received.type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH);
    assert(cdin_frontend_gui_sdl_apply_message(gui, &received, &committed) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(committed != 0);
}

static int output_contains_expected_mouse(const fake_stream *stream)
{
    size_t offset = 0u;
    if (stream == NULL) return 0;
    while (offset <= stream->output_length &&
           stream->output_length - offset >= CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
        const uint8_t *frame = stream->output + offset;
        uint32_t body_length = ((uint32_t)frame[0] << 24u) |
                               ((uint32_t)frame[1] << 16u) |
                               ((uint32_t)frame[2] << 8u) |
                               (uint32_t)frame[3];
        size_t frame_length;
        cdin_frontend_protocol_message_view message;
        if (body_length == 0u ||
            (size_t)body_length > stream->output_length - offset -
                                  CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES) {
            return 0;
        }
        frame_length = CDIN_FRONTEND_PROTOCOL_FRAME_PREFIX_BYTES + (size_t)body_length;
        if (cdin_frontend_protocol_decode_frame(
                frame, frame_length, &message, 4096u) != CDIN_FRONTEND_PROTOCOL_OK) {
            return 0;
        }
        if (message.type == CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE) {
            char action[16];
            char button[16];
            char target[64];
            size_t action_length = 0u;
            size_t button_length = 0u;
            size_t target_length = 0u;
            const uint8_t *position = NULL;
            size_t position_length = 0u;
            uint64_t x = 0u;
            uint64_t y = 0u;
            if (cdin_frontend_protocol_payload_get_text(
                    message.payload, message.payload_length, "action",
                    action, sizeof(action), &action_length) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_text(
                    message.payload, message.payload_length, "button",
                    button, sizeof(button), &button_length) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_text(
                    message.payload, message.payload_length, "target",
                    target, sizeof(target), &target_length) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_raw(
                    message.payload, message.payload_length, "position",
                    &position, &position_length) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_uint(
                    position, position_length, "x", &x) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_uint(
                    position, position_length, "y", &y) == CDIN_FRONTEND_PROTOCOL_OK) {
                if (action_length == 5u && strcmp(action, "press") == 0 &&
                    button_length == 4u && strcmp(button, "left") == 0 &&
                    target_length == 10u && strcmp(target, "view-right") == 0 &&
                    x == 600u && y == 100u) {
                    return 1;
                }
            }
        }
        offset += frame_length;
    }
    return 0;
}

int main(void)
{
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    cdin_frontend_session session;
    cdin_frontend_gui_sdl_config config;
    cdin_frontend_gui_sdl *gui = NULL;
    uint8_t client_buffer[4096];
    SDL_Event event;
    SDL_Window *unrelated_window = NULL;
    size_t key_count_before_unrelated_key;
    cdin_frontend_session cell_session;
    int quit_requested = 0;

#ifdef CDIN_FRONTEND_GUI_SDL_TESTING
    cdin_frontend_gui_sdl_test_set_capture_available(1);
#endif
    trace_step("begin");
    memset(&stream, 0, sizeof(stream));
    memset(&io, 0, sizeof(io));
    io.context = &stream;
    io.read = fake_read;
    io.write = fake_write;
    assert(cdin_frontend_client_init(&client, &io, client_buffer,
                                     sizeof(client_buffer), 1024u) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    cdin_frontend_session_init(&session, &client, NULL, "gui-sdl-smoke");
    session.state = CDIN_FRONTEND_SESSION_ATTACHED;
    session.driver = 1;
    /* This smoke bypasses the negotiated ATTACH handshake. Populate the
     * public state that session_attach() normally establishes. */
    session.units = "pixels";
    session.width = 800u;
    session.height = 600u;
    session.accepted_capabilities =
        cdin_frontend_gui_sdl_supported_capabilities() |
        CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE);
    assert(cdin_frontend_capability_has(
        session.accepted_capabilities, CDIN_FRONTEND_CAP_IME_COMPOSITION));
    (void)snprintf(session.view_id, sizeof(session.view_id), "%s", "smoke-view");

    {
        static const uint8_t preedit[] = { 0xc3u, 0xa9u };
        static const uint8_t committed[] = { 0xc3u, 0xa9u };
        cdin_frontend_session legacy_session = session;
        legacy_session.accepted_capabilities &=
            ~CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_IME_COMPOSITION);
        assert(cdin_frontend_session_ime_start(
            &legacy_session, 1u, "smoke-view", 1u) ==
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
        assert(cdin_frontend_session_ime_update(
            &legacy_session, 1u, preedit, sizeof(preedit), 2u, 0u, 2u) ==
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
        assert(cdin_frontend_session_ime_cancel(
            &legacy_session, 1u, "ime_cancelled") ==
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
        assert(cdin_frontend_session_ime_commit(
            &legacy_session, 1u, committed, sizeof(committed)) ==
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED);
        assert(cdin_frontend_session_ime_cancel(
            &session, 1u, "selection_changed") ==
            CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
    }

    memset(&config, 0, sizeof(config));
    config.session = &session;
    config.window_title = "CDIN GUI-SDL adapter smoke";
    config.width = 800u;
    config.height = 600u;
    config.visible_line_capacity = 16u;

    cell_session = session;
    cell_session.units = "cells";
    config.session = &cell_session;
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT);
    assert(gui == NULL);
    config.session = &session;

    /* Force a late startup failure after video initialization and window
     * creation. The adapter must destroy partial resources and release the
     * video subsystem before a subsequent successful create. */
    trace_step("force renderer startup failure");
    assert(SDL_SetHintWithPriority("SDL_RENDER_DRIVER",
                                    "cdin_nonexistent_renderer",
                                    SDL_HINT_OVERRIDE));
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_SDL_ERROR);
    trace_step("renderer startup failure returned");
    assert(gui == NULL);
    assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);
    trace_step("partial startup resources released");
    SDL_ResetHint("SDL_RENDER_DRIVER");

    trace_step("create normal adapter");
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(gui != NULL);
    trace_step("render initial frame");
    assert(cdin_frontend_gui_sdl_render(gui) == CDIN_FRONTEND_GUI_SDL_OK);
    trace_step("apply two-view semantic snapshot");
    smoke_apply_two_view_snapshot(gui, &session, &stream);

    trace_step("verify splitter pointer lifecycle");
    smoke_test_splitter_lifecycle(gui, &session, &stream);

    trace_step("verify non-negotiated splitter fallback");
    smoke_test_no_splitter_capability(gui, &stream);

    /* A device-loss notification must recreate the renderer and leave the
     * adapter usable. The next frame is rebuilt from the presentation model. */
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_RENDER_DEVICE_LOST;
    event.render.type = SDL_EVENT_RENDER_DEVICE_LOST;
    /* A zero window ID is accepted by the adapter for renderer-wide events. */
    event.render.windowID = 0u;
    trace_step("push renderer device-loss event");
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(cdin_frontend_gui_sdl_render(gui) == CDIN_FRONTEND_GUI_SDL_OK);
    trace_step("renderer device-loss recovery verified");

    /* SDL_PushEvent supplies a real SDL input event through the same adapter
     * pump as a native mouse. The public Core Session serializes it as
     * input.mouse, without any Runtime/keymap interpretation in the GUI. */
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.x = 600.0f;
    event.button.y = 100.0f;
    trace_step("push mouse event");
    assert(SDL_PushEvent(&event));
    trace_step("pump mouse event");
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(quit_requested == 0);
    assert(stream.output_length != 0u);
    assert(output_contains_expected_mouse(&stream));
    trace_step("mouse protocol frame verified");


#ifdef CDIN_FRONTEND_GUI_SDL_TESTING
    /* Native SDL preedit stays local to the GUI and uses exact UTF-8 byte
     * offsets on the wire. SDL reports UTF-8 character indices instead. */
    {
        size_t starts_before = output_count_type(
            &stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START);
        size_t updates_before = output_count_type(
            &stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE);
        cdin_frontend_protocol_message_view output_message;
        uint64_t composition_id = 0u;
        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_TEXT_EDITING;
        event.edit.type = SDL_EVENT_TEXT_EDITING;
        event.edit.text = "éب";
        event.edit.start = 1;
        event.edit.length = 1;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(cdin_frontend_gui_sdl_test_ime_active(gui));
        assert(cdin_frontend_gui_sdl_test_ime_id(gui) == 1u);
        assert(cdin_frontend_session_next_ime_composition_id(&session) == 2u);
        assert(cdin_frontend_session_ime_start(
            &session, 1u, "smoke-view", 1u) ==
            CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT);
        assert(cdin_frontend_gui_sdl_test_ime_preedit_length(gui) == 4u);
        assert(cdin_frontend_gui_sdl_test_ime_cursor_byte(gui) == 2u);
        assert(cdin_frontend_gui_sdl_test_ime_selection_end_byte(gui) == 4u);
        assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_START) ==
               starts_before + 1u);
        assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_UPDATE) ==
               updates_before + 1u);
        assert(cdin_frontend_gui_sdl_render(gui) == CDIN_FRONTEND_GUI_SDL_OK);

        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.type = SDL_EVENT_TEXT_INPUT;
        event.text.text = "éب";
        event.text.timestamp = 424242u;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(!cdin_frontend_gui_sdl_test_ime_active(gui));
        assert(output_find_message(&stream, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT, 0u,
                                   &output_message));
        assert(cdin_frontend_protocol_payload_get_uint(
            output_message.payload, output_message.payload_length,
            "composition_id", &composition_id) == CDIN_FRONTEND_PROTOCOL_OK);
        assert(composition_id == 1u);

        {
            size_t text_frames_after_commit = output_count_type(
                &stream, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT);
            memset(&event, 0, sizeof(event));
            event.type = SDL_EVENT_TEXT_INPUT;
            event.text.type = SDL_EVENT_TEXT_INPUT;
            event.text.text = "éب";
            event.text.timestamp = 424242u;
            assert(SDL_PushEvent(&event));
            assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
                   CDIN_FRONTEND_GUI_SDL_OK);
            assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT) ==
                   text_frames_after_commit);
        }
    }

    /* Combining marks remain byte-addressable even though SDL reports
     * character positions, not grapheme-cluster positions. Escape cancels. */
    {
        size_t cancels_before = output_count_type(
            &stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL);
        cdin_frontend_protocol_message_view output_message;
        char reason[32];
        size_t reason_length = 0u;
        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_TEXT_EDITING;
        event.edit.type = SDL_EVENT_TEXT_EDITING;
        event.edit.text = "é";
        event.edit.start = 1;
        event.edit.length = 1;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(cdin_frontend_gui_sdl_test_ime_active(gui));
        assert(cdin_frontend_gui_sdl_test_ime_id(gui) == 2u);
        assert(cdin_frontend_gui_sdl_test_ime_cursor_byte(gui) == 1u);
        assert(cdin_frontend_gui_sdl_test_ime_selection_end_byte(gui) == 3u);

        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_KEY_DOWN;
        event.key.type = SDL_EVENT_KEY_DOWN;
        event.key.key = SDLK_ESCAPE;
        event.key.down = true;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(!cdin_frontend_gui_sdl_test_ime_active(gui));
        assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL) ==
               cancels_before + 1u);
        assert(output_find_message(&stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL,
                                   cancels_before, &output_message));
        assert(cdin_frontend_protocol_payload_get_text(
            output_message.payload, output_message.payload_length, "reason",
            reason, sizeof(reason), &reason_length) == CDIN_FRONTEND_PROTOCOL_OK);
        assert(strcmp(reason, "ime_cancelled") == 0);
    }

    /* An empty preedit is a valid update, not a commit or a cancellation. */
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_TEXT_EDITING;
    event.edit.type = SDL_EVENT_TEXT_EDITING;
    event.edit.text = "";
    event.edit.start = 0;
    event.edit.length = 0;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(cdin_frontend_gui_sdl_test_ime_active(gui));
    assert(cdin_frontend_gui_sdl_test_ime_id(gui) == 3u);
    assert(cdin_frontend_gui_sdl_test_ime_preedit_length(gui) == 0u);

    /* Backend-forced cancellation is received through Core Session and
     * clears only the matching local composition identifier. */
    {
        static const uint8_t stale_cancel_payload[] = {
            0xa2u, 0x62u, 'i','d', 0x18u, 0x63u,
            0x66u, 'r','e','a','s','o','n', 0x6cu,
            'v','i','e','w','_','c','h','a','n','g','e','d'
        };
        static const uint8_t backend_cancel_payload[] = {
            0xa2u, 0x62u, 'i','d', 0x03u,
            0x66u, 'r','e','a','s','o','n', 0x6cu,
            'v','i','e','w','_','c','h','a','n','g','e','d'
        };
        cdin_frontend_protocol_message_view cancel_message;
        cdin_frontend_protocol_message_view received;
        int committed = 0;
        cancel_message = smoke_message(CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL,
            stale_cancel_payload, sizeof(stale_cancel_payload));
        cancel_message.name_space = "ime";
        cancel_message.sequence = 4u;
        smoke_queue_inbound(&stream, &cancel_message);
        assert(cdin_frontend_session_receive(&session, &received) ==
               CDIN_FRONTEND_PROTOCOL_OK);
        assert(strcmp(received.name_space, "ime") == 0);
        assert(cdin_frontend_gui_sdl_apply_message(gui, &received, &committed) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(cdin_frontend_gui_sdl_test_ime_active(gui));
        assert(cdin_frontend_gui_sdl_test_ime_id(gui) == 3u);

        cancel_message = smoke_message(CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL,
            backend_cancel_payload, sizeof(backend_cancel_payload));
        cancel_message.name_space = "ime";
        cancel_message.sequence = 5u;
        smoke_queue_inbound(&stream, &cancel_message);
        assert(cdin_frontend_session_receive(&session, &received) ==
               CDIN_FRONTEND_PROTOCOL_OK);
        assert(cdin_frontend_gui_sdl_apply_message(gui, &received, &committed) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(!cdin_frontend_gui_sdl_test_ime_active(gui));
    }

    /* Focus loss sends the Frontend-owned cancellation reason before focus(false). */
    {
        size_t cancels_before = output_count_type(
            &stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL);
        cdin_frontend_protocol_message_view output_message;
        char reason[32];
        size_t reason_length = 0u;
        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_TEXT_EDITING;
        event.edit.type = SDL_EVENT_TEXT_EDITING;
        event.edit.text = "focus";
        event.edit.start = 2;
        event.edit.length = 1;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(cdin_frontend_gui_sdl_test_ime_active(gui));

        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        event.window.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(!cdin_frontend_gui_sdl_test_ime_active(gui));
        assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL) ==
               cancels_before + 1u);
        assert(output_find_message(&stream, CDIN_FRONTEND_PROTOCOL_IME_COMPOSITION_CANCEL,
                                   cancels_before, &output_message));
        assert(cdin_frontend_protocol_payload_get_text(
            output_message.payload, output_message.payload_length, "reason",
            reason, sizeof(reason), &reason_length) == CDIN_FRONTEND_PROTOCOL_OK);
        assert(strcmp(reason, "focus_lost") == 0);
        /* Restore the Driver focus before the existing committed-text smoke. */
        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        event.window.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
        assert(SDL_PushEvent(&event));
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
    }

#endif /* CDIN_FRONTEND_GUI_SDL_TESTING */

    /* Exercise the same SDL queue path used by the CDIN composition smoke.
     * A committed text event and a Ctrl+S key-down must become separate
     * Protocol messages; the adapter never resolves the Runtime keymap. */
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = "GUI_SDL_E2E";
    trace_step("push text input");
    assert(SDL_PushEvent(&event));
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_S;
    event.key.mod = SDL_KMOD_CTRL;
    event.key.down = true;
    trace_step("push Ctrl+S key-down");
    assert(SDL_PushEvent(&event));
    trace_step("pump text and key input");
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_contains_type(&stream, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT));
    assert(output_contains_type(&stream, CDIN_FRONTEND_PROTOCOL_INPUT_KEY));
    trace_step("text and key Protocol frames verified");

    /* SDL's event queue is process-wide. A keystroke addressed to another
     * window must not leak into this editor's borrowed Core Session. */
    unrelated_window = SDL_CreateWindow("unrelated SDL host window", 320, 200, 0u);
    assert(unrelated_window != NULL);
    key_count_before_unrelated_key = output_count_type(
        &stream, CDIN_FRONTEND_PROTOCOL_INPUT_KEY);
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(unrelated_window);
    event.key.key = SDLK_F1;
    event.key.down = true;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_INPUT_KEY) ==
           key_count_before_unrelated_key);
    SDL_DestroyWindow(unrelated_window);
    trace_step("foreign-window key event ignored");

    trace_step("destroy adapter");
    cdin_frontend_gui_sdl_destroy(gui);
    assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);

    /* Repeated create/render/destroy cycles must release all SDL video state. */
    {
        size_t cycle;
        for (cycle = 0u; cycle < 3u; ++cycle) {
            gui = NULL;
            assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
                   CDIN_FRONTEND_GUI_SDL_OK);
            assert(gui != NULL);
            assert(cdin_frontend_gui_sdl_render(gui) == CDIN_FRONTEND_GUI_SDL_OK);
            cdin_frontend_gui_sdl_destroy(gui);
            gui = NULL;
            assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);
        }
    }

    /* If the renderer cannot be recreated after device loss/reset, the
     * adapter reports an SDL error and remains non-renderable until destroy. */
    gui = NULL;
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(gui != NULL);
    assert(SDL_SetHintWithPriority("SDL_RENDER_DRIVER",
                                    "cdin_nonexistent_renderer",
                                    SDL_HINT_OVERRIDE));
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_RENDER_DEVICE_RESET;
    event.render.type = SDL_EVENT_RENDER_DEVICE_RESET;
    event.render.windowID = 0u;
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_SDL_ERROR);
    SDL_ResetHint("SDL_RENDER_DRIVER");
    assert(cdin_frontend_gui_sdl_render(gui) ==
           CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT);
    cdin_frontend_gui_sdl_destroy(gui);
    gui = NULL;
    assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);

    /* The GUI borrows Session lifetime. After detach, cached frames must not
     * render, inbound flushes must not be ACKed, and queued input is discarded.
     * The native quit event must still be delivered to the host loop. */
    gui = NULL;
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(gui != NULL);
    session.state = CDIN_FRONTEND_SESSION_DETACHED;

    {
        size_t ack_count = output_count_type(
            &stream, CDIN_FRONTEND_PROTOCOL_ACK);
        uint8_t payload[128];
        size_t payload_length = smoke_snapshot_flush(payload, sizeof(payload), 1u, 3u);
        cdin_frontend_protocol_message_view message = smoke_message(
            CDIN_FRONTEND_PROTOCOL_UI_FLUSH, payload, payload_length);
        int committed = 1;
        message.sequence = 3u;
        assert(cdin_frontend_gui_sdl_apply_message(gui, &message, &committed) ==
               CDIN_FRONTEND_GUI_SDL_INVALID_STATE);
        assert(committed == 0);
        assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_ACK) == ack_count);
    }
    assert(cdin_frontend_gui_sdl_render(gui) ==
           CDIN_FRONTEND_GUI_SDL_INVALID_STATE);

    {
        size_t text_count = output_count_type(
            &stream, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT);
        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.type = SDL_EVENT_TEXT_INPUT;
        event.text.text = "DETACHED_INPUT_MUST_BE_DROPPED";
        assert(SDL_PushEvent(&event));

        memset(&event, 0, sizeof(event));
        event.type = SDL_EVENT_QUIT;
        event.quit.type = SDL_EVENT_QUIT;
        assert(SDL_PushEvent(&event));
        quit_requested = 0;
        assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
               CDIN_FRONTEND_GUI_SDL_OK);
        assert(quit_requested != 0);
        assert(output_count_type(&stream, CDIN_FRONTEND_PROTOCOL_INPUT_TEXT) ==
               text_count);
    }

    cdin_frontend_gui_sdl_destroy(gui);
    gui = NULL;
    assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT);
    assert(gui == NULL);

    trace_step("complete");
    return 0;
}
