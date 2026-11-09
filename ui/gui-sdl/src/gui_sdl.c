#include "cdin/frontend/ui/gui_sdl/gui_sdl.h"

#include "session/model.h"
#include "render/render.h"

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
    int pane_splitter_enabled;
    int quit_requested;
    int splitter_pointer_captured;
    int splitter_drag_active;
    int splitter_preview_active;
    int splitter_waiting_result;
    int splitter_result_applied;
    uint64_t next_splitter_request_id;
    uint64_t splitter_pointer_id;
    uint64_t splitter_request_id;
    uint64_t splitter_layout_revision;
    uint64_t splitter_result_revision;
    uint32_t splitter_axis_origin;
    uint32_t splitter_axis_extent;
    uint16_t splitter_preview_ratio_bp;
    char splitter_id[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES + 1u];
    cdin_frontend_gui_sdl_layout splitter_preview_layout;
    char title[256];
};

static cdin_frontend_gui_sdl_status map_model_status(
    cdin_frontend_gui_sdl_model_status status)
{
    switch (status) {
        case CDIN_FRONTEND_GUI_SDL_MODEL_OK: return CDIN_FRONTEND_GUI_SDL_OK;
        case CDIN_FRONTEND_GUI_SDL_MODEL_STALE: return CDIN_FRONTEND_GUI_SDL_OK;
        case CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT: return CDIN_FRONTEND_GUI_SDL_LIMIT_EXCEEDED;
        case CDIN_FRONTEND_GUI_SDL_MODEL_NO_MEMORY: return CDIN_FRONTEND_GUI_SDL_NO_MEMORY;
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

/* Session lifetime belongs to the Composition Root. A detached borrowed
 * Session must never be used to apply snapshots, render cached state or issue
 * new line requests. */
static int gui_session_attached(const cdin_frontend_gui_sdl *gui)
{
    return gui != NULL && gui->session != NULL &&
           gui->session->state == CDIN_FRONTEND_SESSION_ATTACHED;
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
    cdin_frontend_capability_mask supported =
        CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_MOUSE);
#if defined(CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER)
    supported |= CDIN_FRONTEND_CAP_BIT(CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE);
#endif
    return supported;
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
        config->session->view_id[0] == '\0' || !config->session->driver ||
        config->session->units == NULL ||
        strcmp(config->session->units, "pixels") != 0) {
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
    gui->next_splitter_request_id = 1u;
#if defined(CDIN_FRONTEND_GUI_SDL_ENABLE_PANE_SPLITTER)
    gui->pane_splitter_enabled = cdin_frontend_capability_has(
        cdin_frontend_session_accepted_capabilities(config->session),
        CDIN_FRONTEND_CAP_PANE_SPLITTER_RESIZE);
#else
    gui->pane_splitter_enabled = 0;
#endif
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
        cdin_frontend_gui_sdl_model_destroy(&gui->model);
        free(gui);
        return result;
    }
    gui->window = SDL_CreateWindow(gui->title, (int)gui->width, (int)gui->height,
                                   SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (gui->window == NULL) {
        log_sdl_error(gui, "SDL_CreateWindow");
        goto fail;
    }
    {
        int pixel_width = 0;
        int pixel_height = 0;
        if (!SDL_GetWindowSizeInPixels(gui->window, &pixel_width, &pixel_height) ||
            pixel_width <= 0 || pixel_height <= 0 ||
            (uint32_t)pixel_width > 32768u ||
            (uint32_t)pixel_height > 32768u) {
            log_sdl_error(gui, "SDL_GetWindowSizeInPixels during startup");
            goto fail;
        }
        /* GUI geometry, renderer coordinates and mouse hit-testing all use
         * drawable pixels, not the possibly lower-DPI window dimensions. */
        gui->width = (uint32_t)pixel_width;
        gui->height = (uint32_t)pixel_height;
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
    cdin_frontend_gui_sdl_model_destroy(&gui->model);
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

static void gui_content_viewport(
    const cdin_frontend_gui_sdl *gui,
    cdin_frontend_gui_sdl_rect *out_viewport)
{
    out_viewport->x = 0u;
    out_viewport->y = 42u;
    out_viewport->width = gui->width;
    out_viewport->height = gui->height > 72u ? gui->height - 72u : 1u;
}

static void cancel_splitter_interaction(cdin_frontend_gui_sdl *gui)
{
    if (gui == NULL) return;
    if (gui->splitter_pointer_captured) (void)SDL_CaptureMouse(false);
    gui->splitter_pointer_captured = 0;
    gui->splitter_drag_active = 0;
    gui->splitter_preview_active = 0;
    gui->splitter_waiting_result = 0;
    gui->splitter_result_applied = 0;
    gui->splitter_pointer_id = 0u;
    gui->splitter_request_id = 0u;
    gui->splitter_layout_revision = 0u;
    gui->splitter_result_revision = 0u;
    gui->splitter_axis_origin = 0u;
    gui->splitter_axis_extent = 0u;
    gui->splitter_preview_ratio_bp = 0u;
    gui->splitter_id[0] = '\0';
}

static size_t find_splitter_node(
    const cdin_frontend_gui_sdl_layout *layout, const char *split_id)
{
    size_t index;
    if (layout == NULL || split_id == NULL) return SIZE_MAX;
    for (index = 0u; index < layout->node_count; ++index) {
        if (layout->nodes[index].kind == CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT &&
            strcmp(layout->nodes[index].id, split_id) == 0) return index;
    }
    return SIZE_MAX;
}

static void update_splitter_preview(
    cdin_frontend_gui_sdl *gui, float x, float y)
{
    double offset;
    double scaled;
    double raw_coordinate;
    uint32_t ratio_bp;
    uint32_t coordinate;
    size_t node_index;
    if (gui == NULL || !gui->splitter_drag_active ||
        gui->splitter_axis_extent < 2u ||
        gui->model.layout.revision != gui->splitter_layout_revision) {
        if (gui != NULL && gui->splitter_drag_active) cancel_splitter_interaction(gui);
        return;
    }
    node_index = find_splitter_node(&gui->splitter_preview_layout, gui->splitter_id);
    if (node_index == SIZE_MAX) {
        cancel_splitter_interaction(gui);
        return;
    }
    raw_coordinate =
        gui->splitter_preview_layout.nodes[node_index].axis ==
            CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL ? (double)x : (double)y;
    if (raw_coordinate < 0.0) raw_coordinate = 0.0;
    if (raw_coordinate > 32768.0) raw_coordinate = 32768.0;
    coordinate = (uint32_t)raw_coordinate;
    offset = (double)coordinate - (double)gui->splitter_axis_origin;
    if (offset < 0.0) offset = 0.0;
    if (offset > (double)gui->splitter_axis_extent) {
        offset = (double)gui->splitter_axis_extent;
    }
    scaled = offset * 10000.0 / (double)gui->splitter_axis_extent + 0.5;
    ratio_bp = (uint32_t)scaled;
    if (ratio_bp < 1000u) ratio_bp = 1000u;
    if (ratio_bp > 8999u) ratio_bp = 8999u;
    gui->splitter_preview_layout.nodes[node_index].ratio_bp = (uint16_t)ratio_bp;
    gui->splitter_preview_ratio_bp = (uint16_t)ratio_bp;
    gui->splitter_preview_active = 1;
}

static int begin_splitter_interaction(
    cdin_frontend_gui_sdl *gui, const SDL_MouseButtonEvent *event)
{
    cdin_frontend_gui_sdl_rect viewport;
    cdin_frontend_gui_sdl_rect rects[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES];
    cdin_frontend_gui_sdl_geometry_status geometry_status;
    size_t split_index = SIZE_MAX;
    const cdin_frontend_gui_sdl_layout_node *split;
    uint32_t axis_origin;
    uint32_t axis_extent;
    if (gui == NULL || event == NULL || !gui->pane_splitter_enabled ||
        !cdin_frontend_session_is_driver(gui->session) ||
        gui->splitter_drag_active || gui->splitter_waiting_result ||
        gui->model.has_layout == 0 || gui->model.layout.has_root == 0 ||
        gui->model.layout.revision == 0u ||
        event->x < 0.0f || event->y < 0.0f ||
        event->x > 32768.0f || event->y > 32768.0f) return 0;
    gui_content_viewport(gui, &viewport);
    geometry_status = cdin_frontend_gui_sdl_geometry_splitter_hit_test(
        &gui->model.layout, &viewport, (uint32_t)event->x,
        (uint32_t)event->y, 4u, &split_index);
    if (geometry_status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK ||
        split_index >= gui->model.layout.node_count) return 0;
    geometry_status = cdin_frontend_gui_sdl_geometry_compute(
        &gui->model.layout, &viewport, rects,
        CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES);
    if (geometry_status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK) return 0;
    split = &gui->model.layout.nodes[split_index];
    if (split->axis == CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL) {
        axis_origin = rects[split_index].x;
        axis_extent = rects[split_index].width;
    } else {
        axis_origin = rects[split_index].y;
        axis_extent = rects[split_index].height;
    }
    if (axis_extent < 2u || split->id[0] == '\0') return 0;
    if (!SDL_CaptureMouse(true)) return 0;
    memcpy(&gui->splitter_preview_layout, &gui->model.layout,
           sizeof(gui->splitter_preview_layout));
    gui->splitter_pointer_captured = 1;
    gui->splitter_drag_active = 1;
    gui->splitter_preview_active = 1;
    gui->splitter_waiting_result = 0;
    gui->splitter_result_applied = 0;
    gui->splitter_pointer_id = (uint64_t)event->which;
    gui->splitter_layout_revision = gui->model.layout.revision;
    gui->splitter_axis_origin = axis_origin;
    gui->splitter_axis_extent = axis_extent;
    gui->splitter_preview_ratio_bp = split->ratio_bp;
    (void)snprintf(gui->splitter_id, sizeof(gui->splitter_id), "%s", split->id);
    update_splitter_preview(gui, event->x, event->y);
    return gui->splitter_drag_active;
}

static cdin_frontend_gui_sdl_status commit_splitter_interaction(
    cdin_frontend_gui_sdl *gui, float x, float y)
{
    cdin_frontend_protocol_status status;
    uint64_t request_id;
    if (gui == NULL || !gui->splitter_drag_active) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    update_splitter_preview(gui, x, y);
    if (!gui->splitter_drag_active) return CDIN_FRONTEND_GUI_SDL_OK;
    request_id = gui->next_splitter_request_id;
    if (request_id == 0u || request_id == UINT64_MAX) {
        cancel_splitter_interaction(gui);
        return CDIN_FRONTEND_GUI_SDL_LIMIT_EXCEEDED;
    }
    status = cdin_frontend_session_send_pane_splitter_resize(
        gui->session, request_id, gui->splitter_id,
        gui->splitter_layout_revision, gui->splitter_preview_ratio_bp);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        cancel_splitter_interaction(gui);
        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
    gui->next_splitter_request_id++;
    gui->splitter_request_id = request_id;
    gui->splitter_drag_active = 0;
    gui->splitter_waiting_result = 1;
    gui->splitter_result_applied = 0;
    if (gui->splitter_pointer_captured) (void)SDL_CaptureMouse(false);
    gui->splitter_pointer_captured = 0;
    return CDIN_FRONTEND_GUI_SDL_OK;
}

static cdin_frontend_gui_sdl_status apply_splitter_result(
    cdin_frontend_gui_sdl *gui,
    const cdin_frontend_protocol_message_view *message)
{
    char result_status[32];
    size_t result_status_length = 0u;
    uint64_t request_id = 0u;
    uint64_t layout_revision = 0u;
    if (gui == NULL || message == NULL || message->name_space == NULL ||
        strcmp(message->name_space, "pane") != 0 ||
        !gui->pane_splitter_enabled) return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "request_id",
            &request_id) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "layout_revision",
            &layout_revision) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_text(
            message->payload, message->payload_length, "status",
            result_status, sizeof(result_status), &result_status_length) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
        cancel_splitter_interaction(gui);
        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
    if (!gui->splitter_waiting_result ||
        request_id != gui->splitter_request_id) return CDIN_FRONTEND_GUI_SDL_OK;
    if (strcmp(result_status, "applied") != 0) {
        cancel_splitter_interaction(gui);
        return CDIN_FRONTEND_GUI_SDL_OK;
    }
    if (layout_revision <= gui->splitter_layout_revision) {
        cancel_splitter_interaction(gui);
        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
    gui->splitter_result_applied = 1;
    gui->splitter_result_revision = layout_revision;
    return CDIN_FRONTEND_GUI_SDL_OK;
}

/* Runtime resolves input.mouse.target against its stable Runtime View IDs.
 * Hit-test the pane geometry, then send the leaf's active_view ID—not the
 * internal PaneTree node ID—so Runtime remains the focus authority. */
static const char *mouse_target_for_position(
    cdin_frontend_gui_sdl *gui, float x, float y)
{
    cdin_frontend_gui_sdl_rect viewport;
    cdin_frontend_gui_sdl_geometry_status status;
    size_t leaf_index = SIZE_MAX;
    size_t view_index = SIZE_MAX;
    if (gui == NULL || x < 0.0f || y < 0.0f ||
        x > 32768.0f || y > 32768.0f) {
        return NULL;
    }
    if (gui->model.has_layout == 0) {
        const cdin_frontend_gui_sdl_view *focused =
            cdin_frontend_gui_sdl_model_focused_view(&gui->model);
        return focused != NULL ? focused->semantic.view_id : NULL;
    }
    if (gui->model.layout.has_root == 0) return NULL;
    gui_content_viewport(gui, &viewport);
    status = cdin_frontend_gui_sdl_geometry_hit_test(
        &gui->model.layout, &viewport, (uint32_t)x, (uint32_t)y,
        &leaf_index, &view_index);
    if (status != CDIN_FRONTEND_GUI_SDL_GEOMETRY_OK ||
        leaf_index >= gui->model.layout.node_count ||
        view_index >= gui->model.layout.view_count) {
        return NULL;
    }
    {
        const char *view_id = gui->model.layout.view_ids[view_index];
        const cdin_frontend_gui_sdl_view *view =
            cdin_frontend_gui_sdl_model_find_view_const(&gui->model, view_id);
        if (view == NULL || view->semantic.visible == 0) return NULL;
        return view->semantic.view_id;
    }
}

static cdin_frontend_gui_sdl_status convert_mouse_event_to_render_coordinates(
    cdin_frontend_gui_sdl *gui, SDL_Event *event)
{
    if (gui == NULL || gui->renderer == NULL || event == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    if (!SDL_ConvertEventToRenderCoordinates(gui->renderer, event)) {
        log_sdl_error(gui, "SDL_ConvertEventToRenderCoordinates");
        return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
    }
    return CDIN_FRONTEND_GUI_SDL_OK;
}

static cdin_frontend_gui_sdl_status send_mouse_event(
    cdin_frontend_gui_sdl *gui, const char *action, const char *button,
    const char *target, SDL_Keymod modifiers, float x, float y)
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
        target, logical_x, logical_y, has_position);
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
    if (!gui_session_attached(gui)) cancel_splitter_interaction(gui);
    while (SDL_PollEvent(&event)) {
        if (!gui_session_attached(gui)) {
            cancel_splitter_interaction(gui);
            /* Drop queued input for a detached Session, but preserve the
             * native close path so the Composition Root can still exit. */
            if (event.type == SDL_EVENT_QUIT) {
                gui->quit_requested = 1;
            } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                       event_matches_window(gui, event.window.windowID)) {
                gui->quit_requested = 1;
            }
            continue;
        }
        switch (event.type) {
            case SDL_EVENT_QUIT:
                cancel_splitter_interaction(gui);
                gui->quit_requested = 1;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (event_matches_window(gui, event.window.windowID)) {
                    cancel_splitter_interaction(gui);
                    gui->quit_requested = 1;
                }
                break;
            case SDL_EVENT_KEY_DOWN:
                if (!event_matches_window(gui, event.key.windowID)) break;
                if (event.key.key == SDLK_ESCAPE && !event.key.repeat &&
                    gui->splitter_drag_active) cancel_splitter_interaction(gui);
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
                const char *target;
                if (event.button.windowID != 0u &&
                    event.button.windowID != SDL_GetWindowID(gui->window)) break;
                if (convert_mouse_event_to_render_coordinates(gui, &event) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
                }
                if (gui->splitter_drag_active) {
                    if ((uint64_t)event.button.which != gui->splitter_pointer_id) break;
                    if (!event.button.down && event.button.button == SDL_BUTTON_LEFT) {
                        cdin_frontend_gui_sdl_status splitter_status =
                            commit_splitter_interaction(gui, event.button.x, event.button.y);
                        if (splitter_status != CDIN_FRONTEND_GUI_SDL_OK) {
                            return splitter_status;
                        }
                    }
                    break;
                }
                if (event.button.down && event.button.button == SDL_BUTTON_LEFT &&
                    begin_splitter_interaction(gui, &event.button)) break;
                target = event.button.down
                    ? mouse_target_for_position(gui, event.button.x, event.button.y)
                    : NULL;
                if (button != NULL &&
                    send_mouse_event(gui, event.button.down ? "press" : "release",
                                     button, target, SDL_GetModState(),
                                     event.button.x, event.button.y) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            }
            case SDL_EVENT_MOUSE_MOTION:
                if (event.motion.windowID != 0u &&
                    event.motion.windowID != SDL_GetWindowID(gui->window)) break;
                if (convert_mouse_event_to_render_coordinates(gui, &event) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
                }
                if (gui->splitter_drag_active) {
                    if ((uint64_t)event.motion.which == gui->splitter_pointer_id) {
                        update_splitter_preview(gui, event.motion.x, event.motion.y);
                    }
                    break;
                }
                if (send_mouse_event(gui, "move", "none", NULL,
                                     SDL_GetModState(), event.motion.x,
                                     event.motion.y) != CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL: {
                float vertical;
                const char *button;
                const char *target;
                if (event.wheel.windowID != 0u &&
                    event.wheel.windowID != SDL_GetWindowID(gui->window)) break;
                if (convert_mouse_event_to_render_coordinates(gui, &event) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
                }
                vertical = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                    ? -event.wheel.y : event.wheel.y;
                button = vertical > 0.0f ? "wheel_up" :
                         vertical < 0.0f ? "wheel_down" : "none";
                target = mouse_target_for_position(
                    gui, event.wheel.mouse_x, event.wheel.mouse_y);
                if (vertical != 0.0f &&
                    send_mouse_event(gui, "scroll", button, target,
                                     SDL_GetModState(), event.wheel.mouse_x,
                                     event.wheel.mouse_y) !=
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
                    /* SDL invalidates the renderer on device loss. Recreate
                     * it immediately; the current renderer owns no persistent
                     * textures, so the next render() rebuilds the full frame. */
                    if (recreate_renderer(gui) != CDIN_FRONTEND_GUI_SDL_OK) {
                        log_sdl_error(gui, "SDL renderer recovery after device loss");
                        return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
                    }
                }
                break;
            case SDL_EVENT_RENDER_TARGETS_RESET:
                /* The current renderer owns no persistent textures or render
                 * targets; the next render() rebuilds the complete frame. */
                break;
            case SDL_EVENT_FINGER_CANCELED:
            case SDL_EVENT_MOUSE_REMOVED:
            case SDL_EVENT_WINDOW_HIDDEN:
                cancel_splitter_interaction(gui);
                break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            case SDL_EVENT_WINDOW_FOCUS_LOST: {
                if (!event_matches_window(gui, event.window.windowID)) break;
                int focused = event.type == SDL_EVENT_WINDOW_FOCUS_GAINED;
                if (!focused) cancel_splitter_interaction(gui);
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
    cdin_frontend_gui_sdl *gui,
    const char *view_id,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version)
{
    cdin_frontend_protocol_status status;
    cdin_frontend_gui_sdl_model_status model_status;
    const cdin_frontend_gui_sdl_view *view;
    if (gui == NULL || view_id == NULL || view_id[0] == '\0' || version == 0u) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    if (!gui_session_attached(gui)) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    view = cdin_frontend_gui_sdl_model_find_view_const(&gui->model, view_id);
    if (view == NULL || view->semantic.version != version) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    model_status = cdin_frontend_gui_sdl_model_expect_lines(
        &gui->model, view_id, first_line, line_count, version);
    if (model_status != CDIN_FRONTEND_GUI_SDL_MODEL_OK) {
        return map_model_status(model_status);
    }
    status = cdin_frontend_session_request_lines(
        gui->session, view_id, first_line, line_count, version);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        cdin_frontend_gui_sdl_model_cancel_line_request(&gui->model, view_id);
        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
    return CDIN_FRONTEND_GUI_SDL_OK;
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_request_view_lines(
    cdin_frontend_gui_sdl *gui,
    const char *view_id,
    uint64_t first_line)
{
    const cdin_frontend_gui_sdl_view *view;
    if (gui == NULL || view_id == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    view = cdin_frontend_gui_sdl_model_find_view_const(&gui->model, view_id);
    if (view == NULL || view->semantic.version == 0u) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    return request_line_window(
        gui, view_id, first_line,
        (uint64_t)gui->model.visible_line_capacity, view->semantic.version);
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_request_lines(
    cdin_frontend_gui_sdl *gui, uint64_t first_line)
{
    const cdin_frontend_gui_sdl_view *view;
    if (gui == NULL) return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    view = cdin_frontend_gui_sdl_model_focused_view(&gui->model);
    if (view == NULL || view->semantic.version == 0u) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    return cdin_frontend_gui_sdl_request_view_lines(
        gui, view->semantic.view_id, first_line);
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
    if (!gui_session_attached(gui)) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    if (message->type == CDIN_FRONTEND_PROTOCOL_PANE_SPLITTER_RESULT) {
        return apply_splitter_result(gui, message);
    }
    if (message->name_space == NULL || strcmp(message->name_space, "core") != 0) {
        return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
    }
    if (message->type == CDIN_FRONTEND_PROTOCOL_DRIVER_ROLE_CHANGED &&
        !cdin_frontend_session_is_driver(gui->session)) {
        cancel_splitter_interaction(gui);
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
        if (gui->splitter_drag_active &&
            gui->model.layout.revision != gui->splitter_layout_revision) {
            cancel_splitter_interaction(gui);
        }
        if (gui->splitter_waiting_result) {
            if (gui->splitter_result_applied &&
                gui->model.layout.revision == gui->splitter_result_revision) {
                size_t split_index = find_splitter_node(
                    &gui->model.layout, gui->splitter_id);
                if (split_index == SIZE_MAX ||
                    gui->model.layout.nodes[split_index].ratio_bp !=
                        gui->splitter_preview_ratio_bp) {
                    cancel_splitter_interaction(gui);
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
                cancel_splitter_interaction(gui);
            } else if (gui->model.layout.revision != gui->splitter_layout_revision) {
                cancel_splitter_interaction(gui);
            }
        }
        {
            size_t request_index;
            for (request_index = 0u; request_index < action.request_count;
                 ++request_index) {
                const cdin_frontend_gui_sdl_view_request *request =
                    &action.requests[request_index];
                if (request_line_window(
                        gui, request->view_id, request->first_line,
                        request->line_count, request->version) !=
                    CDIN_FRONTEND_GUI_SDL_OK) {
                    return CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR;
                }
            }
        }
    }
    return CDIN_FRONTEND_GUI_SDL_OK;
}

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_render(
    cdin_frontend_gui_sdl *gui)
{
    cdin_frontend_gui_sdl_rect content;
    cdin_frontend_gui_sdl_render_stats stats;
    cdin_frontend_gui_sdl_render_status render_status;
    if (gui == NULL || gui->renderer == NULL) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    if (!gui_session_attached(gui)) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
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

    gui_content_viewport(gui, &content);
    render_status = cdin_frontend_gui_sdl_render_panes_with_layout(
        gui->renderer, &gui->model,
        gui->splitter_preview_active ? &gui->splitter_preview_layout : NULL,
        &content, &stats);
    if (render_status == CDIN_FRONTEND_GUI_SDL_RENDER_SDL_ERROR) {
        log_sdl_error(gui, "render PaneTree");
        return CDIN_FRONTEND_GUI_SDL_SDL_ERROR;
    }
    if (render_status == CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_LAYOUT) {
        if (gui->logger != NULL) {
            CDIN_FRONTEND_ERROR(gui->logger, "GUI-SDL",
                                "committed PaneTree cannot be rendered");
        }
        return CDIN_FRONTEND_GUI_SDL_INVALID_STATE;
    }
    if (render_status != CDIN_FRONTEND_GUI_SDL_RENDER_OK) {
        return CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT;
    }
    (void)stats;
    (void)SDL_RenderPresent(gui->renderer);
    return CDIN_FRONTEND_GUI_SDL_OK;
}

void cdin_frontend_gui_sdl_destroy(cdin_frontend_gui_sdl *gui)
{
    if (gui == NULL) return;
    cancel_splitter_interaction(gui);
    if (gui->window != NULL) SDL_StopTextInput(gui->window);
    if (gui->renderer != NULL) SDL_DestroyRenderer(gui->renderer);
    if (gui->window != NULL) SDL_DestroyWindow(gui->window);
    if (gui->owns_video) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    cdin_frontend_gui_sdl_model_destroy(&gui->model);
    free(gui);
}
