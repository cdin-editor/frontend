#include <cdin/frontend/ui/gui_sdl/gui_sdl.h>

int main(void)
{
    return cdin_frontend_gui_sdl_supported_capabilities() == 0u ? 0 : 0;
}
