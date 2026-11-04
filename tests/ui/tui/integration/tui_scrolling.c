#include "cdin/frontend/ui/tui/model.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const uint8_t cursor_at_byte_ten[] = {
    0xa4u,
    0x67u, 'v','e','r','s','i','o','n', 0x02u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x68u, 'p','o','s','i','t','i','o','n', 0x0au,
    0x6du, 's','c','r','o','l','l','_','a','n','c','h','o','r', 0x0au
};

static const uint8_t invalidate_v2[] = {
    0xa3u,
    0x66u, 'r','a','n','g','e','s', 0x80u,
    0x67u, 'v','e','r','s','i','o','n', 0x02u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1'
};

static const uint8_t snapshot_flush[] = {
    0xa3u,
    0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
    0x68u, 'r','e','v','i','s','i','o','n', 0x01u,
    0x68u, 's','e','q','u','e','n','c','e', 0x01u
};

static const uint8_t lines_from_start[] = {
    0xa5u,
    0x65u, 'l','i','n','e','s', 0x83u,
        0xa1u, 0x65u, 'b','y','t','e','s', 0x44u, 'a','b','c','\n',
        0xa1u, 0x65u, 'b','y','t','e','s', 0x46u, 'd','e','f','g','h','\n',
        0xa1u, 0x65u, 'b','y','t','e','s', 0x44u, 'i','j','k','l',
    0x67u, 'v','e','r','s','i','o','n', 0x02u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u,
    0x6bu, 't','o','t','a','l','_','l','i','n','e','s', 0x03u
};

static const uint8_t response_flush_2[] = {
    0xa2u,
    0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
    0x68u, 's','e','q','u','e','n','c','e', 0x02u
};

static const uint8_t final_line[] = {
    0xa5u,
    0x65u, 'l','i','n','e','s', 0x81u,
        0xa1u, 0x65u, 'b','y','t','e','s', 0x44u, 'i','j','k','l',
    0x67u, 'v','e','r','s','i','o','n', 0x02u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x02u,
    0x6bu, 't','o','t','a','l','_','l','i','n','e','s', 0x03u
};

static const uint8_t response_flush_3[] = {
    0xa2u,
    0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
    0x68u, 's','e','q','u','e','n','c','e', 0x03u
};

static void apply(cdin_frontend_tui_model *model,
                  cdin_frontend_protocol_message_type type,
                  const uint8_t *payload, size_t payload_length,
                  int expected_commit)
{
    cdin_frontend_protocol_message_view message;
    int committed = 0;
    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.sequence = 1u;
    message.type = type;
    message.name_space = "core";
    message.payload = payload;
    message.payload_length = payload_length;
    assert(cdin_frontend_protocol_validate_payload(type, payload, payload_length) ==
           CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_tui_model_apply_message(model, &message, &committed) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    assert(committed == expected_commit);
}

int main(void)
{
    cdin_frontend_tui_model *model = cdin_frontend_tui_model_create("view-1", 24u, NULL);
    cdin_frontend_tui_frame frame;
    uint64_t first_line;
    uint64_t line_count;
    uint64_t version;
    assert(model != NULL);

    /* Snapshot says the visible top anchor is byte 10, not logical line 10. */
    apply(model, CDIN_FRONTEND_PROTOCOL_UI_CURSOR,
          cursor_at_byte_ten, sizeof(cursor_at_byte_ten), 0);
    apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
          invalidate_v2, sizeof(invalidate_v2), 0);
    apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
          snapshot_flush, sizeof(snapshot_flush), 1);

    assert(cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    assert(first_line == 0u && line_count == 256u && version == 2u);
    cdin_frontend_tui_model_mark_request_pending(model);

    /* Runtime lines preserve their original LF bytes, so the byte anchor maps
     * to the third logical line without asking the Backend to own visual layout. */
    apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
          lines_from_start, sizeof(lines_from_start), 0);
    apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
          response_flush_2, sizeof(response_flush_2), 1);
    assert(cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    assert(first_line == 2u && line_count == 24u && version == 2u);
    cdin_frontend_tui_model_mark_request_pending(model);

    apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
          final_line, sizeof(final_line), 0);
    apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
          response_flush_3, sizeof(response_flush_3), 1);
    assert(!cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_get_frame(model, &frame) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(frame.top_line == 2u && frame.line_count == 1u);
    assert(frame.lines[0].length == 4u && memcmp(frame.lines[0].bytes, "ijkl", 4u) == 0);
    assert(frame.cursor_line == 2u && frame.cursor_byte == 0u);

    cdin_frontend_tui_model_destroy(model);
    return 0;
}
