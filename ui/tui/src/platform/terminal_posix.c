/* POSIX terminal adapter. It owns only terminal mode and file descriptors. */
#include "cdin/frontend/ui/tui/platform/terminal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

typedef struct terminal_context {
    struct termios original;
    int input_fd;
    int output_fd;
    int have_original;
} terminal_context;

static ptrdiff_t terminal_read(void *opaque, uint8_t *buffer, size_t capacity)
{
    terminal_context *context = (terminal_context *)opaque;
    ssize_t amount;
    if (context == NULL || buffer == NULL || capacity == 0u) return -1;
    do { amount = read(context->input_fd, buffer, capacity); } while (amount < 0 && errno == EINTR);
    return (ptrdiff_t)amount;
}

static ptrdiff_t terminal_write(void *opaque, const uint8_t *bytes, size_t length)
{
    terminal_context *context = (terminal_context *)opaque;
    ssize_t amount;
    if (context == NULL || (bytes == NULL && length != 0u)) return -1;
    do { amount = write(context->output_fd, bytes, length); } while (amount < 0 && errno == EINTR);
    return (ptrdiff_t)amount;
}

static int terminal_size(void *opaque, uint32_t *columns, uint32_t *rows)
{
    terminal_context *context = (terminal_context *)opaque;
    struct winsize size;
    if (context == NULL || columns == NULL || rows == NULL) return -1;
    memset(&size, 0, sizeof(size));
    if (ioctl(context->output_fd, TIOCGWINSZ, &size) != 0 || size.ws_col == 0u || size.ws_row == 0u) return -1;
    *columns = (uint32_t)size.ws_col;
    *rows = (uint32_t)size.ws_row;
    return 0;
}

cdin_frontend_terminal_status cdin_frontend_terminal_open(cdin_frontend_terminal *terminal)
{
    terminal_context *context;
    struct termios raw;
    static const uint8_t enter[] = "\033[?1049h\033[?2004h\033[?25l\033[0m\033[2J\033[H";
    if (terminal == NULL) return CDIN_FRONTEND_TERMINAL_INVALID_ARGUMENT;
    memset(terminal, 0, sizeof(*terminal));
    {
        const char *input_name;
        struct stat input_device;
        struct stat output_device;
        if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
            return CDIN_FRONTEND_TERMINAL_NOT_INTERACTIVE;
        }
        input_name = ttyname(STDIN_FILENO);
        if (input_name == NULL) return CDIN_FRONTEND_TERMINAL_NOT_INTERACTIVE;
        context = (terminal_context *)calloc(1u, sizeof(*context));
        if (context == NULL) return CDIN_FRONTEND_TERMINAL_IO_ERROR;
        /* Open a new file description for input. O_NONBLOCK on STDIN_FILENO
         * also changes STDOUT_FILENO when both descriptors share an open-file
         * description (the normal PTY launch case). Keeping input separate
         * ensures renderer writes remain blocking and cannot fail with EAGAIN. */
        context->input_fd = open(input_name, O_RDONLY | O_NONBLOCK | O_NOCTTY);
        context->output_fd = STDOUT_FILENO;
        if (context->input_fd < 0 ||
            fstat(context->input_fd, &input_device) != 0 ||
            fstat(context->output_fd, &output_device) != 0 ||
            !S_ISCHR(input_device.st_mode) || !S_ISCHR(output_device.st_mode) ||
            input_device.st_rdev != output_device.st_rdev) {
            if (context->input_fd >= 0) (void)close(context->input_fd);
            free(context);
            return CDIN_FRONTEND_TERMINAL_NOT_INTERACTIVE;
        }
        if (tcgetattr(context->input_fd, &context->original) != 0) {
            (void)close(context->input_fd);
            free(context);
            return CDIN_FRONTEND_TERMINAL_IO_ERROR;
        }
    }
    context->have_original = 1;
    context->have_original = 1;
    raw = context->original;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t)~OPOST;
    raw.c_cflag |= (tcflag_t)CS8;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(context->input_fd, TCSAFLUSH, &raw) != 0) {
        (void)tcsetattr(context->input_fd, TCSAFLUSH, &context->original);
        (void)close(context->input_fd);
        free(context);
        return CDIN_FRONTEND_TERMINAL_IO_ERROR;
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
    static const uint8_t leave[] = "\033[0m\033[?25h\033[?2004l\033[?1049l";
    terminal_context *context;
    if (terminal == NULL) return;
    context = (terminal_context *)terminal->context;
    if (context != NULL) {
        if (terminal->active) (void)terminal_write(context, leave, sizeof(leave) - 1u);
        if (context->have_original) (void)tcsetattr(context->input_fd, TCSAFLUSH, &context->original);
        if (context->input_fd >= 0) (void)close(context->input_fd);
        free(context);
    }
    memset(terminal, 0, sizeof(*terminal));
}
