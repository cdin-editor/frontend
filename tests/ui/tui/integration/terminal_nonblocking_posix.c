#define _XOPEN_SOURCE 600

#include "cdin/frontend/ui/tui/platform/terminal.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

static int child_check(const char *slave_name)
{
    struct termios before;
    struct termios after;
    cdin_frontend_terminal terminal;
    uint8_t byte = 0u;
    ptrdiff_t amount;
    int original_flags;
    int slave = open(slave_name, O_RDWR | O_NOCTTY);
    assert(slave >= 0);
    assert(dup2(slave, STDIN_FILENO) == STDIN_FILENO);
    assert(dup2(slave, STDOUT_FILENO) == STDOUT_FILENO);
    if (slave > STDOUT_FILENO) close(slave);

    original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    assert(original_flags >= 0);
    assert(tcgetattr(STDIN_FILENO, &before) == 0);
    memset(&terminal, 0, sizeof(terminal));
    assert(cdin_frontend_terminal_open(&terminal) == CDIN_FRONTEND_TERMINAL_OK);
    assert(terminal.active == 1);
    errno = 0;
    amount = terminal.read(terminal.context, &byte, 1u);
    assert(amount == -1);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
    cdin_frontend_terminal_close(&terminal);
    assert(terminal.active == 0);
    assert(fcntl(STDIN_FILENO, F_GETFL, 0) == original_flags);
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
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    char *slave_name;
    pid_t child;
    int status = 0;
    assert(master >= 0);
    assert(grantpt(master) == 0);
    assert(unlockpt(master) == 0);
    slave_name = ptsname(master);
    assert(slave_name != NULL);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        int result;
        (void)close(master);
        result = child_check(slave_name);
        _exit(result);
    }
    assert(waitpid(child, &status, 0) == child);
    (void)close(master);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
    return 0;
}
