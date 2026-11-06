#include "cdin/frontend/ui/tui/input/parser.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

int main(void)
{
    static const uint8_t text_run[] = "WIN_CONPTY_EDIT_8427";
    static const uint8_t text_then_enter[] = { 'h', 'i', '\r' };
    static const uint8_t escape_prefix[] = { 0x1bu, '[' };
    static const uint8_t arrow_tail[] = { 'A' };
    static const uint8_t ctrl_up[] = { 0x1bu, '[', '1', ';', '5', 'A' };
    static const uint8_t persian_first[] = { 0xd8u };
    static const uint8_t persian_second[] = { 0xa7u };
    static const uint8_t paste_open[] = { 0x1bu, '[', '2', '0', '0', '~' };
    static const uint8_t paste_body[] = { 'A', 0u, 0xffu, 0x1bu, '[', '2' };
    static const uint8_t paste_close[] = { '0', '1', '~' };
    static const uint8_t ss3_prefix[] = { 0x1bu, 'O' };
    static const uint8_t ss3_f1[] = { 'P' };
    static const uint8_t alt_x[] = { 0x1bu, 'x' };
    static const uint8_t alt_persian_first[] = { 0x1bu, 0xd8u };
    static const uint8_t alt_persian_second[] = { 0xa7u };
    static const uint8_t f5[] = { 0x1bu, '[', '1', '5', '~' };
    static const uint8_t ctrl_space[] = { 0x00u };
    static const uint8_t empty_paste[] = { 0x1bu, '[', '2', '0', '0', '~',
                                           0x1bu, '[', '2', '0', '1', '~' };
    cdin_frontend_input_parser parser;
    cdin_frontend_input_event event;

    cdin_frontend_input_parser_init(&parser);
    assert(cdin_frontend_input_parser_feed(&parser, text_run, sizeof(text_run) - 1u) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) ==
           CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_TEXT);
    assert(event.text_length == sizeof(text_run) - 1u);
    assert(memcmp(event.text, text_run, sizeof(text_run) - 1u) == 0);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);

    assert(cdin_frontend_input_parser_feed(&parser, text_then_enter,
                                           sizeof(text_then_enter)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) ==
           CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_TEXT &&
           event.text_length == 2u && memcmp(event.text, "hi", 2u) == 0);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) ==
           CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY &&
           strcmp(event.key, "enter") == 0);

    assert(cdin_frontend_input_parser_feed(&parser, escape_prefix, sizeof(escape_prefix)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_feed(&parser, arrow_tail, sizeof(arrow_tail)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "up") == 0);
    assert(cdin_frontend_input_parser_feed(&parser, ctrl_up, sizeof(ctrl_up)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "up") == 0);
    assert((event.modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u);

    assert(cdin_frontend_input_parser_feed(&parser, persian_first, sizeof(persian_first)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_feed(&parser, persian_second, sizeof(persian_second)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_TEXT && event.text_length == 2u);
    assert(event.text[0] == 0xd8u && event.text[1] == 0xa7u);

    assert(cdin_frontend_input_parser_feed(&parser, paste_open, sizeof(paste_open)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_feed(&parser, paste_body, sizeof(paste_body)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_feed(&parser, paste_close, sizeof(paste_close)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_PASTE && event.text_length == 3u);
    assert(memcmp(event.text, paste_body, 3u) == 0);

    assert(cdin_frontend_input_parser_feed(&parser, (const uint8_t *)"\033", 1u) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 1, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "escape") == 0);

    assert(cdin_frontend_input_parser_feed(&parser, ss3_prefix, sizeof(ss3_prefix)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_feed(&parser, ss3_f1, sizeof(ss3_f1)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "f1") == 0);

    assert(cdin_frontend_input_parser_feed(&parser, alt_x, sizeof(alt_x)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "x") == 0);
    assert((event.modifiers & CDIN_FRONTEND_MOD_ALT) != 0u);

    assert(cdin_frontend_input_parser_feed(&parser, alt_persian_first, sizeof(alt_persian_first)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_feed(&parser, alt_persian_second, sizeof(alt_persian_second)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "\xd8\xa7") == 0);
    assert((event.modifiers & CDIN_FRONTEND_MOD_ALT) != 0u);

    assert(cdin_frontend_input_parser_feed(&parser, f5, sizeof(f5)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "f5") == 0);

    assert(cdin_frontend_input_parser_feed(&parser, ctrl_space, sizeof(ctrl_space)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_KEY && strcmp(event.key, "space") == 0);
    assert((event.modifiers & CDIN_FRONTEND_MOD_CTRL) != 0u);

    assert(cdin_frontend_input_parser_feed(&parser, empty_paste, sizeof(empty_paste)) ==
           CDIN_FRONTEND_INPUT_PARSER_NEED_MORE);
    assert(cdin_frontend_input_parser_next(&parser, 0, &event) == CDIN_FRONTEND_INPUT_PARSER_EVENT);
    assert(event.kind == CDIN_FRONTEND_INPUT_PASTE && event.text_length == 0u);
    return 0;
}
