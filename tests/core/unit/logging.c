#include "cdin/frontend/core/diagnostics/log.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct capture_state {
    int calls;
    int32_t level;
    size_t length;
    char line[1024];
} capture_state;

static void capture(void *opaque, int32_t level, const char *line, size_t length)
{
    capture_state *state = (capture_state *)opaque;
    size_t amount = length < sizeof(state->line) - 1u ? length : sizeof(state->line) - 1u;
    state->calls++;
    state->level = level;
    state->length = amount;
    memcpy(state->line, line, amount);
    state->line[amount] = '\0';
}

int main(void)
{
    cdin_frontend_logger *logger = NULL;
    capture_state captured;
    memset(&captured, 0, sizeof(captured));

    assert(cdin_frontend_logger_create(MMLLOG_LEVEL_DEBUG, &logger) == MMLLOG_OK);
    assert(logger != NULL);
    assert(mmlog_logger_set_quiet(logger, 1) == MMLLOG_OK);
    assert(mmlog_logger_add_callback(logger, capture, &captured, MMLLOG_LEVEL_TRACE) == MMLLOG_OK);

    CDIN_FRONTEND_INFO(logger, "CORE", "ready %u", 7u);
    assert(captured.calls == 1);
    assert(captured.level == MMLLOG_LEVEL_INFO);
    assert(strstr(captured.line, "[FRONTEND][CORE]") != NULL);
    assert(strstr(captured.line, "ready 7") != NULL);
    assert(captured.length == strlen(captured.line));

    CDIN_FRONTEND_TRACE(logger, "CORE", "filtered by component logger threshold");
    assert(captured.calls == 1);
    CDIN_FRONTEND_ERROR(logger, "PROTOCOL", "invalid frame");
    assert(captured.calls == 2);
    assert(captured.level == MMLLOG_LEVEL_ERROR);
    assert(strstr(captured.line, "[FRONTEND][PROTOCOL]") != NULL);
    assert(strstr(captured.line, "invalid frame") != NULL);

    cdin_frontend_logger_destroy(logger);
    return 0;
}
