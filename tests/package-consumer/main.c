#include <cdin/frontend/core/session/session.h>
#include <cdin/frontend/ui/tui/event_loop.h>

#include <stddef.h>
#include <stdint.h>

typedef struct consumer_state {
    unsigned shutdowns;
} consumer_state;

static int wait_for_shutdown(void *opaque, uint32_t requested, int timeout_ms, uint32_t *ready)
{
    (void)opaque;
    if (requested != CDIN_FRONTEND_TUI_EVENT_ALL || timeout_ms != -1 || ready == NULL) return -1;
    *ready = CDIN_FRONTEND_TUI_EVENT_TERMINATE;
    return 0;
}

static cdin_frontend_tui_io_result drain_no_work(void *opaque, size_t budget, size_t *processed)
{
    (void)opaque;
    if (budget == 0u || processed == NULL) return CDIN_FRONTEND_TUI_IO_FAILURE;
    *processed = 0u;
    return CDIN_FRONTEND_TUI_IO_AGAIN;
}

static int render_noop(void *opaque)
{
    (void)opaque;
    return 0;
}

static void record_shutdown(void *opaque, cdin_frontend_tui_loop_status reason)
{
    consumer_state *state = (consumer_state *)opaque;
    if (reason == CDIN_FRONTEND_TUI_LOOP_OK) state->shutdowns++;
}

int main(void)
{
    consumer_state state = { 0u };
    cdin_frontend_tui_event_loop loop = { 0 };
    cdin_frontend_protocol_status command_status;

    /* Link a Core symbol as well as the TUI event-loop object from installed archives. */
    command_status = cdin_frontend_session_request_command(NULL, "document.new", NULL);
    if (command_status != CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT) return 1;

    loop.context = &state;
    loop.wait = wait_for_shutdown;
    loop.drain_terminal = drain_no_work;
    loop.drain_backend = drain_no_work;
    loop.render = render_noop;
    loop.shutdown = record_shutdown;
    loop.max_items_per_source = 4u;
    if (cdin_frontend_tui_event_loop_run(&loop) != CDIN_FRONTEND_TUI_LOOP_OK) return 2;
    return state.shutdowns == 1u ? 0 : 3;
}
