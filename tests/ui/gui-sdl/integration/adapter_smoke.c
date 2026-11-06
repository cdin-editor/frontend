#include "cdin/frontend/ui/gui_sdl/gui_sdl.h"

#include <SDL3/SDL.h>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

int main(void)
{
    fake_stream stream;
    cdin_frontend_protocol_io io;
    cdin_frontend_client client;
    cdin_frontend_session session;
    cdin_frontend_gui_sdl_config config;
    cdin_frontend_gui_sdl *gui = NULL;
    uint8_t client_buffer[4096];
    cdin_frontend_protocol_message_view sent;
    SDL_Event event;
    int quit_requested = 0;

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
    session.accepted_capabilities =
        cdin_frontend_gui_sdl_supported_capabilities();
    (void)snprintf(session.view_id, sizeof(session.view_id), "%s", "smoke-view");

    memset(&config, 0, sizeof(config));
    config.session = &session;
    config.window_title = "CDIN GUI-SDL adapter smoke";
    config.width = 800u;
    config.height = 600u;
    config.visible_line_capacity = 16u;

    /* Force a late startup failure after video initialization and window
     * creation. The adapter must destroy partial resources and release the
     * video subsystem before a subsequent successful create. */
    assert(SDL_SetHint("SDL_RENDER_DRIVER", "cdin_nonexistent_renderer"));
    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_SDL_ERROR);
    assert(gui == NULL);
    assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);
    SDL_ResetHint("SDL_RENDER_DRIVER");

    assert(cdin_frontend_gui_sdl_create(&config, &gui) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(gui != NULL);
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
    assert(SDL_PushEvent(&event));
    assert(cdin_frontend_gui_sdl_pump_events(gui, &quit_requested) ==
           CDIN_FRONTEND_GUI_SDL_OK);
    assert(quit_requested == 0);
    assert(stream.output_length != 0u);
    assert(cdin_frontend_protocol_decode_frame(
        stream.output, stream.output_length, &sent, 4096u) ==
        CDIN_FRONTEND_PROTOCOL_OK);
    assert(sent.type == CDIN_FRONTEND_PROTOCOL_INPUT_MOUSE);

    cdin_frontend_gui_sdl_destroy(gui);
    assert((SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u);
    return 0;
}
