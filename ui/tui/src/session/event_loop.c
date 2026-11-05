#include "cdin/frontend/ui/tui/event_loop.h"

static cdin_frontend_tui_loop_status drain_source(
    cdin_frontend_tui_drain_fn drain,
    void *context,
    size_t budget,
    cdin_frontend_tui_loop_status closed_status)
{
    size_t processed = 0u;
    while (processed < budget) {
        size_t amount = 0u;
        size_t remaining = budget - processed;
        cdin_frontend_tui_io_result result = drain(context, remaining, &amount);
        if (result == CDIN_FRONTEND_TUI_DRAIN_AGAIN) {
            return amount == 0u ? CDIN_FRONTEND_TUI_LOOP_OK :
                                 CDIN_FRONTEND_TUI_LOOP_IO_ERROR;
        }
        if (result == CDIN_FRONTEND_TUI_DRAIN_EOF) {
            return amount == 0u ? closed_status : CDIN_FRONTEND_TUI_LOOP_IO_ERROR;
        }
        if (result == CDIN_FRONTEND_TUI_DRAIN_ERROR) {
            return CDIN_FRONTEND_TUI_LOOP_IO_ERROR;
        }
        if (result != CDIN_FRONTEND_TUI_DRAIN_PROGRESS || amount == 0u ||
            amount > remaining) return CDIN_FRONTEND_TUI_LOOP_IO_ERROR;
        processed += amount;
    }
    return CDIN_FRONTEND_TUI_LOOP_OK;
}

static void notify_shutdown(const cdin_frontend_tui_event_loop *loop,
                            cdin_frontend_tui_loop_status reason)
{
    if (loop->shutdown != NULL) loop->shutdown(loop->context, reason);
}

cdin_frontend_tui_loop_status cdin_frontend_tui_event_loop_run(
    const cdin_frontend_tui_event_loop *loop)
{
    int first_frame;
    if (loop == NULL) return CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT;
    if (loop->wait == NULL || loop->drain_terminal == NULL ||
        loop->drain_backend == NULL || loop->render == NULL ||
        loop->shutdown == NULL || loop->max_items_per_source == 0u) {
        notify_shutdown(loop, CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT);
        return CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT;
    }
    first_frame = loop->render_initial_frame != 0;
    if (first_frame) {
        if (loop->render(loop->context) != 0) {
            notify_shutdown(loop, CDIN_FRONTEND_TUI_LOOP_RENDER_ERROR);
            return CDIN_FRONTEND_TUI_LOOP_RENDER_ERROR;
        }
    }
    for (;;) {
        uint32_t ready = 0u;
        cdin_frontend_tui_loop_status status;
        int dirty = 0;
        if (loop->wait(loop->context, CDIN_FRONTEND_TUI_EVENT_ALL, -1, &ready) != 0 ||
            (ready & ~CDIN_FRONTEND_TUI_EVENT_ALL) != 0u || ready == 0u) {
            notify_shutdown(loop, CDIN_FRONTEND_TUI_LOOP_WAIT_ERROR);
            return CDIN_FRONTEND_TUI_LOOP_WAIT_ERROR;
        }
        if ((ready & CDIN_FRONTEND_TUI_EVENT_TERMINATE) != 0u) {
            notify_shutdown(loop, CDIN_FRONTEND_TUI_LOOP_OK);
            return CDIN_FRONTEND_TUI_LOOP_OK;
        }
        /* Always service both ready sources in the same turn. */
        if ((ready & CDIN_FRONTEND_TUI_EVENT_BACKEND_READABLE) != 0u) {
            status = drain_source(loop->drain_backend, loop->context,
                                  loop->max_items_per_source,
                                  CDIN_FRONTEND_TUI_LOOP_BACKEND_CLOSED);
            if (status != CDIN_FRONTEND_TUI_LOOP_OK) {
                notify_shutdown(loop, status);
                return status;
            }
            dirty = 1;
        }
        if ((ready & CDIN_FRONTEND_TUI_EVENT_TERMINAL_READABLE) != 0u) {
            status = drain_source(loop->drain_terminal, loop->context,
                                  loop->max_items_per_source,
                                  CDIN_FRONTEND_TUI_LOOP_TERMINAL_CLOSED);
            if (status != CDIN_FRONTEND_TUI_LOOP_OK) {
                notify_shutdown(loop, status);
                return status;
            }
        }
        if ((ready & CDIN_FRONTEND_TUI_EVENT_RESIZE) != 0u) {
            if (loop->on_resize == NULL || loop->on_resize(loop->context) != 0) {
                notify_shutdown(loop, CDIN_FRONTEND_TUI_LOOP_IO_ERROR);
                return CDIN_FRONTEND_TUI_LOOP_IO_ERROR;
            }
            dirty = 1;
        }
        if (dirty && loop->render(loop->context) != 0) {
            notify_shutdown(loop, CDIN_FRONTEND_TUI_LOOP_RENDER_ERROR);
            return CDIN_FRONTEND_TUI_LOOP_RENDER_ERROR;
        }
    }
}
