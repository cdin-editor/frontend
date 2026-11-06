/* Incremental terminal byte parser: escape and UTF-8 sequences may span reads. */
#include "cdin/frontend/ui/tui/input/parser.h"

#include "cdin/frontend/ui/tui/tui.h"

#include <string.h>

static const uint8_t paste_start[] = { 0x1bu, '[', '2', '0', '0', '~' };
static const uint8_t paste_end[] = { 0x1bu, '[', '2', '0', '1', '~' };

static void consume(cdin_frontend_input_parser *parser, size_t amount)
{
    if (amount >= parser->length) {
        parser->length = 0u;
        return;
    }
    memmove(parser->bytes, parser->bytes + amount, parser->length - amount);
    parser->length -= amount;
}

static size_t utf8_length(uint8_t first)
{
    if (first < 0x80u) return 1u;
    if (first >= 0xc2u && first <= 0xdfu) return 2u;
    if (first >= 0xe0u && first <= 0xefu) return 3u;
    if (first >= 0xf0u && first <= 0xf4u) return 4u;
    return 0u;
}

static int valid_utf8(const uint8_t *bytes, size_t length)
{
    size_t index;
    uint8_t first = bytes[0];
    if (length == 1u) return first < 0x80u;
    for (index = 1u; index < length; index++) {
        if ((bytes[index] & 0xc0u) != 0x80u) return 0;
    }
    if (length == 3u && first == 0xe0u && bytes[1] < 0xa0u) return 0;
    if (length == 3u && first == 0xedu && bytes[1] >= 0xa0u) return 0;
    if (length == 4u && first == 0xf0u && bytes[1] < 0x90u) return 0;
    if (length == 4u && first == 0xf4u && bytes[1] >= 0x90u) return 0;
    return 1;
}

static size_t find_bytes(const uint8_t *haystack, size_t haystack_length,
                         const uint8_t *needle, size_t needle_length)
{
    size_t index;
    if (needle_length > haystack_length) return SIZE_MAX;
    for (index = 0u; index <= haystack_length - needle_length; index++) {
        if (memcmp(haystack + index, needle, needle_length) == 0) return index;
    }
    return SIZE_MAX;
}

void cdin_frontend_input_parser_init(cdin_frontend_input_parser *parser)
{
    if (parser != NULL) memset(parser, 0, sizeof(*parser));
}

static cdin_frontend_input_parser_status parse_alt_key(
    cdin_frontend_input_parser *parser, cdin_frontend_input_event *out_event)
{
    size_t sequence;
    size_t available = parser->length - 1u;
    if (available == 0u) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
    if (parser->bytes[1] < 0x80u) {
        if (parser->bytes[1] < 0x20u || parser->bytes[1] == 0x7fu) {
            consume(parser, 1u);
            return CDIN_FRONTEND_INPUT_PARSER_INVALID;
        }
        memset(out_event, 0, sizeof(*out_event));
        out_event->kind = CDIN_FRONTEND_INPUT_KEY;
        out_event->key[0] = (char)parser->bytes[1];
        out_event->key[1] = '\0';
        out_event->modifiers = CDIN_FRONTEND_MOD_ALT;
        consume(parser, 2u);
        return CDIN_FRONTEND_INPUT_PARSER_EVENT;
    }
    sequence = utf8_length(parser->bytes[1]);
    if (sequence == 0u) {
        consume(parser, 2u);
        return CDIN_FRONTEND_INPUT_PARSER_INVALID;
    }
    if (available < sequence) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
    if (!valid_utf8(parser->bytes + 1u, sequence)) {
        consume(parser, 2u);
        return CDIN_FRONTEND_INPUT_PARSER_INVALID;
    }
    memset(out_event, 0, sizeof(*out_event));
    out_event->kind = CDIN_FRONTEND_INPUT_KEY;
    memcpy(out_event->key, parser->bytes + 1u, sequence);
    out_event->key[sequence] = '\0';
    out_event->modifiers = CDIN_FRONTEND_MOD_ALT;
    consume(parser, sequence + 1u);
    return CDIN_FRONTEND_INPUT_PARSER_EVENT;
}

cdin_frontend_input_parser_status cdin_frontend_input_parser_feed(
    cdin_frontend_input_parser *parser, const uint8_t *bytes, size_t length)
{
    if (parser == NULL || (bytes == NULL && length != 0u)) return CDIN_FRONTEND_INPUT_PARSER_INVALID;
    if (length > sizeof(parser->bytes) - parser->length) return CDIN_FRONTEND_INPUT_PARSER_LIMIT_EXCEEDED;
    if (length != 0u) memcpy(parser->bytes + parser->length, bytes, length);
    parser->length += length;
    return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
}

cdin_frontend_input_parser_status cdin_frontend_input_parser_next(
    cdin_frontend_input_parser *parser, int flush_escape,
    cdin_frontend_input_event *out_event)
{
    size_t length;
    if (parser == NULL || out_event == NULL) return CDIN_FRONTEND_INPUT_PARSER_INVALID;
    if (parser->length == 0u) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
    length = parser->length;

    if (parser->dropping_oversized_paste) {
        size_t end = find_bytes(parser->bytes, length, paste_end, sizeof(paste_end));
        if (end != SIZE_MAX) {
            consume(parser, end + sizeof(paste_end));
            parser->dropping_oversized_paste = 0;
        } else {
            size_t keep = sizeof(paste_end) - 1u;
            if (length > keep) consume(parser, length - keep);
        }
        return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
    }

    if (parser->bytes[0] == 0x1bu) {
        if (length == 1u) {
            if (!flush_escape) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
            memset(out_event, 0, sizeof(*out_event));
            out_event->kind = CDIN_FRONTEND_INPUT_KEY;
            memcpy(out_event->key, "escape", 7u);
            consume(parser, 1u);
            return CDIN_FRONTEND_INPUT_PARSER_EVENT;
        }
        if (parser->bytes[1] == 'O') {
            if (length < 3u) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
            if (cdin_frontend_tui_decode_input(parser->bytes, 3u, out_event) != CDIN_FRONTEND_TUI_OK) {
                consume(parser, 3u);
                return CDIN_FRONTEND_INPUT_PARSER_INVALID;
            }
            consume(parser, 3u);
            return CDIN_FRONTEND_INPUT_PARSER_EVENT;
        }
        if (length < sizeof(paste_start) &&
            memcmp(parser->bytes, paste_start, length) == 0) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
        if (length >= sizeof(paste_start) &&
            memcmp(parser->bytes, paste_start, sizeof(paste_start)) == 0) {
            size_t end = find_bytes(parser->bytes + sizeof(paste_start),
                                    length - sizeof(paste_start), paste_end, sizeof(paste_end));
            if (end == SIZE_MAX) {
                if (length - sizeof(paste_start) > CDIN_FRONTEND_INPUT_MAX_BYTES + sizeof(paste_end) - 1u) {
                    parser->dropping_oversized_paste = 1;
                    consume(parser, length > sizeof(paste_end) - 1u ?
                                   length - (sizeof(paste_end) - 1u) : 0u);
                    return CDIN_FRONTEND_INPUT_PARSER_LIMIT_EXCEEDED;
                }
                return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
            }
            if (end > CDIN_FRONTEND_INPUT_MAX_BYTES) {
                consume(parser, sizeof(paste_start) + end + sizeof(paste_end));
                return CDIN_FRONTEND_INPUT_PARSER_LIMIT_EXCEEDED;
            }
            memset(out_event, 0, sizeof(*out_event));
            out_event->kind = CDIN_FRONTEND_INPUT_PASTE;
            if (end != 0u) memcpy(out_event->text, parser->bytes + sizeof(paste_start), end);
            out_event->text_length = end;
            consume(parser, sizeof(paste_start) + end + sizeof(paste_end));
            return CDIN_FRONTEND_INPUT_PARSER_EVENT;
        }
        if (parser->bytes[1] == '[') {
            size_t end;
            for (end = 2u; end < length; end++) {
                if (parser->bytes[end] >= 0x40u && parser->bytes[end] <= 0x7eu) break;
            }
            if (end == length) {
                if (length <= 32u) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
                consume(parser, 1u);
                return CDIN_FRONTEND_INPUT_PARSER_INVALID;
            }
            if (cdin_frontend_tui_decode_input(parser->bytes, end + 1u, out_event) != CDIN_FRONTEND_TUI_OK) {
                consume(parser, end + 1u);
                return CDIN_FRONTEND_INPUT_PARSER_INVALID;
            }
            consume(parser, end + 1u);
            return CDIN_FRONTEND_INPUT_PARSER_EVENT;
        }
        return parse_alt_key(parser, out_event);
    }

    /* One terminal read commonly contains a whole burst of committed text.
     * Keep that burst as one Protocol input.text event instead of creating a
     * model publication and snapshot for every printable byte. Stop before
     * control bytes/ESC so keys and terminal sequences retain their identity. */
    if ((parser->bytes[0] >= 0x20u && parser->bytes[0] != 0x7fu) ||
        parser->bytes[0] >= 0xc2u) {
        size_t consumed = 0u;
        while (consumed < length &&
               consumed < sizeof(out_event->text)) {
            uint8_t first = parser->bytes[consumed];
            size_t sequence;
            if (first < 0x20u || first == 0x7fu) break;
            sequence = utf8_length(first);
            if (sequence == 0u) break;
            if (length - consumed < sequence) break;
            if (!valid_utf8(parser->bytes + consumed, sequence)) break;
            if (sequence > sizeof(out_event->text) - consumed) break;
            consumed += sequence;
        }
        if (consumed != 0u) {
            memset(out_event, 0, sizeof(*out_event));
            out_event->kind = CDIN_FRONTEND_INPUT_TEXT;
            memcpy(out_event->text, parser->bytes, consumed);
            out_event->text_length = consumed;
            consume(parser, consumed);
            return CDIN_FRONTEND_INPUT_PARSER_EVENT;
        }
    }

    if (parser->bytes[0] < 0x80u) {
        if (cdin_frontend_tui_decode_input(parser->bytes, 1u, out_event) != CDIN_FRONTEND_TUI_OK) {
            consume(parser, 1u);
            return CDIN_FRONTEND_INPUT_PARSER_INVALID;
        }
        consume(parser, 1u);
        return CDIN_FRONTEND_INPUT_PARSER_EVENT;
    }
    {
        size_t sequence = utf8_length(parser->bytes[0]);
        if (sequence == 0u) {
            consume(parser, 1u);
            return CDIN_FRONTEND_INPUT_PARSER_INVALID;
        }
        if (length < sequence) return CDIN_FRONTEND_INPUT_PARSER_NEED_MORE;
        if (!valid_utf8(parser->bytes, sequence)) {
            consume(parser, 1u);
            return CDIN_FRONTEND_INPUT_PARSER_INVALID;
        }
        memset(out_event, 0, sizeof(*out_event));
        out_event->kind = CDIN_FRONTEND_INPUT_TEXT;
        memcpy(out_event->text, parser->bytes, sequence);
        out_event->text_length = sequence;
        consume(parser, sequence);
        return CDIN_FRONTEND_INPUT_PARSER_EVENT;
    }
}
