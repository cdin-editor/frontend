#include "cdin/frontend/ui/gui_sdl/gui_sdl.h"

#include "session/model.h"

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cdin_frontend_gui_sdl {
    cdin_frontend_session *session;
    cdin_frontend_logger *logger;
    cdin_frontend_gui_sdl_model model;
    SDL_Window *window;
    SDL_Renderer *renderer;
    uint32_t width;
    uint32_t height;
    int owns_video;
    int mouse_enabled;
    int quit_requested;
    char title[256];
};

static cdin_frontend_gui_sdl_status map_model_status(
    cdin_frontend_gui_sdl_model_status status)
{
    switch (status) {
        case CDIN_FRONTEND_GUI_SDL_MODEL_OK: return CDIN_FRONTEND_GUI_SDL_OK;
        case CDIN_FRONTEND_GUI_SDL_MODEL_STALE: return CDIN_FRONTEND_GUI_SDL_OK;
        case CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT: return CDIN_FRONTEND_GUI_SDL_LIMIT_EXCEEDED;
        case CDIN_FRONTEND_GUI_SDL_MODEL_INVALID:
        default: return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
}

static void log_sdl_error(cdin_frontend_gui_sdl *gui, const char *operation)
{
    if (gui != NULL && gui->logger != NULL) {
        CDIN_FRONTEND_ERROR(gui->logger, "GUI-SDL", "%s failed: %s",
                            operation, SDL_GetError());
    }
}

static int renderer_event_matches_window(
    const cdin_frontend_gui_sdl *gui, SDL_WindowID window_id)
{
    return gui != NULL && gui->window != NULL &&
           (window_id == 0u || window_id == SDL_GetWindowID(gui->window));
}

static int event_matches_window(
    const cdin_frontend_gui_sdl *gui, SDL_WindowID window_id)
{
    return gui != NULL && gui->window != NULL &&
           (window_id == 0u || window_id == SDL_GetWindowID(gui->window));
}

static cdin_frontend_gui_sdl_status recreate_renderer(cdin_frontend_gui_sdl *gui)
{
    if (gui == NULL || gui->window == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    if (gui->renderer != NULL) {
        SDL_DestroyRenderer(gui->renderer);
        gui->renderer = NULL;
    }
    gui->renderer = SDL_CreateRenderer(gui->window, NULL);
    if (gui->renderer == NULL) {
        log_sdl_error(gui, "SDL_CreateRenderer after device reset");
        return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
    }
    return CDIN_FRONTEND_GUI_SDL_OK;
}

cdin_frontend_capability_mask cdin_frontend_gui_sdl_supported_capabilities(void)
{
    return CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE);
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_create(
    const cdin_frontend_gui_sdl_config *config,
    cdin_frontend_gui_sdl **out_gui)
{
    cdin_frontend_gui_sdl *gui;
    size_t title_length;
    cdin_frontend_gui_sdl_status result = CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
    if (out_gui != NULL) *out_gui = NULL;
    if (config == NULL || out_gui == NULL || config->session == NULL ||
        config->width == 0u || config->height == 0u ||
        config->width > 32768u || config->height > 32768u ||
        config->session->state != CDIN_FRONTEND_SESSION_ATTACHED ||
        config->session->view_id[0] == '\0' || !config->session->driver) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    gui = (cdin_frontend_gui_sdl *)calloc(1u, sizeof(*gui));
    if (gui == NULL) return CDIN_FRONTEND_GUI_SDL_NO_MEMORY;
    gui->session = config->session;
    gui->logger = config->logger;
    gui->width = config->width;
    gui->height = config->height;
    gui->mouse_enabled = cdin_frontend_capability_has(
        cdin_frontend_session_accepted_capabilities(config->session),
        CDIN_FRONTEND_CAP_MOUSE);
    gui->owns_video = (SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO) == 0u;
    if (config->window_title == NULL) {
        gui->title[0] = '\0';
    } else {
        title_length = strlen(config->window_title);
        if (title_length == 0u || title_length >= sizeof(gui->title)) {
            free(gui);
            return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
        }
        memcpy(gui->title, config->window_title, title_length + 1u);
    }
    if (gui->title[0] == '\0') {
        (void)snprintf(gui->title, sizeof(gui->title), "CDIN — GUI-SDL");
    }
    if (cdin_frontend_gui_sdl_model_init(
            &gui->model, "",
            config->visible_line_capacity == 0u ? 32u : config->visible_line_capacity) !=
        CDIN_FRONTEND_GUI_SDL_MODEL_OK) {
        free(gui);
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    if (gui->owns_video && !SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        log_sdl_error(gui, "SDL_InitSubSystem(video)");
        free(gui);
        return result;
    }
    gui->window = SDL_CreateWindow(gui->title, (int)gui->width, (int)gui->height,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (gui->window == NULL) {
        log_sdl_error(gui, "SDL_CreateWindow");
        goto fail;
    }
    gui->renderer = SDL_CreateRenderer(gui->window, NULL);
    if (gui->renderer == NULL) {
        log_sdl_error(gui, "SDL_CreateRenderer");
        goto fail;
    }
    SDL_StartTextInput(gui->window);
    *out_gui = gui;
    return CDIN_FRONTEND_GUI_SDL_OK;

fail:
    if (gui->renderer != NULL) SDL_DestroyRenderer(gui->renderer);
    if (gui->window != NULL) SDL_DestroyWindow(gui->window);
    if (gui->owns_video) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(gui);
    return result;
}

static uint32_t input_modifiers(SDL_Keymod modifiers)
{
    uint32_t value = 0u;
    if ((modifiers & SDL_KMOD_CTRL) != 0) value |= CDIN_FRONTEND_MOD_CTRL;
    if ((modifiers & SDL_KMOD_ALT) != 0) value |= CDIN_FRONTEND_MOD_ALT;
    if ((modifiers & SDL_KMOD_SHIFT) != 0) value |= CDIN_FRONTEND_MOD_SHIFT;
    if ((modifiers & SDL_KMOD_MODE) != 0) value |= CDIN_FRONTEND_MOD_ALTGR;
    return value;
}

static const char *key_name(SDL_Keycode key, char output[24])
{
    switch (key) {
        case SDLK_RETURN: return "enter";
        case SDLK_KP_ENTER: return "enter";
        case SDLK_ESCAPE: return "escape";
        case SDLK_TAB: return "tab";
        case SDLK_BACKSPACE: return "backspace";
        case SDLK_DELETE: return "delete";
        case SDLK_HOME: return "home";
        case SDLK_END: return "end";
        case SDLK_PAGEUP: return "page_up";
        case SDLK_PAGEDOWN: return "page_down";
        case SDLK_LEFT: return "left";
        case SDLK_RIGHT: return "right";
        case SDLK_UP: return "up";
        case SDLK_DOWN: return "down";
        case SDLK_INSERT: return "insert";
        case SDLK_SPACE: return "space";
        default: break;
    }
    if (key >= SDLK_F1 && key <= SDLK_F12) {
        (void)snprintf(output, 24u, "f%u", (unsigned)(key - SDLK_F1 + 1));
        return output;
    }
    if (key >= SDLK_A && key <= SDLK_Z) {
        output[0] = (char)('a' + (key - SDLK_A));
        output[1] = '\0';
        return output;
    }
    if (key >= SDLK_0 && key <= SDLK_9) {
        output[0] = (char)('0' + (key - SDLK_0));
        output[1] = '\0';
        return output;
    }
    return NULL;
}

static cdin_frontend_gui_sdl_status send_key_event(
    cdin_frontend_gui_sdl *gui, const SDL_KeyboardEvent *event,
    const char *action)
{
    char key_buffer[24];
    const char *name;
    cdin_frontend_protocol_status status;
    uint32_t modifiers = input_modifiers(event->mod);
    if (action == NULL) return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    name = key_name(event->key, key_buffer);
    if (name == NULL) return CDIN_FRONTEND_GUI_SDL_OK;

    if (strcmp(action, "press") == 0 &&
        (modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u &&
        (modifiers & CDIN_FRONTEND_MOD_SHIFT) != 0u &&
        event->key == SDLK_V) {
        char *clipboard = SDL_GetClipboardText();
        if (clipboard != NULL) {
            size_t length = strlen(clipboard);
            status = cdin_frontend_session_send_paste(
                gui->session, (const uint8_t *)clipboard, length);
            SDL_free(clipboard);
            return status == CDIN_FRONTEND_PROTOCOL_OK
                ? CDIN_FRONTEND_GUI_SDL_OK : CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
        }
        return CDIN_FRONTEND_GUI_SDL_OK;
    }

    /* Printable unmodified keys are represented by SDL_TEXT_INPUT. Emitting
     * both input.key and input.text for them would execute/insert twice. */
    if ((event->key >= SDLK_A && event->key <= SDLK_Z) ||
        (event->key >= SDLK_0 && event->key <= SDLK_9)) {
        if ((modifiers & (CDIN_FRONTEND_MOD_CTRL | CDIN_FRONTEND_MOD_ALT |
                          CDIN_FRONTEND_MOD_ALTGR)) == 0u) {
            return CDIN_FRONTEND_GUI_SDL_OK;
        }
    }
    status = cdin_frontend_session_send_key(
        gui->session, name, modifiers, action);
    if (status == CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT) {
        return CDIN_FRONTEND_GUI_SDL_OK;
    }
    return status == CDIN_FRONTEND_PROTOCOL_OK
        ? CDIN_FRONTEND_GUI_SDL_OK : CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
}

static const char *mouse_button_name(uint8_t button)
{
    switch (button) {
        case SDL_BUTTON_LEFT: return "left";
        case SDL_BUTTON_MIDDLE: return "middle";
        case SDL_BUTTON_RIGHT: return "right";
        case SDL_BUTTON_X1: return "x1";
        case SDL_BUTTON_X2: return "x2";
        default: return NULL;
    }
}

static cdin_frontend_gui_sdl_status send_mouse_event(
    cdin_frontend_gui_sdl *gui, const char *action, const char *button,
    SDL_Keymod modifiers, float x, float y)
{
    cdin_frontend_protocol_status status;
    uint32_t logical_x = 0u;
    uint32_t logical_y = 0u;
    int has_position = x >= 0.0f && y >= 0.0f &&
        x <= 32768.0f && y <= 32768.0f;
    if (gui == NULL || action == NULL || button == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    if (!gui->mouse_enabled) return CDIN_FRONTEND_GUI_SDL_OK;
    if (has_position) {
        logical_x = (uint32_t)x;
        logical_y = (uint32_t)y;
    }
    status = cdin_frontend_session_send_mouse(
        gui->session, action, button, input_modifiers(modifiers),
        NULL, logical_x, logical_y, has_position);
    if (status == CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
        /* The Composition Root may choose not to negotiate optional mouse
         * support. The adapter then remains keyboard/text-only. */
        return CDIN_FRONTEND_GUI_SDL_OK;
    }
    return status == CDIN_FRONTEND_PROTOCOL_OK
        ? CDIN_FRONTEND_GUI_SDL_OK : CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_pump_events(
    cdin_frontend_gui_sdl *gui, int *out_quit_requested)
{
    SDL_Event event;
    if (out_quit_requested != NULL) *out_quit_requested = 0;
    if (gui == NULL || out_quit_requested == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                gui->quit_requested = 1;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event_matches_window(gui, event.window.windowID)) {
                    gui->quit_requested = 1;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!event_matches_window(gui, event.key.windowID)) break;
                if (send_key_event(gui, &event.key,
                                   event.key.repeat ? "repeat" : "press") !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            case SDL_EVENT_KEY_UP:
                if (!event_matches_window(gui, event.key.windowID)) break;
                if (send_key_event(gui, &event.key, "release") !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP: {
                const char *button = mouse_button_name(event.button.button);
                if (event.button.windowID != 0u &&
                    event.button.windowID != SDL_GetWindowID(gui->window)) break;
                if (button != NULL &&
                    send_mouse_event(gui, event.button.down ? "press" : "release",
                                     button, SDL_GetModState(),
                                     event.button.x, event.button.y) !=
                        CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID != 0u &&
                    event.motion.windowID != SDL_GetWindowID(gui->window)) break;
                if (send_mouse_event(gui, "move", "none", SDL_GetModState(),
                                     event.motion.x, event.motion.y) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                if (event.wheel.windowID != 0u &&
                    event.wheel.windowID != SDL_GetWindowID(gui->window)) break;
                float vertical = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                    ? -event.wheel.y : event.wheel.y;
                const char *button = vertical > 0.0f ? "wheel_up" :
                                     vertical < 0.0f ? "wheel_down" : "none";
                if (vertical != 0.0f &&
                    send_mouse_event(gui, "scroll", button, SDL_GetModState(),
                                     event.wheel.mouse_x, event.wheel.mouse_y) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            }
            case SDL_EVENT_TEXT_INPUT:
                if (!event_matches_window(gui, event.text.windowID)) break;
                if (event.text.text != NULL && event.text.text[0] != '\0') {
                    cdin_frontend_protocol_status status =
                        cdin_frontend_session_send_text(
                            gui->session,
                            (const uint8_t *)event.text.text,
                            SDL_strlen(event.text.text));
                    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
                        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                    }
                }
                break;
            case SDL_EVENT_RENDER_DEVICE_RESET:
                if (renderer_event_matches_window(gui, event.render.windowID) &&
                    recreate_renderer(gui) != CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
                }
                break;
            case SDL_EVENT_RENDER_DEVICE_LOST:
                if (renderer_event_matches_window(gui, event.render.windowID)) {
                    if (gui->renderer != NULL) {
                        SDL_DestroyRenderer(gui->renderer);
                        gui->renderer = NULL;
                    }
                    log_sdl_error(gui, "SDL renderer device lost");
                    return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
                }
                break;
            case SDL_EVENT_RENDER_TARGETS_RESET:
                /* The current renderer owns no persistent textures or render
                 * targets; the next render() rebuilds the complete frame. */
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                if (!event_matches_window(gui, event.window.windowID)) break;
                int focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
                if (cdin_frontend_session_focus(gui->session, focused) !=
                    CDIN_FRONTEND_PROTOCOL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            }
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_RESIZED: {
                if (!event_matches_window(gui, event.window.windowID)) break;
                int width = 0;
                int height = 0;
                if (!SDL_GetWindowSizeInPixels(gui->window, &width, &height) ||
                    width <= 0 || height <= 0) break;
                if ((uint32_t)width <= 32768u && (uint32_t)height <= 32768u) {
                    gui->width = (uint32_t)width;
                    gui->height = (uint32_t)height;
                    if (cdin_frontend_session_resize(
                            gui->session, gui->width, gui->height) !=
                        CDIN_FRONTEND_PROTOCOL_OK) {
                        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                    }
                }
                break;
            }
            default:
                break;
        }
    }
    *out_quit_requested = gui->quit_requested;
    return CDIN_FRONTEND_GUI_SDL_OK;
}

static cdin_frontend_gui_sdl_status request_line_window(
    cdin_frontend_gui_sdl *gui, uint64_t first_line,
    uint64_t line_count, uint64_t version)
{
    cdin_frontend_protocol_status status;
    if (gui == NULL || version == 0u || version != gui->model.version) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    if (cdin_frontend_gui_sdl_model_expect_lines(
            &gui->model, first_line, line_count, version) !=
        CDIN_FRONTEND_GUI_SDL_MODEL_OK) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    status = cdin_frontend_session_request_lines(
        gui->session, gui->model.view_id, first_line, line_count, version);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        gui->model.has_line_request = 0;
        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
    return CDIN_FRONTEND_GUI_SDL_OK;
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_request_lines(
    cdin_frontend_gui_sdl *gui, uint64_t first_line)
{
    if (gui == NULL || gui->model.version == 0u) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    return request_line_window(
        gui, first_line, (uint64_t)gui->model.visible_line_capacity,
        gui->model.version);
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_apply_message(
    cdin_frontend_gui_sdl *gui,
    const cdin_frontend_protocol_message_view *message,
    int *out_committed)
{
    cdin_frontend_gui_sdl_model_action action;
    cdin_frontend_gui_sdl_model_status model_status;
    cdin_frontend_protocol_status protocol_status;
    if (out_committed != NULL) *out_committed = 0;
    if (gui == NULL || message == NULL || out_committed == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    model_status = cdin_frontend_gui_sdl_model_apply(
        &gui->model, message, &action);
    if (model_status == CDIN_FRONTEND_GUI_SDL_MODEL_STALE) {
        /* A stale snapshot still owns an ACK sequence. Release its Backend
         * queue entry even though none of its semantic state was committed. */
        if (message->type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH) {
            protocol_status = cdin_frontend_session_ack(
                gui->session, action.acknowledge_sequence);
            if (protocol_status != CDIN_FRONTEND_PROTOCOL_OK) {
                return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
            }
            *out_committed = 1;
        }
        return CDIN_FRONTEND_GUI_SDL_OK;
    }
    if (model_status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) {
        return map_model_status(model_status);
    }
    if (message->type == CDIN_FRONTEND_PROTOCOL_UI_FLUSH) {
        protocol_status = cdin_frontend_session_ack(
            gui->session, action.acknowledge_sequence);
        if (protocol_status != CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
        }
        *out_committed = 1;
        if (action.request_lines) {
            if (request_line_window(
                    gui, action.first_line, action.line_count,
                    action.version) != CDIN_FRONTEND_GUI_SDL_OK) {
                return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
            }
        }
    }
    return CDIN_FRONTEND_GUI_SDL_OK;
}

static void draw_text_ascii(SDL_Renderer *renderer, float x, float y,
                            const uint8_t *bytes, size_t length,
                            size_t capacity)
{
    char text[512];
    size_t amount = length < capacity ? length : capacity;
    size_t index;
    if (amount >= sizeof(text)) amount = sizeof(text) - 1u;
    for (index = 0u; index < amount; ++index) {
        uint8_t value = bytes[index];
        text[index] = value >= 0x20u && value < 0x7fu ? (char)value : '?';
    }
    text[amount] = '\0';
    (void)SDL_RenderDebugText(renderer, x, y, text);
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_render(
    cdin_frontend_gui_sdl *gui)
{
    size_t index;
    float y = 56.0f;
    if (gui == NULL || gui->renderer == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    (void)SDL_SetRenderDrawColor(gui->renderer, 22u, 25u, 31u, 255u);
    (void)SDL_RenderClear(gui->renderer);
    (void)SDL_SetRenderDrawColor(gui->renderer, 43u, 48u, 58u, 255u);
    {
        SDL_FRect header = { 0.0f, 0.0f, (float)gui->width, 42.0f };
        SDL_FRect footer = { 0.0f, (float)gui->height - 30.0f,
                             (float)gui->width, 30.0f };
        (void)SDL_RenderFillRect(gui->renderer, &header);
        (void)SDL_RenderFillRect(gui->renderer, &footer);
    }
    (void)SDL_SetRenderDrawColor(gui->renderer, 230u, 235u, 242u, 255u);
    (void)SDL_RenderDebugText(gui->renderer, 16.0f, 14.0f, gui->title);
    (void)SDL_RenderDebugText(gui->renderer, 16.0f,
        (float)gui->height - 20.0f,
        gui->model.status[0] != '\0' ? gui->model.status :
        (gui->model.mode[0] != '\0' ? gui->model.mode : "Connected"));
    for (index = 0u; index < gui->model.visible.count; ++index) {
        char line_number[24];
        (void)snprintf(line_number, sizeof(line_number), "%5llu ",
            (unsigned long long)(gui->model.visible.first_line + (uint64_t)index + 1u));
        (void)SDL_RenderDebugText(gui->renderer, 12.0f, y, line_number);
        draw_text_ascii(gui->renderer, 72.0f, y,
            gui->model.visible.lines[index].bytes,
            gui->model.visible.lines[index].length, 430u);
        y += 20.0f;
        if (y > (float)gui->height - 44.0f) break;
    }
    (void)SDL_RenderPresent(gui->renderer);
    return CDIN_FRONTEND_GUI_SDL_OK;
}

void cdin_frontend_gui_sdl_destroy(cdin_frontend_gui_sdl *gui)
{
    if (gui == NULL) return;
    if (gui->window != NULL) SDL_StopTextInput(gui->window);
    if (gui->renderer != NULL) SDL_DestroyRenderer(gui->renderer);
    if (gui->window != NULL) SDL_DestroyWindow(gui->window);
    if (gui->owns_video) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    free(gui);
}
