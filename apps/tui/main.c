/*
 * Runnable TUI presentation preview.
 *
 * FE-009/FE-011: the first interactive frontend is a full-screen TUI.
 * FE-019: terminal ownership is acquired once and released on every exit.
 * PROTO-004: this preview handles only local navigation/quit keys; editor
 * commands and keymaps remain Runtime-owned in the connected application.
 *
 * This executable intentionally uses a static read-only frame. It is a
 * renderer/platform smoke test, not a substitute for cdin's live transport
 * and Runtime composition root.
 */
#include "cdin/frontend/ui/tui/input/parser.h"
#include "cdin/frontend/ui/tui/platform/terminal.h"
#include "cdin/frontend/ui/tui/tui.h"

#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <poll.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <string.h>

static const uint8_t line_0[] = "# CDIN Frontend";
static const uint8_t line_1[] = "";
static const uint8_t line_2[] = "This is the standalone TUI presentation preview.";
static const uint8_t line_3[] = "The connected editor receives its document and UI model";
static const uint8_t line_4[] = "from Runtime through the versioned Frontend Protocol.";
static const uint8_t line_5[] = "";
static const uint8_t line_6[] = "  j / Down       move cursor down";
static const uint8_t line_7[] = "  k / Up         move cursor up";
static const uint8_t line_8[] = "  q / Ctrl-C      quit preview";

static const cdin_frontend_tui_line preview_lines[] = {
    { line_0, sizeof(line_0) - 1u }, { line_1, sizeof(line_1) - 1u },
    { line_2, sizeof(line_2) - 1u }, { line_3, sizeof(line_3) - 1u },
    { line_4, sizeof(line_4) - 1u }, { line_5, sizeof(line_5) - 1u },
    { line_6, sizeof(line_6) - 1u }, { line_7, sizeof(line_7) - 1u },
    { line_8, sizeof(line_8) - 1u }
};

static int is_quit(const cdin_frontend_input_event *event)
{
    return (event->kind == CDIN_FRONTEND_INPUT_TEXT && event->text_length == 1u &&
            event->text[0] == 'q') ||
           (event->kind == CDIN_FRONTEND_INPUT_KEY &&
            strcmp(event->key, "c") == 0 &&
            (event->modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u);
}

static int wait_for_terminal_input(void)
{
#if defined(_WIN32)
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD result;
    if (input == INVALID_HANDLE_VALUE) return -1;
    result = WaitForSingleObject(input, INFINITE);
    return result == WAIT_OBJECT_0 ? 0 : -1;
#else
    struct pollfd descriptor;
    int result;
    descriptor.fd = STDIN_FILENO;
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    do {
        result = poll(&descriptor, 1u, -1);
    } while (result < 0 && errno == EINTR);
    if (result <= 0) return -1;
    return (descriptor.revents & (POLLIN | POLLHUP)) != 0 ? 0 : -1;
#endif
}

static void draw(cdin_frontend_terminal *terminal, size_t cursor_line)
{
    cdin_frontend_tui_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.title = "CDIN Frontend — TUI Preview";
    frame.mode = "PREVIEW / READ ONLY";
    frame.status = "Preview only | Runtime owns editor state and commands";
    frame.prompt = "q / Ctrl-C: quit  |  j/k or arrows: move  |  live editing requires cdin transport";
    frame.lines = preview_lines;
    frame.line_count = sizeof(preview_lines) / sizeof(preview_lines[0]);
    frame.cursor_line = (uint64_t)cursor_line;
    frame.cursor_byte = 0u;
    frame.top_line = 0u;
    frame.show_line_numbers = 1;
    (void)cdin_frontend_tui_render(terminal, &frame, NULL);
}

int main(void)
{
    cdin_frontend_terminal terminal;
    cdin_frontend_input_parser parser;
    uint8_t input[128];
    size_t cursor_line = 0u;
    int running = 1;
    cdin_frontend_terminal_status open_status;

    open_status = cdin_frontend_terminal_open(&terminal);
    if (open_status != CDIN_FRONTEND_TERMINAL_OK) {
        (void)fprintf(stderr,
            "TUI preview needs an interactive terminal (status=%d).\n",
            (int)open_status);
        return 2;
    }
    cdin_frontend_input_parser_init(&parser);
    draw(&terminal, cursor_line);

    while (running) {
        ptrdiff_t amount;
        size_t index;
        if (wait_for_terminal_input() != 0) break;
        amount = terminal.read(terminal.context, input, sizeof(input));
        if (amount < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (amount == 0 || amount < 0 || (size_t)amount > sizeof(input)) break;
        if (cdin_frontend_input_parser_feed(&parser, input, (size_t)amount) !=
            CDIN_FRONTEND_INPUT_PARSER_EVENT && parser.length == 0u) {
            continue;
        }
        for (index = 0u; index < 64u; index++) {
            cdin_frontend_input_event event;
            cdin_frontend_input_parser_status status =
                cdin_frontend_input_parser_next(&parser, 0, &event);
            if (status == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE) break;
            if (status != CDIN_FRONTEND_INPUT_PARSER_EVENT) continue;
            if (is_quit(&event)) { running = 0; break; }
            if (event.kind == CDIN_FRONTEND_INPUT_TEXT && event.text_length == 1u) {
                if (event.text[0] == 'j' && cursor_line + 1u <
                    sizeof(preview_lines) / sizeof(preview_lines[0])) cursor_line++;
                if (event.text[0] == 'k' && cursor_line > 0u) cursor_line--;
            } else if (event.kind == CDIN_FRONTEND_INPUT_KEY) {
                if (strcmp(event.key, "down") == 0 && cursor_line + 1u <
                    sizeof(preview_lines) / sizeof(preview_lines[0])) cursor_line++;
                if (strcmp(event.key, "up") == 0 && cursor_line > 0u) cursor_line--;
            }
            draw(&terminal, cursor_line);
        }
    }

    /* The platform adapter restores termios/console modes and the prior screen. */
    cdin_frontend_terminal_close(&terminal);
    return 0;
}
