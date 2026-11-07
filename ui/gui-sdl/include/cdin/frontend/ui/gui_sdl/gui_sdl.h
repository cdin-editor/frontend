#ifndef CDIN_FRONTEND_UI_GUI_SDL_H
#define CDIN_FRONTEND_UI_GUI_SDL_H

#include "cdin/frontend/core/diagnostics/log.h"
#include "cdin/frontend/core/protocol/wire.h"
#include "cdin/frontend/core/session/session.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cdin_frontend_gui_sdl cdin_frontend_gui_sdl;

typedef enum cdin_frontend_gui_sdl_status {
    CDIN_FRONTEND_GUI_SDL_OK = 0,
    CDIN_FRONTEND_GUI_SDL_INVALID_ARGUMENT,
    CDIN_FRONTEND_GUI_SDL_INVALID_STATE,
    CDIN_FRONTEND_GUI_SDL_SDL_ERROR,
    CDIN_FRONTEND_GUI_SDL_PROTOCOL_ERROR,
    CDIN_FRONTEND_GUI_SDL_LIMIT_EXCEEDED,
    CDIN_FRONTEND_GUI_SDL_NO_MEMORY
} cdin_frontend_gui_sdl_status;

/* The Composition Root owns Protocol transport, client and Session lifetime.
 * It must complete HELLO/HELLO_ACK and ATTACH before creating the GUI adapter.
 * The GUI borrows the attached Session and never links Backend/Runtime/Kernel. */
typedef struct cdin_frontend_gui_sdl_config {
    cdin_frontend_session *session;
    cdin_frontend_logger *logger;
    const char *window_title;
    uint32_t width;
    uint32_t height;
    size_t visible_line_capacity;
} cdin_frontend_gui_sdl_config;

/* Capabilities this adapter actually implements. Call before HELLO and
 * pass the result to cdin_frontend_session_set_capabilities(). */
cdin_frontend_capability_mask cdin_frontend_gui_sdl_supported_capabilities(void);

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_create(
    const cdin_frontend_gui_sdl_config *config,
    cdin_frontend_gui_sdl **out_gui);

/* Pump native window/input events. The caller owns the outer readiness loop
 * and calls apply_message() when its Protocol transport produces a message. */
cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_pump_events(
    cdin_frontend_gui_sdl *gui,
    int *out_quit_requested);

/* Apply one already-decoded Core Protocol message. Snapshot/response flushes
 * are acknowledged through the borrowed Core Session. A version-pinned line
 * request is queued only after the snapshot ACK has succeeded. */
cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_apply_message(
    cdin_frontend_gui_sdl *gui,
    const cdin_frontend_protocol_message_view *message,
    int *out_committed);

/* Request a visible line window using the current committed document version. */
cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_request_lines(
    cdin_frontend_gui_sdl *gui,
    uint64_t first_line);

/* Request a version-pinned line window for a specific Runtime view. This is
 * useful to prepare independent caches for views before they become focused. */
cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_request_view_lines(
    cdin_frontend_gui_sdl *gui,
    const char *view_id,
    uint64_t first_line);

cdin_frontend_gui_sdl_status cdin_frontend_gui_sdl_render(
    cdin_frontend_gui_sdl *gui);

/* Does not destroy or detach the borrowed Session. The Composition Root owns
 * final protocol detach, transport shutdown and Session destruction. */
void cdin_frontend_gui_sdl_destroy(cdin_frontend_gui_sdl *gui);

#ifdef __cplusplus
}
#endif
#endif
