#include "cdin/frontend/ui/tui/tui.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct captured_terminal {
    uint8_t output[32768];
    size_t length;
    uint32_t columns;
    uint32_t rows;
} captured_terminal;

static ptrdiff_t write_capture(void *opaque, const uint8_t *bytes, size_t length)
{
    captured_terminal *terminal = (captured_terminal *)opaque;
    size_t amount = length;
    if (amount > sizeof(terminal->output) - terminal->length) return -1;
    memcpy(terminal->output + terminal->length, bytes, amount);
    terminal->length += amount;
    return (ptrdiff_t)amount;
}

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

static int size_capture(void *opaque, uint32_t *columns, uint32_t *rows)
{
    captured_terminal *terminal = (captured_terminal *)opaque;
    *columns = terminal->columns;
    *rows = terminal->rows;
    return 0;
}

int main(void)
{
    static const uint8_t first[] = "Hello, CDIN";
    static const uint8_t second[] = { 'A', 0u, 0xffu, 'B' };
    cdin_frontend_tui_line lines[2];
    cdin_frontend_tui_frame frame;
    cdin_frontend_terminal terminal;
    captured_terminal captured;
    cdin_frontend_input_event event;
    static const uint8_t up[] = { 0x1bu, '[', 'A' };
    static const uint8_t ctrl_c[] = { 3u };
    static const uint8_t text[] = { 0xd8u, 0xa7u };
    static const uint8_t persian_line[] = { 'A', '\t', 0xd8u, 0xa7u, 0xd9u, 0x8fu };
    static const uint8_t cursor_expected[] = "\033[2;14H";

    memset(&captured, 0, sizeof(captured));
    memset(&terminal, 0, sizeof(terminal));
    captured.columns = 80u;
    captured.rows = 24u;
    terminal.context = &captured;
    terminal.write = write_capture;
    terminal.size = size_capture;
    lines[0].bytes = first;
    lines[0].length = sizeof(first) - 1u;
    lines[1].bytes = second;
    lines[1].length = sizeof(second);

    memset(&frame, 0, sizeof(frame));
    frame.title = "CDIN";
    frame.mode = "NORMAL";
    frame.status = "main.txt [+]  2:1";
    frame.prompt = ":";
    frame.lines = lines;
    frame.line_count = 2u;
    frame.cursor_line = 1u;
    frame.cursor_byte = 1u;
    frame.first_line_byte_offset = 0u;
    frame.selection_anchor = 1u;
    frame.selection_caret = 5u;
    frame.selection_known = 1;
    frame.document_version = 4u;
    frame.show_line_numbers = 1;
    assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
    assert(captured.length > 0u);
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"CDIN", 4u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"H", 1u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"ello", 4u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"CDIN", 4u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"\\x00", 4u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"\xef\xbf\xbd", 3u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"\033[7m", 4u));
    assert(contains_bytes(captured.output, captured.length, (const uint8_t *)"\033[27m", 5u));

    assert(cdin_frontend_tui_decode_input(up, sizeof(up), &event) == CDIN_FRONTEND_TUI_OK);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "up") == 0);
    assert(cdin_frontend_tui_decode_input(ctrl_c, sizeof(ctrl_c), &event) == CDIN_FRONTEND_TUI_OK);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY);
    assert((event.modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u);
    assert(strcmp(event.key, "c") == 0);
    assert(cdin_frontend_tui_decode_input(text, sizeof(text), &event) == CDIN_FRONTEND_TUI_OK);
    assert(event.kind == CDIN_FRONTEND_INPUT_TEXT && event.text_length == sizeof(text));

    lines[0].bytes = persian_line;
    lines[0].length = sizeof(persian_line);
    frame.lines = lines;
    frame.line_count = 1u;
    frame.cursor_line = 0u;
    frame.cursor_byte = 4u; /* A + tab + one two-byte Persian code point. */
    frame.selection_known = 0;
    captured.length = 0u;
    assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
    assert(contains_bytes(captured.output, captured.length,
                          cursor_expected, sizeof(cursor_expected) - 1u));
    return 0;
}
