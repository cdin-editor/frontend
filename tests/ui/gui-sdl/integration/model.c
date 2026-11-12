#include "model.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct cbor_writer {
    uint8_t *data;
    size_t capacity;
    size_t length;
} cbor_writer;

static void put_head(cbor_writer *writer, uint8_t major, uint64_t value)
{
    size_t width;
    size_t index;
    assert(writer != NULL);
    if (value < 24u) {
        assert(writer->length + 1u <= writer->capacity);
        writer->data[writer->length++] =
            (uint8_t)((uint8_t)(major << 5u) | (uint8_t)value);
        return;
    }
    if (value <= UINT8_MAX) width = 1u;
    else if (value <= UINT16_MAX) width = 2u;
    else if (value <= UINT32_MAX) width = 4u;
    else width = 8u;
    assert(writer->length + 1u + width <= writer->capacity);
    writer->data[writer->length++] = (uint8_t)(
        (uint8_t)(major << 5u) | (width == 1u ? 24u :
        width == 2u ? 25u : width == 4u ? 26u : 27u));
    for (index = 0u; index < width; ++index) {
        size_t shift = (width - index - 1u) * 8u;
        writer->data[writer->length++] = (uint8_t)(value >> shift);
    }
}

static void put_uint(cbor_writer *writer, uint64_t value)
{
    put_head(writer, 0u, value);
}

static void put_map(cbor_writer *writer, size_t count)
{
    put_head(writer, 5u, (uint64_t)count);
}

static void put_array(cbor_writer *writer, size_t count)
{
    put_head(writer, 4u, (uint64_t)count);
}

static void put_text_n(cbor_writer *writer, const char *value, size_t length)
{
    put_head(writer, 3u, (uint64_t)length);
    assert(writer->length + length <= writer->capacity);
    if (length != 0u) memcpy(writer->data + writer->length, value, length);
    writer->length += length;
}

static void put_text(cbor_writer *writer, const char *value)
{
    put_text_n(writer, value, strlen(value));
}

static void put_bytes(cbor_writer *writer, const uint8_t *value, size_t length)
{
    put_head(writer, 2u, (uint64_t)length);
    assert(writer->length + length <= writer->capacity);
    if (length != 0u) memcpy(writer->data + writer->length, value, length);
    writer->length += length;
}

static void put_null(cbor_writer *writer)
{
    assert(writer->length + 1u <= writer->capacity);
    writer->data[writer->length++] = 0xf6u;
}

static void put_leaf(cbor_writer *writer, size_t leaf_index)
{
    char node_id[32];
    char view_id[32];
    (void)snprintf(node_id, sizeof(node_id), "leaf-%zu", leaf_index);
    (void)snprintf(view_id, sizeof(view_id), "view-%zu", leaf_index);
    put_map(writer, 4u);
    put_text(writer, "id");
    put_text(writer, node_id);
    put_text(writer, "kind");
    put_text(writer, "leaf");
    put_text(writer, "views");
    put_array(writer, 1u);
    put_text(writer, view_id);
    put_text(writer, "active_view");
    put_text(writer, view_id);
}

static void put_pane_node(
    cbor_writer *writer,
    size_t leaf_count,
    size_t *next_split,
    size_t *next_leaf)
{
    if (leaf_count == 1u) {
        put_leaf(writer, *next_leaf);
        (*next_leaf)++;
        return;
    }
    {
        size_t split_id = (*next_split)++;
        size_t left_count = leaf_count / 2u;
        size_t right_count = leaf_count - left_count;
        char node_id[32];
        (void)snprintf(node_id, sizeof(node_id), "split-%zu", split_id);
        put_map(writer, 6u);
        put_text(writer, "id");
        put_text(writer, node_id);
        put_text(writer, "axis");
        put_text(writer, "horizontal");
        put_text(writer, "kind");
        put_text(writer, "split");
        put_text(writer, "first");
        put_pane_node(writer, left_count, next_split, next_leaf);
        put_text(writer, "second");
        put_pane_node(writer, right_count, next_split, next_leaf);
        put_text(writer, "ratio_bp");
        put_uint(writer, 5000u);
    }
}

static size_t build_layout(
    uint8_t *output, size_t capacity, uint64_t revision, size_t view_count)
{
    cbor_writer writer = { output, capacity, 0u };
    size_t next_split = 0u;
    size_t next_leaf = 0u;
    put_map(&writer, 2u);
    put_text(&writer, "layout");
    put_map(&writer, 2u);
    put_text(&writer, "root");
    if (view_count == 0u) {
        put_null(&writer);
        put_text(&writer, "active_leaf");
        put_null(&writer);
    } else {
        put_pane_node(&writer, view_count, &next_split, &next_leaf);
        put_text(&writer, "active_leaf");
        put_text(&writer, "leaf-0");
    }
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    return writer.length;
}

static size_t build_invalidate(
    uint8_t *output, size_t capacity, const char *view_id, uint64_t version)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    return writer.length;
}

static size_t build_cursor(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version, uint64_t position,
    int has_scroll_anchor, uint64_t scroll_anchor)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, has_scroll_anchor ? 4u : 3u);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    put_text(&writer, "position");
    put_uint(&writer, position);
    if (has_scroll_anchor) {
        put_text(&writer, "scroll_anchor");
        put_uint(&writer, scroll_anchor);
    }
    return writer.length;
}

static size_t build_selection(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version,
    uint64_t anchor, uint64_t caret)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 4u);
    put_text(&writer, "caret");
    put_uint(&writer, caret);
    put_text(&writer, "anchor");
    put_uint(&writer, anchor);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    return writer.length;
}

static size_t build_lines(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version,
    uint64_t first_line, const uint8_t *line_bytes, size_t line_length)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 4u);
    put_text(&writer, "lines");
    put_array(&writer, 1u);
    put_map(&writer, 1u);
    put_text(&writer, "bytes");
    put_bytes(&writer, line_bytes, line_length);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    put_text(&writer, "first_line");
    put_uint(&writer, first_line);
    return writer.length;
}

static size_t build_lines_with_first_byte(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version,
    uint64_t first_byte, uint64_t first_line,
    const uint8_t *line_bytes, size_t line_length)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 5u);
    put_text(&writer, "lines");
    put_array(&writer, 1u);
    put_map(&writer, 1u);
    put_text(&writer, "bytes");
    put_bytes(&writer, line_bytes, line_length);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    put_text(&writer, "first_byte");
    put_uint(&writer, first_byte);
    put_text(&writer, "first_line");
    put_uint(&writer, first_line);
    return writer.length;
}

static size_t build_lines_with_one_span(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version,
    uint64_t first_line, const uint8_t *line_bytes, size_t line_length,
    uint64_t span_start, uint64_t span_end, const char *role)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 4u);
    put_text(&writer, "lines");
    put_array(&writer, 1u);
    put_map(&writer, 2u);
    put_text(&writer, "bytes");
    put_bytes(&writer, line_bytes, line_length);
    put_text(&writer, "spans");
    put_array(&writer, 1u);
    put_map(&writer, 3u);
    put_text(&writer, "end");
    put_uint(&writer, span_end);
    put_text(&writer, "role");
    put_text(&writer, role);
    put_text(&writer, "start");
    put_uint(&writer, span_start);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    put_text(&writer, "first_line");
    put_uint(&writer, first_line);
    return writer.length;
}

static size_t build_lines_with_two_spans(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version,
    uint64_t first_line, const uint8_t *line_bytes, size_t line_length,
    uint64_t first_start, uint64_t first_end,
    uint64_t second_start, uint64_t second_end, const char *role)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 4u);
    put_text(&writer, "lines");
    put_array(&writer, 1u);
    put_map(&writer, 2u);
    put_text(&writer, "bytes");
    put_bytes(&writer, line_bytes, line_length);
    put_text(&writer, "spans");
    put_array(&writer, 2u);
    put_map(&writer, 3u);
    put_text(&writer, "end");
    put_uint(&writer, first_end);
    put_text(&writer, "role");
    put_text(&writer, role);
    put_text(&writer, "start");
    put_uint(&writer, first_start);
    put_map(&writer, 3u);
    put_text(&writer, "end");
    put_uint(&writer, second_end);
    put_text(&writer, "role");
    put_text(&writer, role);
    put_text(&writer, "start");
    put_uint(&writer, second_start);
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    put_text(&writer, "first_line");
    put_uint(&writer, first_line);
    return writer.length;
}

static size_t build_style_table(
    uint8_t *output, size_t capacity, uint64_t revision, const char *role)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "styles");
    put_map(&writer, 1u);
    put_text(&writer, role);
    put_map(&writer, 0u);
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    return writer.length;
}

static size_t build_panel(
    uint8_t *output, size_t capacity, uint64_t revision,
    const char *panel_id, const char *kind)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 3u);
    put_text(&writer, "panel");
    put_map(&writer, 1u);
    put_text(&writer, "kind");
    put_text(&writer, kind);
    put_text(&writer, "panel_id");
    put_text(&writer, panel_id);
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    return writer.length;
}

static size_t build_panel_remove(
    uint8_t *output, size_t capacity, uint64_t revision,
    const char *panel_id)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 3u);
    put_text(&writer, "panel");
    put_null(&writer);
    put_text(&writer, "panel_id");
    put_text(&writer, panel_id);
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    return writer.length;
}

static size_t build_prompt(
    uint8_t *output, size_t capacity, uint64_t revision,
    const char *kind)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "prompt");
    put_map(&writer, 1u);
    put_text(&writer, "kind");
    put_text(&writer, kind);
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    return writer.length;
}

static size_t build_oversized_style_table(
    uint8_t *output, size_t capacity, const uint8_t *value, size_t value_length)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "styles");
    put_map(&writer, 1u);
    put_text(&writer, "normal");
    put_map(&writer, 1u);
    put_text(&writer, "extension");
    put_bytes(&writer, value, value_length);
    put_text(&writer, "revision");
    put_uint(&writer, 1u);
    return writer.length;
}

static size_t build_oversized_prompt(
    uint8_t *output, size_t capacity, const uint8_t *value, size_t value_length)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "prompt");
    put_map(&writer, 1u);
    put_text(&writer, "extension");
    put_bytes(&writer, value, value_length);
    put_text(&writer, "revision");
    put_uint(&writer, 1u);
    return writer.length;
}

static size_t build_prompt_clear(
    uint8_t *output, size_t capacity, uint64_t revision)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "prompt");
    put_null(&writer);
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    return writer.length;
}

static size_t build_large_lines(
    uint8_t *output, size_t capacity,
    const char *view_id, uint64_t version,
    uint64_t first_line, const uint8_t *line_bytes, size_t line_length,
    size_t count)
{
    cbor_writer writer = { output, capacity, 0u };
    size_t index;
    put_map(&writer, 4u);
    put_text(&writer, "lines");
    put_array(&writer, count);
    for (index = 0u; index < count; ++index) {
        put_map(&writer, 1u);
        put_text(&writer, "bytes");
        put_bytes(&writer, line_bytes, line_length);
    }
    put_text(&writer, "version");
    put_uint(&writer, version);
    put_text(&writer, "view_id");
    put_text(&writer, view_id);
    put_text(&writer, "first_line");
    put_uint(&writer, first_line);
    return writer.length;
}

static size_t build_snapshot_flush(
    uint8_t *output, size_t capacity, uint64_t revision, uint64_t sequence)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 3u);
    put_text(&writer, "scope");
    put_text(&writer, "snapshot");
    put_text(&writer, "revision");
    put_uint(&writer, revision);
    put_text(&writer, "sequence");
    put_uint(&writer, sequence);
    return writer.length;
}

static size_t build_response_flush(
    uint8_t *output, size_t capacity, uint64_t sequence)
{
    cbor_writer writer = { output, capacity, 0u };
    put_map(&writer, 2u);
    put_text(&writer, "scope");
    put_text(&writer, "response");
    put_text(&writer, "sequence");
    put_uint(&writer, sequence);
    return writer.length;
}

static cdin_frontend_protocol_message_view message(
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload, size_t length)
{
    cdin_frontend_protocol_message_view result;
    memset(&result, 0, sizeof(result));
    result.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    result.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    result.type = type;
    result.name_space = "core";
    result.payload = payload;
    result.payload_length = length;
    return result;
}

static cdin_frontend_gui_sdl_model_status apply_payload(
    cdin_frontend_gui_sdl_model *model,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload, size_t length,
    cdin_frontend_gui_sdl_model_action *action)
{
    cdin_frontend_protocol_message_view view = message(type, payload, length);
    return cdin_frontend_gui_sdl_model_apply(model, &view, action);
}

static void test_independent_view_state_and_cache(void)
{
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_model_action action;
    uint8_t payload[8192];
    size_t length;
    cdin_frontend_gui_sdl_model_status status;
    static const uint8_t first_text[] = "first-view";
    static const uint8_t second_text[] = "second-view";
    static const uint8_t anchored_text[] = { 'a', 'b', 'c', '\n' };
    cdin_frontend_gui_sdl_view *first;
    cdin_frontend_gui_sdl_view *second;
    cdin_frontend_gui_sdl_layout before_invalid;

    assert(cdin_frontend_gui_sdl_model_init(&model, "attach-viewport", 16u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_layout(payload, sizeof(payload), 9u, 2u);
    status = apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                           payload, length, &action);
    assert(status == CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.snapshot_pending_valid != 0);
    assert(model.view_count == 0u);
    assert(model.staged_snapshot.view_count == 2u);

    length = build_invalidate(payload, sizeof(payload), "view-0", 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-1", 3u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_cursor(payload, sizeof(payload), "view-0", 1u, 5u, 1, 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_CURSOR,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_selection(payload, sizeof(payload), "view-0", 1u, 2u, 5u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_SELECTION,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_cursor(payload, sizeof(payload), "view-1", 3u, 12u, 0, 0u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_CURSOR,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_selection(payload, sizeof(payload), "view-1", 3u, 9u, 12u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_SELECTION,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_snapshot_flush(payload, sizeof(payload), 1u, 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(action.acknowledge_sequence == 1u);
    assert(action.request_count == 2u);
    assert(strcmp(action.requests[0].view_id, "view-0") == 0);
    assert(action.requests[0].has_anchor_byte != 0);
    assert(action.requests[0].anchor_byte == 2u);
    assert(strcmp(action.requests[1].view_id, "view-1") == 0);
    assert(action.requests[1].has_anchor_byte == 0);
    assert(model.view_count == 2u);
    assert(model.layout.node_count == 3u);
    assert(model.layout.view_count == 2u);

    first = cdin_frontend_gui_sdl_model_find_view(&model, "view-0");
    second = cdin_frontend_gui_sdl_model_find_view(&model, "view-1");
    assert(first != NULL && second != NULL);
    assert(first->semantic.focused != 0);
    assert(first->semantic.visible != 0);
    assert(first->semantic.version == 1u);
    assert(first->semantic.cursor_position == 5u);
    assert(first->semantic.selection_anchor == 2u);
    assert(first->semantic.selection_caret == 5u);
    assert(first->semantic.scroll_anchor == 2u);
    assert(first->semantic.has_scroll_anchor != 0);
    assert(second->semantic.focused == 0);
    assert(second->semantic.visible != 0);
    assert(second->semantic.version == 3u);
    assert(second->semantic.cursor_position == 12u);
    assert(second->semantic.selection_anchor == 9u);
    assert(second->semantic.selection_caret == 12u);
    assert(cdin_frontend_gui_sdl_model_focused_view(&model) == first);

    assert(cdin_frontend_gui_sdl_model_expect_lines(
        &model, "view-0", 0u, 16u, 1u) == CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(cdin_frontend_gui_sdl_model_expect_lines(
        &model, "view-1", 10u, 16u, 3u) == CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_lines(payload, sizeof(payload), "view-1", 3u, 10u,
                         second_text, sizeof(second_text) - 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_lines(payload, sizeof(payload), "view-0", 1u, 0u,
                         first_text, sizeof(first_text) - 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(first->visible.count == 0u && first->pending_lines_valid != 0);
    assert(second->visible.count == 0u && second->pending_lines_valid != 0);

    length = build_response_flush(payload, sizeof(payload), 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(first->visible.count == 1u);
    assert(first->visible.first_line == 0u);
    assert(first->visible.lines[0].length == sizeof(first_text) - 1u);
    assert(memcmp(first->visible.lines[0].bytes, first_text,
                  sizeof(first_text) - 1u) == 0);
    assert(second->visible.count == 1u);
    assert(second->visible.first_line == 10u);
    assert(second->visible.lines[0].length == sizeof(second_text) - 1u);
    assert(memcmp(second->visible.lines[0].bytes, second_text,
                  sizeof(second_text) - 1u) == 0);
    assert(model.cache_bytes ==
           (sizeof(first_text) - 1u) + (sizeof(second_text) - 1u));

    /* A response for the wrong first-line window cannot overwrite either view. */
    assert(cdin_frontend_gui_sdl_model_expect_lines(
        &model, "view-0", 20u, 16u, 1u) == CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_lines(payload, sizeof(payload), "view-0", 1u, 19u,
                         second_text, sizeof(second_text) - 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    assert(first->visible.first_line == 0u);
    assert(second->visible.first_line == 10u);
    /* A response for a newer/older document version or another view is stale. */
    assert(cdin_frontend_gui_sdl_model_expect_lines(
        &model, "view-0", 20u, 16u, 1u) == CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_lines(payload, sizeof(payload), "view-0", 2u, 20u,
                         second_text, sizeof(second_text) - 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    length = build_lines(payload, sizeof(payload), "view-1", 3u, 20u,
                         second_text, sizeof(second_text) - 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    assert(first->visible.first_line == 0u);
    assert(second->visible.first_line == 10u);
    cdin_frontend_gui_sdl_model_cancel_line_request(&model, "view-0");

    /* A byte-anchored response may start at a different logical line, but its
     * exact first byte must identify a returned line that contains the anchor. */
    {
        assert(cdin_frontend_gui_sdl_model_expect_lines_ex(
            &model, "view-0", 0u, 16u, 1u, 1, 12u, 1) ==
            CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        length = build_lines_with_first_byte(
            payload, sizeof(payload), "view-0", 1u, 10u, 1u,
            anchored_text, sizeof(anchored_text));
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        length = build_response_flush(payload, sizeof(payload), 3u);
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        assert(first->visible.first_line == 1u);
        assert(first->visible.has_first_byte != 0);
        assert(first->visible.first_byte == 10u);

        /* A response whose first byte is after the requested anchor is stale;
         * the previously committed visible window remains untouched. */
        assert(cdin_frontend_gui_sdl_model_expect_lines_ex(
            &model, "view-0", 0u, 16u, 1u, 1, 12u, 1) ==
            CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        length = build_lines_with_first_byte(
            payload, sizeof(payload), "view-0", 1u, 13u, 1u,
            anchored_text, sizeof(anchored_text));
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
        assert(first->visible.first_line == 1u);
        assert(first->visible.first_byte == 10u);
        cdin_frontend_gui_sdl_model_cancel_line_request(&model, "view-0");
    }

    /* A same-version complete snapshot retains both independent line caches. */
    length = build_layout(payload, sizeof(payload), 10u, 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-0", 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-1", 3u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_snapshot_flush(payload, sizeof(payload), 2u, 3u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    first = cdin_frontend_gui_sdl_model_find_view(&model, "view-0");
    second = cdin_frontend_gui_sdl_model_find_view(&model, "view-1");
    assert(first != NULL && second != NULL);
    assert(first->visible.count == 1u);
    assert(second->visible.count == 1u);

    /* A document version change invalidates only that view's cache. */
    length = build_layout(payload, sizeof(payload), 11u, 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-0", 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-1", 4u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_snapshot_flush(payload, sizeof(payload), 3u, 4u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    first = cdin_frontend_gui_sdl_model_find_view(&model, "view-0");
    second = cdin_frontend_gui_sdl_model_find_view(&model, "view-1");
    assert(first != NULL && second != NULL);
    assert(first->semantic.version == 1u && first->visible.count == 1u);
    assert(second->semantic.version == 4u && second->visible.count == 0u);
    assert(second->semantic.needs_lines != 0);

    /* A complete replacement snapshot removes omitted views and frees their cache. */
    length = build_layout(payload, sizeof(payload), 12u, 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-0", 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_snapshot_flush(payload, sizeof(payload), 4u, 5u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.view_count == 1u);
    assert(cdin_frontend_gui_sdl_model_find_view(&model, "view-1") == NULL);
    assert(cdin_frontend_gui_sdl_model_find_view(&model, "view-0") != NULL);
    /* The byte-anchored response replaced view-0's earlier line window. */
    assert(model.cache_bytes == sizeof(anchored_text));

    /* Re-adding a removed ID creates fresh state; an earlier cache is not resurrected. */
    length = build_layout(payload, sizeof(payload), 13u, 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-0", 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-1", 5u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_snapshot_flush(payload, sizeof(payload), 5u, 7u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    second = cdin_frontend_gui_sdl_model_find_view(&model, "view-1");
    assert(second != NULL);
    assert(second->semantic.version == 5u);
    assert(second->visible.count == 0u);
    assert(second->pending_lines_valid == 0);
    assert(model.cache_bytes == sizeof(anchored_text));

    /* A malformed replacement layout does not destroy the last valid tree. */
    before_invalid = model.layout;
    /* A one-byte reserved CBOR value is a deliberately malformed layout. */
    payload[0] = 0xffu;
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, 1u, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
    length = build_snapshot_flush(payload, sizeof(payload), 6u, 8u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.layout.revision == before_invalid.revision);
    assert(model.layout.node_count == before_invalid.node_count);

    cdin_frontend_gui_sdl_model_destroy(&model);
    assert(model.cache_bytes == 0u);
}

static void test_styles_panels_prompts_and_line_spans(void)
{
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_model_action action;
    uint8_t payload[8192];
    size_t length;
    static const uint8_t line_bytes[] = "alpha beta";
    const cdin_frontend_gui_sdl_view *view;

    assert(cdin_frontend_gui_sdl_model_init(&model, "", 16u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_layout(payload, sizeof(payload), 1u, 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-0", 4u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    /* The accepted ADR-0026 style-role schema is decoded into a bounded
     * typed lookup; panel and prompt widget payloads remain opaque here. */
    length = build_style_table(payload, sizeof(payload), 7u, "normal");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_panel(payload, sizeof(payload), 1u, "tree", "tree");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PANEL,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_panel(payload, sizeof(payload), 1u, "search", "list");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PANEL,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_panel_remove(payload, sizeof(payload), 1u, "tree");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PANEL,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_prompt(payload, sizeof(payload), 1u, "input");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PROMPT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_snapshot_flush(payload, sizeof(payload), 1u, 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.presentation.style_revision == 7u);
    assert(model.presentation.style_table_length != 0u);
    assert(model.presentation.panel_count == 1u);
    assert(strcmp(model.presentation.panels[0].panel_id, "search") == 0);
    assert(model.presentation.panels[0].payload_length != 0u);
    assert(model.presentation.has_prompt != 0);
    assert(model.presentation.prompt_length != 0u);
    assert(model.presentation.encoded_bytes ==
           model.presentation.style_table_length +
           model.presentation.panels[0].payload_length +
           model.presentation.prompt_length);

    assert(cdin_frontend_gui_sdl_model_expect_lines(
        &model, "view-0", 0u, 16u, 4u) ==
        CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_lines_with_one_span(
        payload, sizeof(payload), "view-0", 4u, 0u,
        line_bytes, sizeof(line_bytes) - 1u, 6u, 10u, "keyword");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    view = cdin_frontend_gui_sdl_model_find_view_const(&model, "view-0");
    assert(view != NULL && view->pending_lines_valid != 0);
    assert(view->pending.count == 1u);
    assert(view->pending.lines[0].span_count == 1u);
    assert(view->pending.lines[0].spans_cbor_length != 0u);
    assert(view->pending.lines[0].spans_cbor[0] == 0x81u);
    {
        size_t before = model.cache_bytes;
        length = build_lines_with_one_span(
            payload, sizeof(payload), "view-0", 4u, 0u,
            line_bytes, sizeof(line_bytes) - 1u, 0u, 999u, "invalid");
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
        assert(model.cache_bytes == before);
        assert(view->pending_lines_valid != 0);
        assert(view->pending.lines[0].span_count == 1u);
    }
    {
        static const uint8_t arabic_line[] = {0xd8u, 0xa7u};
        size_t before = model.cache_bytes;
        length = build_lines_with_one_span(
            payload, sizeof(payload), "view-0", 4u, 0u,
            arabic_line, sizeof(arabic_line), 1u, 2u, "keyword");
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
        assert(model.cache_bytes == before);
        length = build_lines_with_one_span(
            payload, sizeof(payload), "view-0", 4u, 0u,
            line_bytes, sizeof(line_bytes) - 1u, 6u, 6u, "keyword");
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
        assert(model.cache_bytes == before);
    }
    {
        size_t before = model.cache_bytes;
        length = build_lines_with_two_spans(
            payload, sizeof(payload), "view-0", 4u, 0u,
            line_bytes, sizeof(line_bytes) - 1u,
            0u, 6u, 3u, 10u, "keyword");
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
        assert(model.cache_bytes == before);
    }
    length = build_response_flush(payload, sizeof(payload), 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    view = cdin_frontend_gui_sdl_model_find_view_const(&model, "view-0");
    assert(view != NULL && view->visible.count == 1u);
    assert(view->visible.lines[0].span_count == 1u);
    assert(model.cache_bytes ==
           sizeof(line_bytes) - 1u + view->visible.lines[0].spans_cbor_length);

    /* A complete snapshot replaces the declarative surfaces. Omitting panels
     * removes them; prompt null clears it; a new style table replaces the old. */
    length = build_layout(payload, sizeof(payload), 2u, 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_invalidate(payload, sizeof(payload), "view-0", 4u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_style_table(payload, sizeof(payload), 8u, "comment");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_prompt_clear(payload, sizeof(payload), 2u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PROMPT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_snapshot_flush(payload, sizeof(payload), 2u, 3u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.presentation.style_revision == 8u);
    assert(model.presentation.panel_count == 0u);
    assert(model.presentation.has_prompt == 0);
    assert(model.presentation.prompt_length == 0u);
    assert(model.presentation.encoded_bytes ==
           model.presentation.style_table_length);

    cdin_frontend_gui_sdl_model_destroy(&model);
    assert(model.cache_bytes == 0u);
}

static void test_presentation_limits_and_atomic_rejection(void)
{
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_model_action action;
    uint8_t payload[65536];
    uint8_t oversized[CDIN_FRONTEND_GUI_SDL_MAX_STYLE_TABLE_BYTES + 1u];
    char panel_id[CDIN_FRONTEND_GUI_SDL_MAX_PANEL_ID_BYTES + 2u];
    char valid_id[32];
    size_t length;
    size_t index;

    memset(oversized, 0x5a, sizeof(oversized));
    assert(cdin_frontend_gui_sdl_model_init(&model, "", 16u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    length = build_layout(payload, sizeof(payload), 1u, 0u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_oversized_style_table(
        payload, sizeof(payload), oversized, sizeof(oversized));
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_STYLE_TABLE,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT);
    assert(model.staged_snapshot.presentation.style_table_length == 0u);
    assert(model.staged_snapshot.presentation.encoded_bytes == 0u);

    length = build_oversized_prompt(
        payload, sizeof(payload), oversized, CDIN_FRONTEND_GUI_SDL_MAX_PROMPT_BYTES + 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PROMPT,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT);
    assert(model.staged_snapshot.presentation.has_prompt == 0);
    assert(model.staged_snapshot.presentation.encoded_bytes == 0u);

    for (index = 0u; index < CDIN_FRONTEND_GUI_SDL_MAX_PANELS; ++index) {
        (void)snprintf(valid_id, sizeof(valid_id), "panel-%zu", index);
        length = build_panel(payload, sizeof(payload), 1u, valid_id, "list");
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PANEL,
                             payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    }
    assert(model.staged_snapshot.presentation.panel_count ==
           CDIN_FRONTEND_GUI_SDL_MAX_PANELS);
    length = build_panel(payload, sizeof(payload), 1u, "panel-overflow", "list");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PANEL,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT);
    assert(model.staged_snapshot.presentation.panel_count ==
           CDIN_FRONTEND_GUI_SDL_MAX_PANELS);

    memset(panel_id, 'p', sizeof(panel_id));
    panel_id[sizeof(panel_id) - 1u] = '\0';
    length = build_panel(payload, sizeof(payload), 1u, panel_id, "list");
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_PANEL,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT);
    assert(model.staged_snapshot.presentation.panel_count ==
           CDIN_FRONTEND_GUI_SDL_MAX_PANELS);

    cdin_frontend_gui_sdl_model_destroy(&model);
}

static void test_aggregate_cache_budget_and_lru(void)
{
    enum { TEST_VIEW_COUNT = 9, LARGE_LINE_COUNT = 64, LARGE_LINE_BYTES = 8192 };
    const size_t large_capacity = 600000u;
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_model_action action;
    uint8_t layout_payload[32768];
    uint8_t *line_payload = (uint8_t *)malloc(large_capacity);
    uint8_t small_payload[256];
    uint8_t *line_bytes = (uint8_t *)malloc(LARGE_LINE_BYTES);
    size_t length;
    size_t index;
    size_t missing_nonfocused = 0u;
    cdin_frontend_gui_sdl_model_status status;

    assert(line_payload != NULL && line_bytes != NULL);
    memset(line_bytes, 'x', LARGE_LINE_BYTES);
    assert(cdin_frontend_gui_sdl_model_init(&model, "", 64u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);

    length = build_layout(layout_payload, sizeof(layout_payload), 1u,
                          TEST_VIEW_COUNT);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         layout_payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    for (index = 0u; index < TEST_VIEW_COUNT; ++index) {
        char view_id[32];
        (void)snprintf(view_id, sizeof(view_id), "view-%zu", index);
        length = build_invalidate(small_payload, sizeof(small_payload),
                                  view_id, 1u);
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                             small_payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    }
    length = build_snapshot_flush(small_payload, sizeof(small_payload), 1u, 1u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         small_payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(action.request_count == TEST_VIEW_COUNT);

    for (index = 0u; index < TEST_VIEW_COUNT; ++index) {
        char view_id[32];
        (void)snprintf(view_id, sizeof(view_id), "view-%zu", index);
        assert(cdin_frontend_gui_sdl_model_expect_lines(
            &model, view_id, 0u, LARGE_LINE_COUNT, 1u) ==
            CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        length = build_large_lines(
            line_payload, large_capacity, view_id, 1u, 0u, line_bytes,
            LARGE_LINE_BYTES, LARGE_LINE_COUNT);
        {
            cdin_frontend_protocol_message_view view =
                message(CDIN_FRONTEND_PROTOCOL_VIEW_LINES, line_payload, length);
            status = cdin_frontend_gui_sdl_model_apply(&model, &view, &action);
        }
        assert(status == CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        length = build_response_flush(small_payload, sizeof(small_payload),
                                      (uint64_t)index + 2u);
        assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                             small_payload, length, &action) ==
               CDIN_FRONTEND_GUI_SDL_MODEL_OK);
        assert(model.cache_bytes <= CDIN_FRONTEND_GUI_SDL_MAX_CACHE_BYTES);
    }

    {
        cdin_frontend_gui_sdl_view *focused =
            cdin_frontend_gui_sdl_model_focused_view(&model);
        assert(focused != NULL);
        assert(strcmp(focused->semantic.view_id, "view-0") == 0);
        assert(focused->visible.count == LARGE_LINE_COUNT);
    }
    for (index = 1u; index < TEST_VIEW_COUNT; ++index) {
        char view_id[32];
        const cdin_frontend_gui_sdl_view *view;
        (void)snprintf(view_id, sizeof(view_id), "view-%zu", index);
        view = cdin_frontend_gui_sdl_model_find_view_const(&model, view_id);
        assert(view != NULL);
        if (view->visible.count == 0u) missing_nonfocused++;
    }
    assert(model.cache_bytes == CDIN_FRONTEND_GUI_SDL_MAX_CACHE_BYTES);
    assert(missing_nonfocused >= 1u);

    cdin_frontend_gui_sdl_model_destroy(&model);
    assert(model.cache_bytes == 0u);
    free(line_payload);
    free(line_bytes);
}

int main(void)
{
    test_independent_view_state_and_cache();
    test_styles_panels_prompts_and_line_spans();
    test_presentation_limits_and_atomic_rejection();
    test_aggregate_cache_budget_and_lru();
    return 0;
}
