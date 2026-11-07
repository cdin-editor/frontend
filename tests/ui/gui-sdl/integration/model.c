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
    static const uint8_t layout_split[] = {
        0xa2u,
        0x68u, 'r','e','v','i','s','i','o','n', 0x09u,
        0x66u, 'l','a','y','o','u','t',
        0xa2u,
        0x64u, 'r','o','o','t',
        0xa6u,
          0x64u, 'k','i','n','d', 0x65u, 's','p','l','i','t',
          0x62u, 'i','d', 0x67u, 's','p','l','i','t','-','1',
          0x64u, 'a','x','i','s', 0x68u, 'v','e','r','t','i','c','a','l',
          0x68u, 'r','a','t','i','o','_','b','p', 0x19u, 0x13u, 0x88u,
          0x65u, 'f','i','r','s','t',
          0xa4u,
            0x64u, 'k','i','n','d', 0x64u, 'l','e','a','f',
            0x62u, 'i','d', 0x66u, 'l','e','a','f','-','1',
            0x65u, 'v','i','e','w','s', 0x81u, 0x66u, 'v','i','e','w','-','1',
            0x6bu, 'a','c','t','i','v','e','_','v','i','e','w', 0x66u, 'v','i','e','w','-','1',
          0x66u, 's','e','c','o','n','d',
          0xa4u,
            0x64u, 'k','i','n','d', 0x64u, 'l','e','a','f',
            0x62u, 'i','d', 0x66u, 'l','e','a','f','-','2',
            0x65u, 'v','i','e','w','s', 0x81u, 0x66u, 'v','i','e','w','-','2',
            0x6bu, 'a','c','t','i','v','e','_','v','i','e','w', 0x66u, 'v','i','e','w','-','2',
        0x6bu, 'a','c','t','i','v','e','_','l','e','a','f', 0x66u, 'l','e','a','f','-','1'
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
    view = message(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
                   layout_split, sizeof(layout_split));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    /* PaneTree is part of the atomic snapshot: it is parsed while staged but
     * cannot replace the committed layout before the matching ui.flush. */
    assert(model.has_layout == 0);
    assert(model.staged_snapshot.has_layout != 0);
    assert(model.staged_snapshot.layout.node_count == 3u);
    view = message(CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                   snapshot_flush, sizeof(snapshot_flush));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(action.acknowledge_sequence == 1u);
    assert(model.version == 1u);
    assert(model.has_cursor != 0);
    assert(model.cursor_position == 5u);
    assert(model.snapshot_pending_valid == 0);
    assert(action.request_lines != 0);
    assert(action.version == 1u);
    assert(model.snapshot_revision == 1u);
    assert(model.has_snapshot_revision != 0);
    assert(model.has_layout != 0);
    assert(model.layout.revision == 9u);
    assert(model.layout.has_root != 0);
    assert(model.layout.has_active_leaf != 0);
    assert(model.layout.node_count == 3u);
    assert(model.layout.leaf_count == 2u);
    assert(model.layout.view_count == 2u);
    assert(model.layout.nodes[model.layout.root_index].kind ==
           CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT);
    assert(model.layout.nodes[model.layout.root_index].axis ==
           CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL);
    assert(model.layout.nodes[model.layout.root_index].ratio_bp == 5000u);
    assert(strcmp(model.layout.nodes[model.layout.active_leaf_index].id,
                  "leaf-1") == 0);

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
