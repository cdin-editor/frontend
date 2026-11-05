/* Cell renderer for the terminal frontend. It never interprets editor commands. */
#include "cdin/frontend/ui/tui/tui.h"

#include <stdio.h>
#include <string.h>

#define TUI_LINE_NUMBER_WIDTH 8u
#define TUI_MAX_ROWS 512u
#define TUI_MAX_COLS 512u

typedef struct output_buffer {
    uint8_t bytes[CDIN_FRONTEND_TUI_OUTPUT_CAPACITY];
    size_t length;
    int failed;
} output_buffer;

static void append_bytes(output_buffer *output, const void *bytes, size_t length)
{
    if (output->failed || length > sizeof(output->bytes) - output->length) { output->failed = 1; return; }
    if (length != 0u) memcpy(output->bytes + output->length, bytes, length);
    output->length += length;
}

static void append_text(output_buffer *output, const char *text)
{
    if (text != NULL) append_bytes(output, text, strlen(text));
}

static void append_position(output_buffer *output, size_t row, size_t column)
{
    char temporary[48];
    int length = snprintf(temporary, sizeof(temporary), "\033[%zu;%zuH", row, column);
    if (length < 0 || (size_t)length >= sizeof(temporary)) { output->failed = 1; return; }
    append_bytes(output, temporary, (size_t)length);
}

static size_t utf8_sequence_length(uint8_t first)
{
    if (first < 0x80u) return 1u;
    if (first >= 0xc2u && first <= 0xdfu) return 2u;
    if (first >= 0xe0u && first <= 0xefu) return 3u;
    if (first >= 0xf0u && first <= 0xf4u) return 4u;
    return 0u;
}

static int valid_utf8_sequence(const uint8_t *bytes, size_t length)
{
    size_t index;
    uint8_t first = bytes[0];
    if (length == 1u) return first < 0x80u;
    for (index = 1u; index < length; index++) if ((bytes[index] & 0xc0u) != 0x80u) return 0;
    if (length == 3u && first == 0xe0u && bytes[1] < 0xa0u) return 0;
    if (length == 3u && first == 0xedu && bytes[1] >= 0xa0u) return 0;
    if (length == 4u && first == 0xf0u && bytes[1] < 0x90u) return 0;
    if (length == 4u && first == 0xf4u && bytes[1] >= 0x90u) return 0;
    return 1;
}

static uint32_t decode_codepoint(const uint8_t *bytes, size_t length)
{
    if (length == 1u) return bytes[0];
    if (length == 2u) return ((uint32_t)(bytes[0] & 0x1fu) << 6u) |
                           (uint32_t)(bytes[1] & 0x3fu);
    if (length == 3u) return ((uint32_t)(bytes[0] & 0x0fu) << 12u) |
                           ((uint32_t)(bytes[1] & 0x3fu) << 6u) |
                           (uint32_t)(bytes[2] & 0x3fu);
    return ((uint32_t)(bytes[0] & 0x07u) << 18u) |
           ((uint32_t)(bytes[1] & 0x3fu) << 12u) |
           ((uint32_t)(bytes[2] & 0x3fu) << 6u) |
           (uint32_t)(bytes[3] & 0x3fu);
}

static int codepoint_is_combining(uint32_t value)
{
    return (value >= 0x0300u && value <= 0x036fu) ||
           (value >= 0x0483u && value <= 0x0489u) ||
           (value >= 0x0591u && value <= 0x05bdu) || value == 0x05bfu ||
           (value >= 0x05c1u && value <= 0x05c2u) ||
           (value >= 0x0610u && value <= 0x061au) ||
           (value >= 0x064bu && value <= 0x065fu) || value == 0x0670u ||
           (value >= 0x06d6u && value <= 0x06edu) ||
           (value >= 0x1ab0u && value <= 0x1affu) ||
           (value >= 0x1dc0u && value <= 0x1dffu) ||
           (value >= 0x20d0u && value <= 0x20ffu) ||
           (value >= 0xfe20u && value <= 0xfe2fu);
}

static size_t codepoint_cell_width(uint32_t value)
{
    if (codepoint_is_combining(value) || value == 0x200du ||
        (value >= 0xfe00u && value <= 0xfe0fu)) return 0u;
    if (value >= 0x1100u &&
        (value <= 0x115fu || value == 0x2329u || value == 0x232au ||
         (value >= 0x2e80u && value <= 0xa4cfu) ||
         (value >= 0xac00u && value <= 0xd7a3u) ||
         (value >= 0xf900u && value <= 0xfaffu) ||
         (value >= 0xfe10u && value <= 0xfe19u) ||
         (value >= 0xfe30u && value <= 0xfe6fu) ||
         (value >= 0xff00u && value <= 0xff60u) ||
         (value >= 0xffe0u && value <= 0xffe6u) ||
         (value >= 0x1f300u && value <= 0x1faffu) ||
         (value >= 0x20000u && value <= 0x3fffdu))) return 2u;
    return 1u;
}

static size_t visible_width_before(const cdin_frontend_tui_line *line, size_t byte_limit)
{
    size_t offset = 0u;
    size_t cells = 0u;
    if (line->bytes == NULL) return 0u;
    if (byte_limit > line->length) byte_limit = line->length;
    while (offset < byte_limit) {
        uint8_t value = line->bytes[offset];
        size_t sequence;
        if (value == '\r' || value == '\n') break;
        if (value == '\t') {
            cells += 4u - (cells % 4u);
            offset++;
            continue;
        }
        if (value == 0u || value < 0x20u || value == 0x7fu) {
            cells += 4u;
            offset++;
            continue;
        }
        sequence = utf8_sequence_length(value);
        if (sequence == 0u || sequence > byte_limit - offset ||
            !valid_utf8_sequence(line->bytes + offset, sequence)) {
            cells++;
            offset++;
            continue;
        }
        cells += codepoint_cell_width(decode_codepoint(line->bytes + offset, sequence));
        offset += sequence;
    }
    return cells;
}

static int byte_is_selected(uint64_t byte_offset, size_t byte_length,
                            uint64_t selection_start, uint64_t selection_end,
                            int selection_known)
{
    uint64_t unit_end;
    if (!selection_known || selection_start >= selection_end || byte_offset > UINT64_MAX - (uint64_t)byte_length) {
        return 0;
    }
    unit_end = byte_offset + (uint64_t)byte_length;
    return byte_offset < selection_end && unit_end > selection_start;
}

static void set_selection_output(output_buffer *output, int selected, int *current)
{
    if (selected == *current) return;
    append_text(output, selected ? "\033[7m" : "\033[27m");
    *current = selected;
}

static void append_visible_line(output_buffer *output, const cdin_frontend_tui_line *line,
                                 size_t maximum_cells, uint64_t line_byte_offset,
                                 uint64_t selection_start, uint64_t selection_end,
                                 int selection_known)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t offset = 0u;
    size_t cells = 0u;
    int selection_active = 0;
    if (line->bytes == NULL && line->length != 0u) return;
    while (offset < line->length && cells < maximum_cells) {
        uint8_t value = line->bytes[offset];
        size_t sequence;
        size_t width;
        int selected;
        uint64_t absolute_offset;
        if (line_byte_offset > UINT64_MAX - (uint64_t)offset) break;
        absolute_offset = line_byte_offset + (uint64_t)offset;
        if (value == '\r' || value == '\n') break;
        if (value == '\t') {
            size_t spaces = 4u - (cells % 4u);
            selected = byte_is_selected(absolute_offset, 1u, selection_start, selection_end, selection_known);
            set_selection_output(output, selected, &selection_active);
            while (spaces-- > 0u && cells < maximum_cells) { append_text(output, " "); cells++; }
            offset++;
            continue;
        }
        if (value == 0u || value < 0x20u || value == 0x7fu) {
            char escaped[4] = { '\\', 'x', hex[value >> 4u], hex[value & 0x0fu] };
            if (cells + sizeof(escaped) > maximum_cells) break;
            selected = byte_is_selected(absolute_offset, 1u, selection_start, selection_end, selection_known);
            set_selection_output(output, selected, &selection_active);
            append_bytes(output, escaped, sizeof(escaped));
            cells += sizeof(escaped);
            offset++;
            continue;
        }
        sequence = utf8_sequence_length(value);
        if (sequence == 0u || sequence > line->length - offset ||
            !valid_utf8_sequence(line->bytes + offset, sequence)) {
            if (cells + 1u > maximum_cells) break;
            selected = byte_is_selected(absolute_offset, 1u, selection_start, selection_end, selection_known);
            set_selection_output(output, selected, &selection_active);
            append_text(output, "\xef\xbf\xbd");
            cells++;
            offset++;
            continue;
        }
        width = codepoint_cell_width(decode_codepoint(line->bytes + offset, sequence));
        if (width > maximum_cells - cells) break;
        selected = byte_is_selected(absolute_offset, sequence, selection_start, selection_end, selection_known);
        set_selection_output(output, selected, &selection_active);
        append_bytes(output, line->bytes + offset, sequence);
        cells += width;
        offset += sequence;
    }
    if (selection_active) append_text(output, "\033[27m");
}

static int write_all(cdin_frontend_terminal *terminal, const uint8_t *bytes, size_t length)
{
    size_t offset = 0u;
    while (offset < length) {
        ptrdiff_t amount = terminal->write(terminal->context, bytes + offset, length - offset);
        if (amount <= 0 || (size_t)amount > length - offset) return -1;
        offset += (size_t)amount;
    }
    return 0;
}

cdin_frontend_tui_status cdin_frontend_tui_render(
    cdin_frontend_terminal *terminal,
    const cdin_frontend_tui_frame *frame,
    cdin_frontend_logger *logger)
{
    output_buffer output;
    uint32_t columns = 80u;
    uint32_t rows = 24u;
    size_t content_rows;
    size_t number_width;
    size_t index;
    size_t cursor_row;
    size_t cursor_column;
    uint64_t cursor_line;
    uint64_t line_byte_offset;
    uint64_t selection_start = 0u;
    uint64_t selection_end = 0u;
    int selection_known;
    if (terminal == NULL || frame == NULL || terminal->write == NULL || terminal->size == NULL ||
        (frame->lines == NULL && frame->line_count != 0u)) return CDIN_FRONTEND_TUI_INVALID_ARGUMENT;
    memset(&output, 0, sizeof(output));
    if (terminal->size(terminal->context, &columns, &rows) != 0) { columns = 80u; rows = 24u; }
    if (columns == 0u) columns = 1u;
    if (rows < 3u) rows = 3u;
    if (columns > TUI_MAX_COLS) columns = TUI_MAX_COLS;
    if (rows > TUI_MAX_ROWS) rows = TUI_MAX_ROWS;
    number_width = frame->show_line_numbers ? TUI_LINE_NUMBER_WIDTH : 0u;
    content_rows = (size_t)rows - 2u;
    line_byte_offset = frame->first_line_byte_offset;
    selection_known = frame->selection_known;
    if (frame->selection_anchor <= frame->selection_caret) {
        selection_start = frame->selection_anchor;
        selection_end = frame->selection_caret;
    } else {
        selection_start = frame->selection_caret;
        selection_end = frame->selection_anchor;
    }

    append_text(&output, "\033[?25l\033[H\033[0m\033[7m");
    append_text(&output, frame->title != NULL ? frame->title : "CDIN");
    append_text(&output, "  ");
    append_text(&output, frame->mode != NULL ? frame->mode : "NORMAL");
    append_text(&output, "\033[K\033[0m\r\n");

    for (index = 0u; index < content_rows; index++) {
        uint64_t logical_line = frame->top_line + (uint64_t)index;
        if (index < frame->line_count) {
            if (number_width != 0u) {
                char number[16];
                int number_length = snprintf(number, sizeof(number), "%7llu ",
                                             (unsigned long long)(logical_line + 1u));
                if (number_length < 0 || (size_t)number_length >= sizeof(number)) output.failed = 1;
                else append_bytes(&output, number, (size_t)number_length);
            }
            append_visible_line(&output, &frame->lines[index],
                                (size_t)columns > number_width ? (size_t)columns - number_width : 1u,
                                line_byte_offset, selection_start, selection_end, selection_known);
            if (line_byte_offset <= UINT64_MAX - (uint64_t)frame->lines[index].length) {
                line_byte_offset += (uint64_t)frame->lines[index].length;
            }
        } else if (number_width != 0u) {
            append_text(&output, "        ");
        }
        append_text(&output, "\033[K");
        if (index + 1u < content_rows) append_text(&output, "\r\n");
    }
    append_text(&output, "\r\n\033[7m");
    append_text(&output, frame->status != NULL ? frame->status : " ");
    append_text(&output, "\033[K\033[0m\r\n");
    append_text(&output, frame->prompt != NULL ? frame->prompt : "");
    append_text(&output, "\033[K");

    cursor_line = frame->cursor_line;
    if (cursor_line < frame->top_line) cursor_line = frame->top_line;
    if (cursor_line >= frame->top_line + content_rows) cursor_line = frame->top_line + content_rows - 1u;
    cursor_row = 2u + (size_t)(cursor_line - frame->top_line);
    cursor_column = 1u + number_width;
    if (cursor_line >= frame->top_line && cursor_line < frame->top_line + frame->line_count) {
        size_t line_index = (size_t)(cursor_line - frame->top_line);
        uint64_t byte_position = frame->cursor_byte;
        if (byte_position > (uint64_t)frame->lines[line_index].length) byte_position = (uint64_t)frame->lines[line_index].length;
        cursor_column += visible_width_before(&frame->lines[line_index], (size_t)byte_position);
    }
    if (cursor_column > (size_t)columns) cursor_column = (size_t)columns;
    append_position(&output, cursor_row, cursor_column);
    append_text(&output, "\033[?25h");

    if (output.failed || write_all(terminal, output.bytes, output.length) != 0) {
        if (logger != NULL) CDIN_FRONTEND_ERROR(logger, "TUI", "terminal frame write failed");
        return CDIN_FRONTEND_TUI_IO_ERROR;
    }
    if (logger != NULL) {
        CDIN_FRONTEND_TRACE(
            logger,
            "TUI",
            "rendered %llu lines at version %llu",
            (unsigned long long)frame->line_count,
            (unsigned long long)frame->document_version
        );
    }
    return CDIN_FRONTEND_TUI_OK;
}
