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
    {
        static const uint8_t family[] = {
            0xf0u,0x9fu,0x91u,0xa9u, 0xe2u,0x80u,0x8du,
            0xf0u,0x9fu,0x91u,0xa9u, 0xe2u,0x80u,0x8du,
            0xf0u,0x9fu,0x91u,0xa7u, 0xe2u,0x80u,0x8du,
            0xf0u,0x9fu,0x91u,0xa6u, 'X'
        };
        static const uint8_t flag[] = {
            0xf0u,0x9fu,0x87u,0xaeu, 0xf0u,0x9fu,0x87u,0xb7u, 'X'
        };
        static const uint8_t heart[] = {
            0xe2u,0x9du,0xa4u, 0xefu,0xb8u,0x8fu, 'X'
        };
        static const uint8_t combining[] = { 'A', 0xccu,0x81u, 'X' };
        static const uint8_t invalid_utf8[] = { 0xffu, 'X' };
        static const uint8_t family_cursor[] = "\033[2;11H";
        static const uint8_t text_cursor[] = "\033[2;10H";
        static const uint8_t replacement[] = "\xef\xbf\xbd";

        lines[0].bytes = family;
        lines[0].length = sizeof(family);
        frame.lines = lines;
        frame.line_count = 1u;
        frame.cursor_line = 0u;
        frame.cursor_byte = sizeof(family) - 1u;
        frame.selection_known = 0;
        captured.length = 0u;
        assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
        assert(contains_bytes(captured.output, captured.length, family, sizeof(family)));
        assert(contains_bytes(captured.output, captured.length,
                              family_cursor, sizeof(family_cursor) - 1u));

        lines[0].bytes = flag;
        lines[0].length = sizeof(flag);
        frame.cursor_byte = sizeof(flag) - 1u;
        captured.length = 0u;
        assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
        assert(contains_bytes(captured.output, captured.length,
                              family_cursor, sizeof(family_cursor) - 1u));

        lines[0].bytes = heart;
        lines[0].length = sizeof(heart);
        frame.cursor_byte = sizeof(heart) - 1u;
        captured.length = 0u;
        assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
        assert(contains_bytes(captured.output, captured.length,
                              family_cursor, sizeof(family_cursor) - 1u));

        lines[0].bytes = combining;
        lines[0].length = sizeof(combining);
        frame.cursor_byte = sizeof(combining) - 1u;
        captured.length = 0u;
        assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
        assert(contains_bytes(captured.output, captured.length,
                              text_cursor, sizeof(text_cursor) - 1u));

        lines[0].bytes = invalid_utf8;
        lines[0].length = sizeof(invalid_utf8);
        frame.cursor_byte = 0u;
        captured.length = 0u;
        assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
        assert(contains_bytes(captured.output, captured.length,
                              replacement, sizeof(replacement) - 1u));
        assert(!contains_bytes(captured.output, captured.length, invalid_utf8, 1u));

        {
            static const uint8_t ordinary_zwj[] = { 'A', 0xe2u,0x80u,0x8du, 'B', 'X' };
            static const uint8_t ordinary_cursor[] = "\033[2;11H";
            lines[0].bytes = ordinary_zwj;
            lines[0].length = sizeof(ordinary_zwj);
            frame.cursor_byte = sizeof(ordinary_zwj) - 1u;
            captured.length = 0u;
            assert(cdin_frontend_tui_render(&terminal, &frame, NULL) == CDIN_FRONTEND_TUI_OK);
            assert(contains_bytes(captured.output, captured.length,
                                  ordinary_zwj, sizeof(ordinary_zwj)));
            assert(contains_bytes(captured.output, captured.length,
                                  ordinary_cursor, sizeof(ordinary_cursor) - 1u));
        }
    }
    return 0;
}
