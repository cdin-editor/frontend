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
    static const uint8_t cursor[] = {
        0xa3u,
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
        0x68u, 'p','o','s','i','t','i','o','n', 0x05u
    };
    static const uint8_t snapshot_flush[] = {
        0xa2u,
        0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
        0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    static const uint8_t lines[] = {
        0xa4u,
        0x65u, 'l','i','n','e','s', 0x81u,
          0xa1u, 0x65u, 'b','y','t','e','s', 0x43u, 'a','b','c',
        0x67u, 'v','e','r','s','i','o','n', 0x01u,
        0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
        0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u
    };
    static const uint8_t response_flush[] = {
        0xa2u,
        0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
        0x68u, 's','e','q','u','e','n','c','e', 0x02u
    };
    cdin_frontend_gui_sdl_model model;
    cdin_frontend_gui_sdl_model_action action;
    cdin_frontend_protocol_message_view view;

    assert(cdin_frontend_gui_sdl_model_init(&model, "view-1", 16u) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    view = message(CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                   invalidate, sizeof(invalidate));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    /* Snapshot messages are staged; none of their state is visible before
     * the matching ui.flush commits the whole group. */
    assert(model.version == 0u);
    assert(model.snapshot_pending_valid != 0);
    view = message(CDIN_FRONTEND_PROTOCOL_UI_CURSOR, cursor, sizeof(cursor));
    assert(cdin_frontend_gui_sdl_model_apply(&model, &view, &action) ==
           CDIN_FRONTEND_GUI_SDL_MODEL_OK);
    assert(model.cursor_position == 0u);
    assert(model.has_cursor == 0);
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

    return 0;
}
