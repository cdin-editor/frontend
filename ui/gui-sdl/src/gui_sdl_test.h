#ifndef CDIN_FRONTEND_GUI_SDL_TEST_H
#define CDIN_FRONTEND_GUI_SDL_TEST_H

/* Test-only seam for deterministic pointer-capture lifecycle coverage. */
void cdin_frontend_gui_sdl_test_set_capture_available(int available);
struct cdin_frontend_gui_sdl;
int cdin_frontend_gui_sdl_test_ime_active(const struct cdin_frontend_gui_sdl *gui);
uint64_t cdin_frontend_gui_sdl_test_ime_id(const struct cdin_frontend_gui_sdl *gui);
size_t cdin_frontend_gui_sdl_test_ime_preedit_length(const struct cdin_frontend_gui_sdl *gui);
uint64_t cdin_frontend_gui_sdl_test_ime_cursor_byte(const struct cdin_frontend_gui_sdl *gui);
uint64_t cdin_frontend_gui_sdl_test_ime_selection_end_byte(const struct cdin_frontend_gui_sdl *gui);

#endif
