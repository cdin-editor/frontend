#include "cdin/frontend/ui/tui/event_loop.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct fake_loop {
    size_t waits;
    size_t backend_items;
    size_t terminal_items;
    size_t renders;
    size_t resizes;
    size_t shutdowns;
    cdin_frontend_tui_loop_status shutdown_reason;
} fake_loop;

static int fake_wait(void *opaque, uint32_t requested, int timeout_ms, uint32_t *ready)
{
    fake_loop *state = (fake_loop *)opaque;
    assert(requested == CDIN_FRONTEND_TUI_EVENT_ALL);
    assert(timeout_ms == -1);
    state->waits++;
    if (state->waits == 1u) {
        *ready = CDIN_FRONTEND_TUI_EVENT_BACKEND_READABLE |
                 CDIN_FRONTEND_TUI_EVENT_TERMINAL_READABLE |
                 CDIN_FRONTEND_TUI_EVENT_RESIZE;
    } else if (state->backend_items < 5u) {
        *ready = CDIN_FRONTEND_TUI_EVENT_BACKEND_READABLE;
    } else {
        *ready = CDIN_FRONTEND_TUI_EVENT_TERMINATE;
    }
    return 0;
}

static cdin_frontend_tui_io_result fake_backend(
    void *opaque, size_t budget, size_t *out_processed)
{
    fake_loop *state = (fake_loop *)opaque;
    assert(budget > 0u && out_processed != NULL);
    *out_processed = 0u;
    if (state->backend_items < 5u) {
        state->backend_items++;
        *out_processed = 1u;
        return CDIN_FRONTEND_TUI_DRAIN_PROGRESS;
    }
    return CDIN_FRONTEND_TUI_DRAIN_AGAIN;
}

static cdin_frontend_tui_io_result fake_terminal(
    void *opaque, size_t budget, size_t *out_processed)
{
    fake_loop *state = (fake_loop *)opaque;
    assert(budget > 0u && out_processed != NULL);
    *out_processed = 0u;
    if (state->terminal_items < 1u) {
        state->terminal_items++;
        *out_processed = 1u;
        return CDIN_FRONTEND_TUI_DRAIN_PROGRESS;
    }
    return CDIN_FRONTEND_TUI_DRAIN_AGAIN;
}

static int fake_resize(void *opaque)
{
    fake_loop *state = (fake_loop *)opaque;
    state->resizes++;
    return 0;
}

static int fake_render(void *opaque)
{
    fake_loop *state = (fake_loop *)opaque;
    state->renders++;
    return 0;
}

static void fake_shutdown(void *opaque, cdin_frontend_tui_loop_status reason)
{
    fake_loop *state = (fake_loop *)opaque;
    state->shutdowns++;
    state->shutdown_reason = reason;
}

int main(void)
{
    fake_loop state;
    cdin_frontend_tui_event_loop loop;
    memset(&state, 0, sizeof(state));
    memset(&loop, 0, sizeof(loop));
    loop.context = &state;
    loop.wait = fake_wait;
    loop.drain_terminal = fake_terminal;
    loop.drain_backend = fake_backend;
    loop.on_resize = fake_resize;
    loop.render = fake_render;
    loop.shutdown = fake_shutdown;
    loop.max_items_per_source = 2u;
    loop.render_initial_frame = 1;

    assert(cdin_frontend_tui_event_loop_run(&loop) == CDIN_FRONTEND_TUI_LOOP_OK);
    assert(state.waits == 4u);
    assert(state.backend_items == 5u);
    assert(state.terminal_items == 1u);
    assert(state.resizes == 1u);
    assert(state.renders == 4u);
    assert(state.shutdowns == 1u);
    assert(state.shutdown_reason == CDIN_FRONTEND_TUI_LOOP_OK);

    loop.wait = NULL;
    assert(cdin_frontend_tui_event_loop_run(&loop) ==
           CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT);
    assert(state.shutdowns == 2u);
    assert(state.shutdown_reason == CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT);
    return 0;
}
