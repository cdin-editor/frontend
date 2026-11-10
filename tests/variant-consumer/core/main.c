#include <cdin/frontend/core/session/session.h>

int main(void)
{
    return cdin_frontend_session_request_command(NULL, "document.new", NULL)
        == CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT ? 0 : 1;
}
