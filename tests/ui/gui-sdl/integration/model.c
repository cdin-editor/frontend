#include "model.h"

#include <assert.h>
#include <string.h>

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

int main(void)
{
    static const uint8_t pane_layout[] = {
        0xa2u, 0x68u, 0x72u, 0x65u, 0x76u, 0x69u, 0x73u, 0x69u, 0x6fu, 0x6eu, 0x01u,
        0x66u, 0x6cu, 0x61u, 0x79u, 0x6fu, 0x75u, 0x74u, 0xa2u,
        0x6bu, 0x61u, 0x63u, 0x74u, 0x69u, 0x76u, 0x65u, 0x5fu, 0x6cu, 0x65u, 0x61u, 0x66u,
        0x66u, 0x6cu, 0x65u, 0x61u, 0x66u, 0x2du, 0x61u,
        0x64u, 0x72u, 0x6fu, 0x6fu, 0x74u, 0xa6u,
        0x64u, 0x6bu, 0x69u, 0x6eu, 0x64u, 0x65u, 0x73u, 0x70u, 0x6cu, 0x69u, 0x74u,
        0x62u, 0x69u, 0x64u, 0x67u, 0x73u, 0x70u, 0x6cu, 0x69u, 0x74u, 0x2du, 0x31u,
        0x64u, 0x61u, 0x78u, 0x69u, 0x73u, 0x68u, 0x76u, 0x65u, 0x72u, 0x74u, 0x69u, 0x63u, 0x61u, 0x6cu,
        0x68u, 0x72u, 0x61u, 0x74u, 0x69u, 0x6fu, 0x5fu, 0x62u, 0x70u, 0x19u, 0x13u, 0x88u,
        0x65u, 0x66u, 0x69u, 0x72u, 0x73u, 0x74u, 0xa4u,
        0x64u, 0x6bu, 0x69u, 0x6eu, 0x64u, 0x64u, 0x6cu, 0x65u, 0x61u, 0x66u,
        0x62u, 0x69u, 0x64u, 0x66u, 0x6cu, 0x65u, 0x61u, 0x66u, 0x2du, 0x61u,
        0x65u, 0x76u, 0x69u, 0x65u, 0x77u, 0x73u, 0x81u, 0x66u, 0x76u, 0x69u, 0x65u, 0x77u, 0x2du, 0x31u,
        0x6bu, 0x61u, 0x63u, 0x74u, 0x69u, 0x76u, 0x65u, 0x5fu, 0x76u, 0x69u, 0x65u, 0x77u, 0x66u, 0x76u, 0x69u, 0x65u, 0x77u, 0x2du, 0x31u,
        0x66u, 0x73u, 0x65u, 0x63u, 0x6fu, 0x6eu, 0x64u, 0xa4u,
        0x64u, 0x6bu, 0x69u, 0x6eu, 0x64u, 0x64u, 0x6cu, 0x65u, 0x61u, 0x66u,
        0x62u, 0x69u, 0x64u, 0x66u, 0x6cu, 0x65u, 0x61u, 0x66u, 0x2du, 0x62u,
        0x65u, 0x76u, 0x69u, 0x65u, 0x77u, 0x73u, 0x81u, 0x66u, 0x76u, 0x69u, 0x65u, 0x77u, 0x2du, 0x32u,
        0x6bu, 0x61u, 0x63u, 0x74u, 0x69u, 0x76u, 0x65u, 0x5fu, 0x76u, 0x69u, 0x65u, 0x77u, 0xf6u
    };
    static const uint8_t invalid_pane_layout[] = {
        0xa2u, 0x68u, 0x72u, 0x65u, 0x76u, 0x69u, 0x73u, 0x69u, 0x6fu, 0x6eu, 0x02u,
        0x66u, 0x6cu, 0x61u, 0x79u, 0x6fu, 0x75u, 0x74u, 0xa2u,
        0x6bu, 0x61u, 0x63u, 0x74u, 0x69u, 0x76u, 0x65u, 0x5fu, 0x6cu, 0x65u, 0x61u, 0x66u,
        0x67u, 0x6du, 0x69u, 0x73u, 0x73u, 0x69u, 0x6eu, 0x67u,
        0x64u, 0x72u, 0x6fu, 0x6fu, 0x74u, 0xf6u
    };
    static const uint8_t invalidate[] = {
        0xa2u,
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1'
    };
    static const uint8_t other_view_invalidate[] = {
        0xa2u,
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','2'
    };
    static const uint8_t cursor[] = {
        0xa3u,
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
        0x68u, 'p','o','s','i','t','i','o','n', 0x05u
    };
    static const uint8_t snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u,
        0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    static const uint8_t invalidate_v2[] = {
        0xa2u,
        0x67u, 'v','e','r','s','i','o','n', 0x02u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1'
    };
    static const uint8_t stale_snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x00u,
        0x68u, 's','e','q','u','e','n','c','e', 0x03u
    };
    static const uint8_t lines[] = {
        0xa4u,
        0x65u, 'l','i','n','e','s', 0x81u,
          0xa1u, 0x65u, 'b','y','t','e','s', 0x43u, 'a','b','c',
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
        0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u
    };
    static const uint8_t lines_wrong_window[] = {
        0xa4u,
        0x65u, 'l','i','n','e','s', 0x81u,
          0xa1u, 0x65u, 'b','y','t','e','s', 0x43u, 'w','r','g',
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
        0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x01u
    };
    static const uint8_t response_flush[] = {
        0xa2u,
        0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
        0x68u, 's','e','q','u','e','n','c','e', 0x02u
    };
    static const uint8_t lines_v1_replacement[] = {
        0xa4u,
        0x65u, 'l','i','n','e','s', 0x81u,
          0xa1u, 0x65u, 'b','y','t','e','s', 0x43u, 'x','y','z',
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
        0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u
    };
    static const uint8_t response_flush_v4[] = {
        0xa2u,
        0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
        0x68u, 's','e','q','u','e','n','c','e', 0x04u
    };
    static const uint8_t equal_snapshot_flush[] = {
        0xa3u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 'r','e','v','i','s','i','o','n', 0x01u,
        0x68u, 's','e','q','u','e','n','c','e', 0x05u
    };
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_model_action action;
    cdin_frontend_protocol_message_view view;

    /* The attach viewport ID is not a Runtime semantic view ID. The
     * first view-scoped snapshot message establishes the actual view. */
    assert(cdin_frontend_gui_sdl_model_init(&model, "", 16u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.view_id[0] == '\0');
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                   invalidate, sizeof(invalidate));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(strcmp(model.view_id, "view-1") == 0);
    /* Snapshot messages are staged; none of their state is visible before
     * the matching ui.flush commits the whole group. */
    assert(model.version == 0u);
    assert(model.snapshot_pending_valid != 0);
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                   other_view_invalidate, sizeof(other_view_invalidate));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    assert(strcmp(model.view_id, "view-1") == 0);
    view = message(CDIN_FRONTEND_PROTOCOL_UI_CURSOR, cursor, sizeof(cursor));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.cursor_position == 0u);
    assert(model.has_cursor == 0);

    view = message(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT, pane_layout, sizeof(pane_layout));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.has_layout == 0);
    view = message(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                   invalid_pane_layout, sizeof(invalid_pane_layout));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_INVALID);
    assert(model.staged_snapshot.pane_tree.node_count == 0u);
    view = message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                   snapshot_flush, sizeof(snapshot_flush));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(action.acknowledge_sequence == 1u);
    assert(model.version == 1u);
    assert(model.has_cursor != 0);
    assert(model.cursor_position == 5u);
    assert(model.snapshot_pending_valid == 0);
    assert(model.has_layout != 0);
    assert(model.pane_tree.has_root != 0);
    assert(model.pane_tree.has_active_leaf != 0);
    assert(strcmp(model.pane_tree.active_leaf, "leaf-a") == 0);
    assert(model.pane_tree.node_count == 3u);
    assert(model.pane_tree.nodes[0].kind == CDIN_FRONTEND_GUI_SDL_PANE_SPLIT);
    assert(model.pane_tree.nodes[0].axis == CDIN_FRONTEND_GUI_SDL_PANE_VERTICAL);
    assert(model.pane_tree.nodes[0].ratio_bp == 5000u);
    assert(model.pane_tree.nodes[1].kind == CDIN_FRONTEND_GUI_SDL_PANE_LEAF);
    assert(model.pane_tree.nodes[1].view_count == 1u);
    assert(strcmp(model.pane_tree.nodes[1].views[0], "view-1") == 0);
    assert(model.pane_tree.nodes[2].kind == CDIN_FRONTEND_GUI_SDL_PANE_LEAF);
    assert(model.pane_tree.nodes[2].has_active_view == 0);
    assert(action.request_lines != 0);
    assert(action.version == 1u);
    assert(model.snapshot_revision == 1u);
    assert(model.has_snapshot_revision != 0);

    assert(cdin_frontend_gui_sdl_model_expect_lines(&model, 0u, 16u, 1u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                   lines_wrong_window, sizeof(lines_wrong_window));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    assert(model.pending_lines_valid == 0);
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_LINES, lines, sizeof(lines));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.visible.count == 0u); /* response stays staged until flush */
    view = message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                   response_flush, sizeof(response_flush));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(action.acknowledge_sequence == 2u);
    assert(model.visible.count == 1u);
    assert(model.visible.first_line == 0u);
    assert(model.visible.lines[0].length == 3u);
    assert(memcmp(model.visible.lines[0].bytes, "abc", 3u) == 0);
    assert(model.needs_lines == 0);

    /* A version-pinned response may already be staged while a newer
     * snapshot is in flight. If that snapshot is rejected as stale, the
     * response still belongs to the committed v1 model and must survive. */
    assert(cdin_frontend_gui_sdl_model_expect_lines(&model, 0u, 16u, 1u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                   lines_v1_replacement, sizeof(lines_v1_replacement));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.pending_lines_valid != 0);
    assert(memcmp(model.pending.lines[0].bytes, "xyz", 3u) == 0);

    /* A newer document version may be staged, but an older snapshot revision
     * must not replace committed data or its line cache. Its flush remains
     * acknowledgeable so the Backend can release the stale queue group. */
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                   invalidate_v2, sizeof(invalidate_v2));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.version == 1u);
    assert(model.pending_lines_valid != 0);
    view = message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                   stale_snapshot_flush, sizeof(stale_snapshot_flush));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    assert(action.acknowledge_sequence == 3u);
    assert(model.version == 1u);
    assert(model.snapshot_revision == 1u);
    assert(model.snapshot_pending_valid == 0);
    assert(model.pending_lines_valid != 0);

    view = message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                   response_flush_v4, sizeof(response_flush_v4));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(action.acknowledge_sequence == 4u);
    assert(model.visible.count == 1u);
    assert(model.visible.lines[0].length == 3u);
    assert(memcmp(model.visible.lines[0].bytes, "xyz", 3u) == 0);

    /* Snapshot revision equality is not a new revision and must not commit. */
    view = message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                   equal_snapshot_flush, sizeof(equal_snapshot_flush));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_STALE);
    assert(action.acknowledge_sequence == 5u);
    assert(model.snapshot_revision == 1u);

    return 0;
}
