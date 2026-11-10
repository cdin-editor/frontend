#include <cdin/frontend/core/session/session.h>
#include <cdin/frontend/ui/gui_sdl/gui_sdl.h>
#include <cdin/frontend/ui/tui/event_loop.h>

int main(void)
{
    if (cdin_frontend_session_request_command(NULL, "document.new", NULL)
        != CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT) return 1;
    if (cdin_frontend_gui_sdl_supported_capabilities() == 0u) return 2;
    if (cdin_frontend_tui_event_loop_run(NULL) != CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT) return 3;
    return 0;
}
