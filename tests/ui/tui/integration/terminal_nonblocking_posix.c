#define _XOPEN_SOURCE 600

#include "cdin/frontend/ui/tui/platform/terminal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* Compare status flags that applications can set and rely on. Darwin's
 * F_GETFL may also expose kernel-only per-file bookkeeping (FWASWRITTEN)
 * after a PTY write; that bit is not a user-settable status flag and must not
 * be mistaken for the adapter enabling O_NONBLOCK. */
static int contains_bytes(const uint8_t *haystack, size_t haystack_length,
                          const uint8_t *needle, size_t needle_length)
{
    size_t index;
    if (needle_length > haystack_length) return 0;
    for (index = 0u; index <= haystack_length - needle_length; index++) {
        if (memcmp(haystack + index, needle, needle_length) == 0) return 1;
    }
    return 0;
}

static int public_status_flags(int flags)
{
    int mask = O_ACCMODE | O_APPEND | O_NONBLOCK;
#ifdef O_ASYNC
    mask |= O_ASYNC;
#endif
#ifdef O_SYNC
    mask |= O_SYNC;
#endif
#ifdef O_DSYNC
    mask |= O_DSYNC;
#endif
    return flags & mask;
}

static int child_check(const char *slave_name)
{
    struct termios before;
    struct termios after;
    cdin_frontend_terminal terminal;
    uint8_t byte = 0u;
    ptrdiff_t amount;
    int original_flags;
    int original_output_flags;
    int slave = open(slave_name, O_RDWR | O_NOCTTY);
    assert(slave >= 0);
    assert(dup2(slave, STDIN_FILENO) == STDIN_FILENO);
    assert(dup2(slave, STDOUT_FILENO) == STDOUT_FILENO);
    if (slave > STDOUT_FILENO) close(slave);

    original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    original_output_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    assert(original_flags >= 0 && original_output_flags >= 0);
    assert(tcgetattr(STDIN_FILENO, &before) == 0);
    memset(&terminal, 0, sizeof(terminal));
    assert(cdin_frontend_terminal_open(&terminal) == CDIN_FRONTEND_TERMINAL_OK);
    assert(terminal.active == 1);
    /* The adapter must not alter process descriptor flags. In PTY launches,
     * stdin/stdout may refer to one open-file description. */
    {
        int current_input_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        int current_output_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
        if (public_status_flags(current_input_flags) != public_status_flags(original_flags) ||
            public_status_flags(current_output_flags) != public_status_flags(original_output_flags)) {
            (void)fprintf(stderr,
                "terminal_open changed descriptor flags: stdin %d -> %d, stdout %d -> %d\\n",
                original_flags, current_input_flags,
                original_output_flags, current_output_flags);
            return -1;
        }
    }
    {
        static const uint8_t marker[] = "blocking-output";
        assert(terminal.write(terminal.context, marker, sizeof(marker) - 1u) ==
               (ptrdiff_t)(sizeof(marker) - 1u));
        /* Do not tcdrain() a PTY whose master is intentionally not consumed
         * by the parent: Darwin may wait for the peer to read queued output. */

    }
    errno = 0;
    amount = terminal.read(terminal.context, &byte, 1u);
    assert(amount == -1);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
    cdin_frontend_terminal_close(&terminal);
    assert(terminal.active == 0);
    assert(public_status_flags(fcntl(STDIN_FILENO, F_GETFL, 0)) ==
           public_status_flags(original_flags));
    assert(public_status_flags(fcntl(STDOUT_FILENO, F_GETFL, 0)) ==
           public_status_flags(original_output_flags));
    assert(tcgetattr(STDIN_FILENO, &after) == 0);
    assert(before.c_iflag == after.c_iflag);
    assert(before.c_oflag == after.c_oflag);
    assert(before.c_cflag == after.c_cflag);
    assert(before.c_lflag == after.c_lflag);
    assert(memcmp(before.c_cc, after.c_cc, sizeof(before.c_cc)) == 0);
    return 0;
}

int main(void)
{
    static const uint8_t restore_cursor[] = "\033[?25h";
    static const uint8_t restore_paste[] = "\033[?2004l";
    static const uint8_t restore_screen[] = "\033[?1049l";
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    char *slave_name;
    pid_t child;
    int status = 0;
    int master_flags;
    uint8_t captured[4096];
    size_t captured_length = 0u;
    struct timespec pause = { 0, 10000000L };
    assert(master >= 0);
    assert(grantpt(master) == 0);
    assert(unlockpt(master) == 0);
    slave_name = ptsname(master);
    assert(slave_name != NULL);
    master_flags = fcntl(master, F_GETFL, 0);
    assert(master_flags >= 0);
    assert(fcntl(master, F_SETFL, master_flags | O_NONBLOCK) == 0);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        int result;
        (void)close(master);
        /* Bound failures inside terminal setup/read/restore so a platform
         * regression is reported by CTest rather than hanging the job. */
        (void)alarm(4u);
        result = child_check(slave_name);
        _exit(result);
    }
    /* The PTY master represents the terminal emulator. Keep consuming its
     * output while the child restores termios with TCSAFLUSH; waiting without
     * reading can deadlock a valid terminal teardown on Darwin. */
    for (;;) {
        uint8_t chunk[512];
        ssize_t amount;
        pid_t waited;
        do {
            amount = read(master, chunk, sizeof(chunk));
            if (amount > 0) {
                size_t copy = (size_t)amount;
                if (copy > sizeof(captured) - captured_length) {
                    copy = sizeof(captured) - captured_length;
                }
                if (copy != 0u) {
                    memcpy(captured + captured_length, chunk, copy);
                    captured_length += copy;
                }
            }
        } while (amount > 0);
        assert(amount == 0 || errno == EAGAIN || errno == EWOULDBLOCK ||
               errno == EINTR || errno == EIO);
        waited = waitpid(child, &status, WNOHANG);
        if (waited == child) break;
        assert(waited == 0 || (waited < 0 && errno == EINTR));
        (void)nanosleep(&pause, NULL);
    }
    for (;;) {
        uint8_t chunk[512];
        ssize_t amount = read(master, chunk, sizeof(chunk));
        if (amount <= 0) break;
        {
            size_t copy = (size_t)amount;
            if (copy > sizeof(captured) - captured_length) {
                copy = sizeof(captured) - captured_length;
            }
            if (copy != 0u) {
                memcpy(captured + captured_length, chunk, copy);
                captured_length += copy;
            }
        }
    }
    (void)close(master);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        (void)fprintf(stderr, "POSIX terminal PTY child failed or timed out (status=%d)\\n", status);
        return 1;
    }
    assert(contains_bytes(captured, captured_length,
                          restore_cursor, sizeof(restore_cursor) - 1u));
    assert(contains_bytes(captured, captured_length,
                          restore_paste, sizeof(restore_paste) - 1u));
    assert(contains_bytes(captured, captured_length,
                          restore_screen, sizeof(restore_screen) - 1u));
    return 0;
}
