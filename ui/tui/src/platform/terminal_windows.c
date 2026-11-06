/* Windows console adapter for native, MSYS2 UCRT64 and ConPTY sessions. */
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
    HANDLE original_standard_input;
    int have_modes;
    int replaced_standard_input;
    int owns_input;
    int owns_output;
} terminal_context;

static void close_owned_handles(terminal_context *context)
{
    if (context == NULL) return;
    /* The owner loop waits on GetStdHandle(STD_INPUT_HANDLE). When ConPTY
     * exposes redirected stdio, the adapter reads from CONIN$ instead. Keep
     * the wait source and read source identical, and restore the process-owned
     * stdio handle before releasing the adapter-owned console handle. */
    if (context->replaced_standard_input) {
        if (SetStdHandle(STD_INPUT_HANDLE, context->original_standard_input)) {
            context->replaced_standard_input = 0;
        }
    }
    if (context->owns_input && !context->replaced_standard_input &&
        context->input != NULL && context->input != INVALID_HANDLE_VALUE) {
        CloseHandle(context->input);
    }
    if (context->owns_output && context->output != NULL &&
        context->output != INVALID_HANDLE_VALUE) {
        CloseHandle(context->output);
    }
    context->input = INVALID_HANDLE_VALUE;
    context->output = INVALID_HANDLE_VALUE;
    context->owns_input = 0;
    context->owns_output = 0;
}

static ptrdiff_t terminal_read(void *opaque, uint8_t *buffer, size_t capacity)
{
    terminal_context *context = (terminal_context *)opaque;
    DWORD read_count = 0u;
    DWORD requested;
    if (context == NULL || buffer == NULL || capacity == 0u ||
        capacity > (size_t)UINT32_MAX) return -1;
    requested = (DWORD)capacity;
    if (!ReadFile(context->input, buffer, requested, &read_count, NULL)) return -1;
    return (ptrdiff_t)read_count;
}

static ptrdiff_t terminal_write(void *opaque, const uint8_t *bytes, size_t length)
{
    terminal_context *context = (terminal_context *)opaque;
    DWORD written = 0u;
    if (context == NULL || (bytes == NULL && length != 0u) ||
        length > (size_t)UINT32_MAX) return -1;
    if (!WriteFile(context->output, bytes, (DWORD)length, &written, NULL)) return -1;
    return (ptrdiff_t)written;
}

static int terminal_size(void *opaque, uint32_t *columns, uint32_t *rows)
{
    terminal_context *context = (terminal_context *)opaque;
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (context == NULL || columns == NULL || rows == NULL ||
        !GetConsoleScreenBufferInfo(context->output, &info)) return -1;
    *columns = (uint32_t)(info.srWindow.Right - info.srWindow.Left + 1);
    *rows = (uint32_t)(info.srWindow.Bottom - info.srWindow.Top + 1);
    return 0;
}

static int terminal_open_console_handles(terminal_context *context)
{
    DWORD input_mode;
    DWORD output_mode;
    HANDLE standard_input;
    HANDLE standard_output;

    standard_input = GetStdHandle(STD_INPUT_HANDLE);
    standard_output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (standard_input != NULL && standard_input != INVALID_HANDLE_VALUE &&
        standard_output != NULL && standard_output != INVALID_HANDLE_VALUE &&
        GetConsoleMode(standard_input, &input_mode) &&
        GetConsoleMode(standard_output, &output_mode)) {
        context->input = standard_input;
        context->output = standard_output;
        context->original_input_mode = input_mode;
        context->original_output_mode = output_mode;
        context->have_modes = 1;
        return 0;
    }

    /* ConPTY deliberately connects the hosted process to byte-stream pipes.
     * Those redirected standard handles are correct for ordinary stdio but
     * are not console handles, so GetConsoleMode() and console-size queries
     * fail on them. Open the attached pseudoconsole's console devices instead;
     * this also keeps the TUI usable when a native console's stdio is redirected. */
    context->input = CreateFileW(
        L"CONIN$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (context->input == INVALID_HANDLE_VALUE) return -1;
    context->owns_input = 1;

    context->output = CreateFileW(
        L"CONOUT$", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (context->output == INVALID_HANDLE_VALUE) {
        close_owned_handles(context);
        return -1;
    }
    context->owns_output = 1;

    if (!GetConsoleMode(context->input, &context->original_input_mode) ||
        !GetConsoleMode(context->output, &context->original_output_mode)) {
        close_owned_handles(context);
        return -1;
    }

    /* In a ConPTY host, STD_INPUT_HANDLE remains the byte-stream pipe supplied
     * by CreateProcessW, while console input arrives on the pseudoconsole's
     * CONIN$ buffer. CDIN's readiness bridge waits on GetStdHandle(), so point
     * that waitable handle at the same console input object used by ReadFile.
     * The original process-owned pipe is restored by close_owned_handles(). */
    context->original_standard_input = standard_input;
    if (!SetStdHandle(STD_INPUT_HANDLE, context->input)) {
        close_owned_handles(context);
        return -1;
    }
    context->replaced_standard_input = 1;
    context->have_modes = 1;
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
    context->input = INVALID_HANDLE_VALUE;
    context->output = INVALID_HANDLE_VALUE;

    if (terminal_open_console_handles(context) != 0) {
        close_owned_handles(context);
        free(context);
        return CDIN_FRONTEND_TERMINAL_NOT_INTERACTIVE;
    }

    input_mode = context->original_input_mode &
        (DWORD)~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    input_mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    output_mode = context->original_output_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(context->input, input_mode) ||
        !SetConsoleMode(context->output, output_mode)) {
        (void)SetConsoleMode(context->input, context->original_input_mode);
        (void)SetConsoleMode(context->output, context->original_output_mode);
        close_owned_handles(context);
        free(context);
        return CDIN_FRONTEND_TERMINAL_UNSUPPORTED;
    }

    terminal->context = context;
    terminal->read = terminal_read;
    terminal->write = terminal_write;
    terminal->size = terminal_size;
    terminal->active = 1;
    if (terminal_write(context, enter, sizeof(enter) - 1u) !=
        (ptrdiff_t)(sizeof(enter) - 1u)) {
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
        close_owned_handles(context);
        free(context);
    }
    memset(terminal, 0, sizeof(*terminal));
}
#else
#error "terminal_windows.c must only be compiled on Windows"
#endif
