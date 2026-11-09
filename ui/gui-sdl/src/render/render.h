#ifndef CDIN_FRONTEND_GUI_SDL_RENDER_H
#define CDIN_FRONTEND_GUI_SDL_RENDER_H

#include "../session/model.h"
#include "../layout/geometry.h"

#include <SDL3/SDL.h>

#include <stddef.h>

typedef enum cdin_frontend_gui_sdl_render_status {
    CDIN_FRONTEND_GUI_SDL_RENDER_OK = 0,
    CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_ARGUMENT,
    CDIN_FRONTEND_GUI_SDL_RENDER_INVALID_LAYOUT,
    CDIN_FRONTEND_GUI_SDL_RENDER_SDL_ERROR
} cdin_frontend_gui_sdl_render_status;

typedef struct cdin_frontend_gui_sdl_render_stats {
    size_t pane_count;
    size_t rendered_view_count;
} cdin_frontend_gui_sdl_render_stats;

/* Render the editor content area from the committed PaneTree. Each leaf is
 * clipped to its deterministic pixel rectangle; only that leaf's active
 * Runtime view is drawn. The function does not clear or present the target. */
/* Render the committed model using an optional local-only preview layout.
 * The override is never written back to the model or sent to Runtime. */
cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes_with_layout(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_layout *layout_override,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *out_stats);

cdin_frontend_gui_sdl_render_status cdin_frontend_gui_sdl_render_panes(
    SDL_Renderer *renderer,
    const cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_gui_sdl_rect *viewport,
    cdin_frontend_gui_sdl_render_stats *out_stats);

#endif
