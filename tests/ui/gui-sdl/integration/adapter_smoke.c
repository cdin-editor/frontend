#include "cdin/frontend/ui/gui_sdl/gui_sdl.h"

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
    uint8_t output[8192];
    size_t output_length;
} fake_stream;

static ptrdiff_t fake_read(void *opaque, uint8_t *buffer, size_t length)
{
    (void)opaque;
    (void)buffer;
    (void)length;
    return 0;
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
            size_t action_length = 0u;
            size_t button_length = 0u;
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
                cdin_frontend_protocol_payload_get_raw(
                    message.payload, message.payload_length, "position",
                    &position, &position_length) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_uint(
                    position, position_length, "x", &x) == CDIN_FRONTEND_PROTOCOL_OK &&
                cdin_frontend_protocol_payload_get_uint(
                    position, position_length, "y", &y) == CDIN_FRONTEND_PROTOCOL_OK) {
                return action_length == 5u && strcmp(action, "press") == 0 &&
                       button_length == 4u && strcmp(button, "left") == 0 &&
                       x == 12u && y == 21u;
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
        cdin_frontend_gui_sdl_supported_capabilities();
    (void)snprintf(session.view_id, sizeof(session.view_id), "%s", "smoke-view");

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

    /* SDL_PushEvent supplies a real SDL input event through the same adapter
     * pump as a native mouse. The public Core Session serializes it as
     * input.mouse, without any Runtime/keymap interpretation in the GUI. */
    memset(&event, 0, sizeof(event));
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.down = true;
    event.button.x = 12.0f;
    event.button.y = 21.0f;
    trace_step("push mouse event");
    assert(SDL_PushEvent(&event));
    trace_step("pump mouse event");
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(quit_requested == 0);
    assert(stream.output_length != 0u);
    assert(output_contains_expected_mouse(&stream));
    trace_step("mouse protocol frame verified");

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
    trace_step("complete");
    return 0;
}
