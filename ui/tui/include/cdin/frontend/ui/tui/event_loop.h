#ifndef CDIN_FRONTEND_UI_TUI_EVENT_LOOP_H
#define CDIN_FRONTEND_UI_TUI_EVENT_LOOP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_TUI_EVENT_TERMINAL_READABLE (1u << 0u)
#define CDIN_FRONTEND_TUI_EVENT_BACKEND_READABLE  (1u << 1u)
#define CDIN_FRONTEND_TUI_EVENT_RESIZE             (1u << 2u)
#define CDIN_FRONTEND_TUI_EVENT_TERMINATE          (1u << 3u)
#define CDIN_FRONTEND_TUI_EVENT_ALL \
    (CDIN_FRONTEND_TUI_EVENT_TERMINAL_READABLE | \
     CDIN_FRONTEND_TUI_EVENT_BACKEND_READABLE | \
     CDIN_FRONTEND_TUI_EVENT_RESIZE | \
     CDIN_FRONTEND_TUI_EVENT_TERMINATE)

typedef enum cdin_frontend_tui_io_result {
    CDIN_FRONTEND_TUI_IO_PROGRESS = 0,
    CDIN_FRONTEND_TUI_IO_AGAIN,
    CDIN_FRONTEND_TUI_IO_EOF,
    CDIN_FRONTEND_TUI_IO_FAILURE
} cdin_frontend_tui_io_result;

typedef enum cdin_frontend_tui_loop_status {
    CDIN_FRONTEND_TUI_LOOP_OK = 0,
    CDIN_FRONTEND_TUI_LOOP_INVALID_ARGUMENT,
    CDIN_FRONTEND_TUI_LOOP_WAIT_ERROR,
    CDIN_FRONTEND_TUI_LOOP_TERMINAL_CLOSED,
    CDIN_FRONTEND_TUI_LOOP_BACKEND_CLOSED,
    CDIN_FRONTEND_TUI_LOOP_IO_ERROR,
    CDIN_FRONTEND_TUI_LOOP_RENDER_ERROR
} cdin_frontend_tui_loop_status;

typedef int (*cdin_frontend_tui_wait_fn)(
    void *context, uint32_t requested_events, int timeout_ms, uint32_t *out_ready_events);
typedef cdin_frontend_tui_io_result (*cdin_frontend_tui_drain_fn)(
    void *context, size_t max_items, size_t *out_processed);
typedef int (*cdin_frontend_tui_resize_fn)(void *context);
typedef int (*cdin_frontend_tui_render_fn)(void *context);
typedef void (*cdin_frontend_tui_shutdown_fn)(
    void *context, cdin_frontend_tui_loop_status reason);

typedef struct cdin_frontend_tui_event_loop {
    void *context;
    cdin_frontend_tui_wait_fn wait;
    cdin_frontend_tui_drain_fn drain_terminal;
    cdin_frontend_tui_drain_fn drain_backend;
    cdin_frontend_tui_resize_fn on_resize;
    cdin_frontend_tui_render_fn render;
    cdin_frontend_tui_shutdown_fn shutdown;
    size_t max_items_per_source;
    int render_initial_frame;
} cdin_frontend_tui_event_loop;

/*
 * Runs a readiness-driven loop. The composition root implements wait() and
 * multiplexes terminal, transport, resize and termination sources. Drain
 * callbacks must be nonblocking. PROGRESS must report 1..max_items processed;
 * AGAIN, EOF and ERROR must report zero. Each source receives a bounded budget
 * per turn so neither can starve the other. A shutdown callback is mandatory
 * so the composition root can detach and restore the terminal on every exit.
 * timeout_ms is -1: this function never polls or busy-waits.
 */
cdin_frontend_tui_loop_status cdin_frontend_tui_event_loop_run(
    const cdin_frontend_tui_event_loop *loop);

#ifdef __cplusplus
}
#endif
#endif
