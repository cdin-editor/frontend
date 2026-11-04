#include "cdin/frontend/ui/tui/model.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const uint8_t options_payload[] = {
    0xa3u,
    0x65u, 't','h','e','m','e', 0x64u, 'd','a','r','k',
    0x67u, 'o','p','t','i','o','n','s', 0xa0u,
    0x68u, 'r','e','v','i','s','i','o','n', 0x01u
};

static const uint8_t cursor_payload[] = {
    0xa4u,
    0x67u, 'v','e','r','s','i','o','n', 0x01u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x68u, 'p','o','s','i','t','i','o','n', 0x02u,
    0x6du, 's','c','r','o','l','l','_','a','n','c','h','o','r', 0x00u
};

static const uint8_t selection_payload[] = {
    0xa4u,
    0x65u, 'c','a','r','e','t', 0x04u,
    0x66u, 'a','n','c','h','o','r', 0x01u,
    0x67u, 'v','e','r','s','i','o','n', 0x01u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1'
};

static const uint8_t lines_payload[] = {
    0xa4u,
    0x65u, 'l','i','n','e','s', 0x81u,
        0xa1u, 0x65u, 'b','y','t','e','s', 0x45u, 'H','e','l','l','o',
    0x67u, 'v','e','r','s','i','o','n', 0x01u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u
};

static const uint8_t snapshot_flush[] = {
    0xa3u,
    0x65u, 's','c','o','p','e', 0x68u, 's','n','a','p','s','h','o','t',
    0x68u, 'r','e','v','i','s','i','o','n', 0x01u,
    0x68u, 's','e','q','u','e','n','c','e', 0x01u
};

static const uint8_t stale_lines_payload[] = {
    0xa4u,
    0x65u, 'l','i','n','e','s', 0x81u,
        0xa1u, 0x65u, 'b','y','t','e','s', 0x43u, 'o','l','d',
    0x67u, 'v','e','r','s','i','o','n', 0x00u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u
};

static const uint8_t unexpected_future_lines_payload[] = {
    0xa4u,
    0x65u, 'l','i','n','e','s', 0x81u,
        0xa1u, 0x65u, 'b','y','t','e','s', 0x43u, 'n','e','w',
    0x67u, 'v','e','r','s','i','o','n', 0x03u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1',
    0x6au, 'f','i','r','s','t','_','l','i','n','e', 0x00u
};

static const uint8_t response_flush_2[] = {
    0xa2u,
    0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
    0x68u, 's','e','q','u','e','n','c','e', 0x02u
};

static const uint8_t invalidate_payload[] = {
    0xa3u,
    0x66u, 'r','a','n','g','e','s', 0x80u,
    0x67u, 'v','e','r','s','i','o','n', 0x02u,
    0x67u, 'v','i','e','w','_','i','d', 0x66u, 'v','i','e','w','-','1'
};

static const uint8_t response_flush_3[] = {
    0xa2u,
    0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
    0x68u, 's','e','q','u','e','n','c','e', 0x03u
};

static const uint8_t response_flush_4[] = {
    0xa2u,
    0x65u, 's','c','o','p','e', 0x68u, 'r','e','s','p','o','n','s','e',
    0x68u, 's','e','q','u','e','n','c','e', 0x04u
};

static cdin_frontend_tui_model_status apply(
    cdin_frontend_tui_model *model,
    cdin_frontend_protocol_message_type type,
    const uint8_t *payload,
    size_t payload_length,
    int expected_commit)
{
    cdin_frontend_protocol_message_view message;
    cdin_frontend_tui_model_status status;
    int committed = 0;
    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.sequence = 1u;
    message.type = type;
    message.name_space = "core";
    message.payload = payload;
    message.payload_length = payload_length;
    {
        cdin_frontend_protocol_status validation = cdin_frontend_protocol_validate_payload(type, payload, payload_length);
        if (validation != CDIN_FRONTEND_PROTOCOL_OK) {
            (void)fprintf(stderr, "invalid %s payload: %s\n",
                          cdin_frontend_protocol_message_name(type),
                          cdin_frontend_protocol_status_name(validation));
        }
        assert(validation == CDIN_FRONTEND_PROTOCOL_OK);
    }
    status = cdin_frontend_tui_model_apply_message(model, &message, &committed);
    assert(committed == expected_commit);
    return status;
}

int main(void)
{
    cdin_frontend_tui_model *model;
    cdin_frontend_tui_frame frame;
    uint64_t first_line = UINT64_MAX;
    uint64_t line_count = 0u;
    uint64_t version = UINT64_MAX;

    model = cdin_frontend_tui_model_create("view-1", 32u, NULL);
    assert(model != NULL);
    assert(cdin_frontend_tui_model_revision(model) == 0u);
    assert(cdin_frontend_tui_model_needs_lines(model));

    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_OPTIONS,
                 options_payload, sizeof(options_payload), 0) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(cdin_frontend_tui_model_revision(model) == 0u);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_CURSOR,
                 cursor_payload, sizeof(cursor_payload), 0) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_SELECTION,
                 selection_payload, sizeof(selection_payload), 0) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                 snapshot_flush, sizeof(snapshot_flush), 1) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    assert(first_line == 0u && line_count == 32u && version == 1u);
    cdin_frontend_tui_model_mark_request_pending(model);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                 lines_payload, sizeof(lines_payload), 0) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                 response_flush_2, sizeof(response_flush_2), 1) == CDIN_FRONTEND_TUI_MODEL_OK);

    assert(cdin_frontend_tui_model_revision(model) == 1u);
    assert(cdin_frontend_tui_model_last_flush(model) == 2u);
    assert(!cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_get_frame(model, &frame) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(frame.line_count == 1u);
    assert(frame.lines[0].length == 5u);
    assert(memcmp(frame.lines[0].bytes, "Hello", 5u) == 0);
    assert(frame.document_version == 1u);
    assert(frame.cursor_line == 0u && frame.cursor_byte == 2u);
    assert(frame.selection_known == 1);
    assert(frame.selection_anchor == 1u && frame.selection_caret == 4u);
    assert(strcmp(frame.mode, "") == 0);

    /* Invalidation discards the old cache and exposes a version-pinned pull. */
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE,
                 invalidate_payload, sizeof(invalidate_payload), 0) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                 response_flush_3, sizeof(response_flush_3), 1) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    assert(first_line == 0u && line_count == 32u && version == 2u);
    cdin_frontend_tui_model_mark_request_pending(model);
    assert(!cdin_frontend_tui_model_needs_lines(model));

    /* A response for a version newer than the requested version is rejected. */
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                 unexpected_future_lines_payload, sizeof(unexpected_future_lines_payload), 0) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                 response_flush_4, sizeof(response_flush_4), 0) ==
           CDIN_FRONTEND_TUI_MODEL_INVALID_MESSAGE);
    assert(cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    cdin_frontend_tui_model_mark_request_pending(model);

    /* A delayed response for the prior document version is ignored atomically. */
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_VIEW_LINES,
                 stale_lines_payload, sizeof(stale_lines_payload), 0) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(apply(model, CDIN_FRONTEND_PROTOCOL_UI_FLUSH,
                 response_flush_4, sizeof(response_flush_4), 1) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(cdin_frontend_tui_model_needs_lines(model));
    assert(cdin_frontend_tui_model_get_frame(model, &frame) == CDIN_FRONTEND_TUI_MODEL_OK);
    assert(frame.document_version == 2u && frame.line_count == 0u);
    assert(cdin_frontend_tui_model_request_range(model, &first_line, &line_count, &version) ==
           CDIN_FRONTEND_TUI_MODEL_OK);
    cdin_frontend_tui_model_mark_request_pending(model);
    cdin_frontend_tui_model_request_failed(model);
    assert(cdin_frontend_tui_model_needs_lines(model));

    cdin_frontend_tui_model_destroy(model);
    return 0;
}
