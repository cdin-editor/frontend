#ifndef CDIN_FRONTEND_PLATFORM_TERMINAL_H
#define CDIN_FRONTEND_PLATFORM_TERMINAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cdin_frontend_terminal_status {
    CDIN_FRONTEND_TERMINAL_OK = 0,
    CDIN_FRONTEND_TERMINAL_INVALID_ARGUMENT,
    CDIN_FRONTEND_TERMINAL_NOT_INTERACTIVE,
    CDIN_FRONTEND_TERMINAL_IO_ERROR,
    CDIN_FRONTEND_TERMINAL_UNSUPPORTED
} cdin_frontend_terminal_status;

typedef ptrdiff_t (*cdin_frontend_terminal_read_fn)(void *context, uint8_t *buffer, size_t capacity);
typedef ptrdiff_t (*cdin_frontend_terminal_write_fn)(void *context, const uint8_t *bytes, size_t length);
typedef int (*cdin_frontend_terminal_size_fn)(void *context, uint32_t *out_columns, uint32_t *out_rows);

typedef struct cdin_frontend_terminal {
    void *context;
    cdin_frontend_terminal_read_fn read;
    cdin_frontend_terminal_write_fn write;
    cdin_frontend_terminal_size_fn size;
    int active;
} cdin_frontend_terminal;

/* Opens the process terminal in raw mode and enters its alternate screen.
 * On POSIX, read() probes the dedicated input descriptor with a zero-timeout
 * poll before reading; -1 with errno EAGAIN/EWOULDBLOCK means the current input
 * batch is drained. It never changes STDIN_FILENO/STDOUT_FILENO file status
 * flags, even when those descriptors share one open-file description.
 * close() restores terminal mode, cursor and screen. */
cdin_frontend_terminal_status cdin_frontend_terminal_open(cdin_frontend_terminal *terminal);
void cdin_frontend_terminal_close(cdin_frontend_terminal *terminal);

#ifdef __cplusplus
}
#endif
#endif
