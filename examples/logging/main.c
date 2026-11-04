#include "cdin/frontend/core/diagnostics/log.h"

int main(void)
{
    cdin_frontend_logger *logger = NULL;
    if (cdin_frontend_logger_create(MMLLOG_LEVEL_TRACE, &logger) != MMLLOG_OK) return 1;

    CDIN_FRONTEND_INFO(logger, "CORE", "Frontend Core diagnostics are ready");
    CDIN_FRONTEND_DEBUG(logger, "PROTOCOL", "protocol client example started");

    cdin_frontend_logger_destroy(logger);
    return 0;
}
