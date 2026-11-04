/* Windows console adapter for native console and MSYS2 UCRT64 builds. */
#include "cdin/frontend/ui/tui/platform/terminal.h"

#if defined(_WIN32)
#include <windows.h>
#include <stdlib.h>
#include <string.h>

typedef struct terminal_context {
    HANDLE input;
    HANDLE output;
    DWORD original_input_mode;
    DWORD original_output_mode;
    int have_modes;
} terminal_context;

static ptrdiff_t terminal_read(void *opaque, uint8_t *buffer, size_t capacity)
{
    terminal_context *context = (terminal_context *)opaque;
    DWORD read_count = 0u;
    DWORD requested;
    if (context == NULL || buffer == NULL || capacity == 0u || capacity > (size_t)UINT32_MAX) return -1;
    requested = (DWORD)capacity;
    if (!ReadFile(context->input, buffer, requested, &read_count, NULL)) return -1;
    return (ptrdiff_t)read_count;
}

static ptrdiff_t terminal_write(void *opaque, const uint8_t *bytes, size_t length)
{
    terminal_context *context = (terminal_context *)opaque;
    DWORD written = 0u;
    if (context == NULL || (bytes == NULL && length != 0u) || length > (size_t)UINT32_MAX) return -1;
    if (!WriteFile(context->output, bytes, (DWORD)length, &written, NULL)) return -1;
    return (ptrdiff_t)written;
}

static int terminal_size(void *opaque, uint32_t *columns, uint32_t *rows)
{
    terminal_context *context = (terminal_context *)opaque;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (context == NULL || columns == NULL || rows == NULL || !GetConsoleScreenBufferInfo(context->output, &info)) return -1;
    *columns = (uint32_t)(info.srWindow.Right - info.srWindow.Left + 1);
    *rows = (uint32_t)(info.srWindow.Bottom - info.srWindow.Top + 1);
    return 0;
}

cdin_frontend_terminal_status cdin_frontend_terminal_open(cdin_frontend_terminal *terminal)
{
    terminal_context *context;
    DWORD input_mode;
    DWORD output_mode;
    static const uint8_t enter[] = "\033[?1049h\033[?2004h\033[?25l\033[0m\033[2J\033[H";
    if (terminal == NULL) return CDIN_FRONTEND_TERMINAL_INVALID_ARGUMENT;
    memset(terminal, 0, sizeof(*terminal));
    context = (terminal_context *)calloc(1u, sizeof(*context));
    if (context == NULL) return CDIN_FRONTEND_TERMINAL_IO_ERROR;
    context->input = GetStdHandle(STD_INPUT_HANDLE);
    context->output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (context->input == INVALID_HANDLE_VALUE || context->output == INVALID_HANDLE_VALUE ||
        !GetConsoleMode(context->input, &context->original_input_mode) ||
        !GetConsoleMode(context->output, &context->original_output_mode)) {
        free(context);
        return CDIN_FRONTEND_TERMINAL_NOT_INTERACTIVE;
    }
    context->have_modes = 1;
    input_mode = context->original_input_mode & (DWORD)~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    input_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    output_mode = context->original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(context->input, input_mode) || !SetConsoleMode(context->output, output_mode)) {
        (void)SetConsoleMode(context->input, context->original_input_mode);
        (void)SetConsoleMode(context->output, context->original_output_mode);
        free(context);
        return CDIN_FRONTEND_TERMINAL_UNSUPPORTED;
    }
    terminal->context = context;
    terminal->read = terminal_read;
    terminal->write = terminal_write;
    terminal->size = terminal_size;
    terminal->active = 1;
    if (terminal_write(context, enter, sizeof(enter) - 1u) != (ptrdiff_t)(sizeof(enter) - 1u)) {
        cdin_frontend_terminal_close(terminal);
        return CDIN_FRONTEND_TERMINAL_IO_ERROR;
    }
    return CDIN_FRONTEND_TERMINAL_OK;
}

void cdin_frontend_terminal_close(cdin_frontend_terminal *terminal)
{
    terminal_context *context;
    static const uint8_t leave[] = "\033[0m\033[?25h\033[?2004l\033[?1049l";
    if (terminal == NULL) return;
    context = (terminal_context *)terminal->context;
    if (context != NULL) {
        if (terminal->active) (void)terminal_write(context, leave, sizeof(leave) - 1u);
        if (context->have_modes) {
            (void)SetConsoleMode(context->input, context->original_input_mode);
            (void)SetConsoleMode(context->output, context->original_output_mode);
        }
        free(context);
    }
    memset(terminal, 0, sizeof(*terminal));
}
#else
#error "terminal_windows.c must only be compiled on Windows"
#endif
