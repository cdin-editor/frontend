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
    length = build_cursor(payload, sizeof(payload), "view-1", 3u, 12u, 1, 7u);
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
    assert(strcmp(action.requests[1].view_id, "view-1") == 0);
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
    cdin_frontend_gui_sdl_model_cancel_line_request(&model, "view-0");

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
    assert(model.cache_bytes == sizeof(first_text) - 1u);

    /* A malformed replacement layout does not destroy the last valid tree. */
    before_invalid = model.layout;
    /* A one-byte reserved CBOR value is a deliberately malformed layout. */
    payload[0] = 0xffu;
    length = 1u;
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                         payload, 1u, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
    length = build_snapshot_flush(payload, sizeof(payload), 5u, 6u);
    assert(apply_payload(&model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                         payload, length, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.layout.revision == before_invalid.revision);
    assert(model.layout.node_count == before_invalid.node_count);

    cdin_frontend_gui_sdl_model_destroy(&model);
    assert(model.cache_bytes == 0u);
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
    test_aggregate_cache_budget_and_lru();
    return 0;
}
