#include "model.h"

#include <stdio.h>
#include <string.h>

static cdin_frontend_protocol_status get_text(
    const cdin_frontend_protocol_message_view *message,
    const char *field, char *output, size_t capacity)
{
    size_t length = 0u;
    cdin_frontend_protocol_status status;
    if (capacity == 0u) return CDIN_FRONTEND_PROTOCOL_INVALID_ARGUMENT;
    status = cdin_frontend_protocol_payload_get_text(
        message->payload, message->payload_length, field,
        output, capacity, &length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) output[0] = '\0';
    return status;
}


static int raw_is_null(const uint8_t *raw, size_t length)
{
    return raw != NULL && length == 1u && raw[0] == 0xf6u;
}

static int copy_text_field(const uint8_t *map, size_t map_length,
                           const char *field, char *output, size_t capacity)
{
    size_t length = 0u;
    return cdin_frontend_protocol_payload_get_text(
        map, map_length, field, output, capacity, &length) ==
        CDIN_FRONTEND_PROTOCOL_OK && length != 0u && length < capacity;
}

static int pane_view_exists(const cdin_frontend_gui_sdl_pane_node *node,
                            const char *view_id)
{
    size_t index;
    for (index = 0u; index < node->view_count; ++index) {
        if (strcmp(node->views[index], view_id) == 0) return 1;
    }
    return 0;
}

static int parse_pane_node(cdin_frontend_gui_sdl_pane_tree *tree,
                           const uint8_t *map, size_t map_length,
                           unsigned depth, int *out_index)
{
    cdin_frontend_gui_sdl_pane_node *node;
    char kind[16];
    size_t index;
    int node_index;
    if (tree == NULL || map == NULL || out_index == NULL ||
        depth >= CDIN_FRONTEND_GUI_SDL_MAX_PANE_DEPTH ||
        tree->node_count >= CDIN_FRONTEND_GUI_SDL_MAX_PANE_NODES ||
        !copy_text_field(map, map_length, "kind", kind, sizeof(kind))) return 0;

    node_index = (int)tree->node_count++;
    node = &tree->nodes[node_index];
    memset(node, 0, sizeof(*node));
    node->first = -1;
    node->second = -1;
    if (!copy_text_field(map, map_length, "id", node->id, sizeof(node->id))) {
        return 0;
    }
    for (index = 0u; index < (size_t)node_index; ++index) {
        if (strcmp(tree->nodes[index].id, node->id) == 0) return 0;
    }

    if (strcmp(kind, "leaf") == 0) {
        const uint8_t *views = NULL;
        size_t views_length = 0u;
        size_t view_count = 0u;
        const uint8_t *active = NULL;
        size_t active_length = 0u;
        if (cdin_frontend_protocol_payload_get_raw(
                map, map_length, "views", &views, &views_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_array_count(views, views_length, &view_count) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            view_count > CDIN_FRONTEND_GUI_SDL_MAX_PANE_VIEWS) return 0;
        node->kind = CDIN_FRONTEND_GUI_SDL_PANE_LEAF;
        node->view_count = view_count;
        for (index = 0u; index < view_count; ++index) {
            size_t view_length = 0u;
            size_t previous;
            if (cdin_frontend_protocol_array_get_text(
                    views, views_length, index, node->views[index],
                    sizeof(node->views[index]), &view_length) !=
                    CDIN_FRONTEND_PROTOCOL_OK || view_length == 0u) return 0;
            for (previous = 0u; previous < index; ++previous) {
                if (strcmp(node->views[previous], node->views[index]) == 0) return 0;
            }
        }
        if (cdin_frontend_protocol_payload_get_raw(
                map, map_length, "active_view", &active, &active_length) !=
                CDIN_FRONTEND_PROTOCOL_OK) return 0;
        if (!raw_is_null(active, active_length)) {
            if (!copy_text_field(map, map_length, "active_view",
                                 node->active_view, sizeof(node->active_view)) ||
                !pane_view_exists(node, node->active_view)) return 0;
            node->has_active_view = 1;
        }
    } else if (strcmp(kind, "split") == 0) {
        const uint8_t *first = NULL;
        const uint8_t *second = NULL;
        size_t first_length = 0u;
        size_t second_length = 0u;
        char axis[16];
        uint64_t ratio = 0u;
        int first_index;
        int second_index;
        if (!copy_text_field(map, map_length, "axis", axis, sizeof(axis)) ||
            cdin_frontend_protocol_payload_get_uint(
                map, map_length, "ratio_bp", &ratio) != CDIN_FRONTEND_PROTOCOL_OK ||
            ratio < 1000u || ratio > 8999u ||
            cdin_frontend_protocol_payload_get_raw(
                map, map_length, "first", &first, &first_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_raw(
                map, map_length, "second", &second, &second_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            (strcmp(axis, "horizontal") != 0 && strcmp(axis, "vertical") != 0)) return 0;
        node->kind = CDIN_FRONTEND_GUI_SDL_PANE_SPLIT;
        node->axis = strcmp(axis, "horizontal") == 0
            ? CDIN_FRONTEND_GUI_SDL_PANE_HORIZONTAL
            : CDIN_FRONTEND_GUI_SDL_PANE_VERTICAL;
        node->ratio_bp = (uint16_t)ratio;
        if (!parse_pane_node(tree, first, first_length, depth + 1u, &first_index) ||
            !parse_pane_node(tree, second, second_length, depth + 1u, &second_index)) return 0;
        node = &tree->nodes[node_index];
        node->first = first_index;
        node->second = second_index;
    } else {
        return 0;
    }
    *out_index = node_index;
    return 1;
}

static int parse_pane_tree(const uint8_t *map, size_t map_length,
                           cdin_frontend_gui_sdl_pane_tree *tree)
{
    const uint8_t *root = NULL;
    const uint8_t *active_leaf = NULL;
    size_t root_length = 0u;
    size_t active_length = 0u;
    size_t index;
    int root_index = -1;
    if (map == NULL || tree == NULL) return 0;
    memset(tree, 0, sizeof(*tree));
    tree->root_index = -1;
    if (cdin_frontend_protocol_payload_get_raw(
            map, map_length, "root", &root, &root_length) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            map, map_length, "active_leaf", &active_leaf, &active_length) !=
            CDIN_FRONTEND_PROTOCOL_OK) return 0;
    if (!raw_is_null(root, root_length)) {
        if (!parse_pane_node(tree, root, root_length, 0u, &root_index)) return 0;
        tree->has_root = 1;
        tree->root_index = root_index;
    }
    if (!raw_is_null(active_leaf, active_length)) {
        if (!copy_text_field(map, map_length, "active_leaf",
                             tree->active_leaf, sizeof(tree->active_leaf))) return 0;
        tree->has_active_leaf = 1;
    }
    if (tree->has_root != tree->has_active_leaf) return 0;
    if (tree->has_active_leaf) {
        int found = 0;
        for (index = 0u; index < tree->node_count; ++index) {
            const cdin_frontend_gui_sdl_pane_node *node = &tree->nodes[index];
            if (node->kind == CDIN_FRONTEND_GUI_SDL_PANE_LEAF &&
                strcmp(node->id, tree->active_leaf) == 0) found = 1;
        }
        if (!found) return 0;
    }
    /* Protocol v1 bounds the whole semantic snapshot to 32 views, not
     * 32 views per leaf. Count globally before accepting the tree. */
    {
        size_t total_views = 0u;
        for (index = 0u; index < tree->node_count; ++index) {
            if (tree->nodes[index].kind == CDIN_FRONTEND_GUI_SDL_PANE_LEAF) {
                if (tree->nodes[index].view_count >
                    CDIN_FRONTEND_GUI_SDL_MAX_PANE_VIEWS - total_views) return 0;
                total_views += tree->nodes[index].view_count;
            }
        }
    }
    /* A semantic view belongs to exactly one leaf in a PaneTree. */
    for (index = 0u; index < tree->node_count; ++index) {
        size_t other;
        size_t view;
        const cdin_frontend_gui_sdl_pane_node *node = &tree->nodes[index];
        if (node->kind != CDIN_FRONTEND_GUI_SDL_PANE_LEAF) continue;
        for (other = index + 1u; other < tree->node_count; ++other) {
            const cdin_frontend_gui_sdl_pane_node *candidate = &tree->nodes[other];
            if (candidate->kind != CDIN_FRONTEND_GUI_SDL_PANE_LEAF) continue;
            for (view = 0u; view < node->view_count; ++view) {
                if (pane_view_exists(candidate, node->views[view])) return 0;
            }
        }
    }
    return 1;
}

static cdin_frontend_gui_sdl_model_status apply_layout(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *layout = NULL;
    size_t layout_length = 0u;
    uint64_t revision = 0u;
    cdin_frontend_gui_sdl_pane_tree parsed;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "layout",
            &layout, &layout_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        !parse_pane_tree(layout, layout_length, &parsed)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    if (revision < model->staged_snapshot.layout_revision) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    model->staged_snapshot.layout_revision = revision;
    model->staged_snapshot.has_layout = 1;
    model->staged_snapshot.pane_tree = parsed;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static int same_view(cdin_frontend_gui_sdl_model *model,
                     const cdin_frontend_protocol_message_view *message)
{
    char view_id[64];
    size_t length;
    if (get_text(message, "view_id", view_id, sizeof(view_id)) !=
        CDIN_FRONTEND_PROTOCOL_OK) return 0;
    if (model->view_id[0] == '\0') {
        /* ATTACH identifies the Frontend viewport, not a Runtime document
         * view. Bind the single-view SDL baseline to the first semantic view
         * actually published by Runtime instead of assuming both IDs match. */
        length = strlen(view_id);
        if (length == 0u || length >= sizeof(model->view_id)) return 0;
        memcpy(model->view_id, view_id, length + 1u);
        return 1;
    }
    return strcmp(view_id, model->view_id) == 0;
}

static void snapshot_begin(cdin_frontend_gui_sdl_model *model)
{
    cdin_frontend_gui_sdl_snapshot_state *staged;
    if (model->snapshot_pending_valid) return;
    staged = &model->staged_snapshot;
    memset(staged, 0, sizeof(*staged));
    staged->version = model->version;
    staged->cursor_position = model->cursor_position;
    staged->layout_revision = model->layout_revision;
    staged->has_cursor = model->has_cursor;
    staged->has_layout = model->has_layout;
    staged->needs_lines = model->needs_lines;
    memset(&staged->pane_tree, 0, sizeof(staged->pane_tree));
    staged->pane_tree.root_index = -1;
    memcpy(staged->theme, model->theme, sizeof(staged->theme));
    memcpy(staged->mode, model->mode, sizeof(staged->mode));
    memcpy(staged->status, model->status, sizeof(staged->status));
    memcpy(staged->notification, model->notification, sizeof(staged->notification));
    model->snapshot_pending_valid = 1;
}

static void snapshot_commit(cdin_frontend_gui_sdl_model *model)
{
    const cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    int version_changed = staged->version != model->version;
    model->version = staged->version;
    model->cursor_position = staged->cursor_position;
    model->layout_revision = staged->layout_revision;
    model->has_cursor = staged->has_cursor;
    model->has_layout = staged->has_layout;
    model->needs_lines = staged->needs_lines;
    model->pane_tree = staged->pane_tree;
    memcpy(model->theme, staged->theme, sizeof(model->theme));
    memcpy(model->mode, staged->mode, sizeof(model->mode));
    memcpy(model->status, staged->status, sizeof(model->status));
    memcpy(model->notification, staged->notification, sizeof(model->notification));
    if (version_changed) {
        memset(&model->visible, 0, sizeof(model->visible));
        model->pending_lines_valid = 0;
        model->has_line_request = 0;
        model->needs_lines = model->version != 0u;
    }
    model->snapshot_pending_valid = 0;
}

static cdin_frontend_gui_sdl_model_status apply_invalidate(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    uint64_t version = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
        CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < staged->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (version != staged->version) {
        staged->has_cursor = 0;
    }
    staged->version = version;
    staged->needs_lines = 1;
    /* Do not discard an independently staged response here. Invalidation is
     * not visible until its snapshot ui.flush commits; the response may still
     * belong to the currently committed document version. snapshot_commit()
     * invalidates the line cache only after a newer version is committed. */
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_cursor(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    uint64_t version = 0u;
    uint64_t position = 0u;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "position", &position) !=
            CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version < staged->version) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    staged->cursor_position = position;
    staged->has_cursor = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_lines(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *array = NULL;
    size_t array_length = 0u;
    size_t count = 0u;
    uint64_t version = 0u;
    uint64_t first_line = 0u;
    size_t index;
    if (!same_view(model, message)) return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "version", &version) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "first_line", &first_line) !=
            CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "lines",
            &array, &array_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_array_count(array, array_length, &count) !=
            CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    if (version != model->version || version == 0u ||
        model->has_line_request == 0 ||
        version != model->requested_version ||
        first_line != model->requested_first_line ||
        count > model->requested_line_count) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
    }
    if (count > model->visible_line_capacity ||
        count > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
    }

    memset(&model->pending, 0, sizeof(model->pending));
    model->pending.first_line = first_line;
    model->pending.count = count;
    for (index = 0u; index < count; ++index) {
        const uint8_t *line_map = NULL;
        size_t line_map_length = 0u;
        const uint8_t *bytes = NULL;
        size_t bytes_length = 0u;
        if (cdin_frontend_protocol_array_get_raw(
                array, array_length, index, &line_map, &line_map_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_bytes(
                line_map, line_map_length, "bytes", &bytes, &bytes_length) !=
                CDIN_FRONTEND_PROTOCOL_OK) return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        if (bytes_length > CDIN_FRONTEND_GUI_SDL_MAX_LINE_BYTES) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_LIMIT;
        }
        if (bytes_length != 0u) {
            memcpy(model->pending.lines[index].bytes, bytes, bytes_length);
        }
        model->pending.lines[index].length = bytes_length;
    }
    model->pending_lines_valid = 1;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static void optional_text(const cdin_frontend_protocol_message_view *message,
                          const char *field, char *output, size_t capacity)
{
    if (get_text(message, field, output, capacity) != CDIN_FRONTEND_PROTOCOL_OK) {
        output[0] = '\0';
    }
}

static cdin_frontend_gui_sdl_model_status apply_status(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message)
{
    const uint8_t *map = NULL;
    size_t map_length = 0u;
    cdin_frontend_gui_sdl_snapshot_state *staged = &model->staged_snapshot;
    if (cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "status",
            &map, &map_length) == CDIN_FRONTEND_PROTOCOL_OK) {
        size_t length = 0u;
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "mode", staged->mode,
                sizeof(staged->mode), &length) != CDIN_FRONTEND_PROTOCOL_OK) {
            staged->mode[0] = '\0';
        }
        if (cdin_frontend_protocol_payload_get_text(
                map, map_length, "text", staged->status,
                sizeof(staged->status), &length) != CDIN_FRONTEND_PROTOCOL_OK) {
            staged->status[0] = '\0';
        }
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

static cdin_frontend_gui_sdl_model_status apply_flush(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_model_action *action)
{
    char scope[16];
    size_t scope_length = 0u;
    uint64_t sequence = 0u;
    uint64_t revision = 0u;
    if (cdin_frontend_protocol_payload_get_text(
            message->payload, message->payload_length, "scope",
            scope, sizeof(scope), &scope_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "sequence", &sequence) !=
            CDIN_FRONTEND_PROTOCOL_OK || sequence == 0u) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    action->acknowledge_sequence = sequence;
    if (scope_length == 8u && memcmp(scope, "snapshot", 8u) == 0) {
        if (cdin_frontend_protocol_payload_get_uint(
                message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        if (revision == 0u ||
            (model->has_snapshot_revision != 0 &&
             revision <= model->snapshot_revision)) {
            model->snapshot_pending_valid = 0;
            memset(&model->staged_snapshot, 0, sizeof(model->staged_snapshot));
            return CDIN_FRONTEND_GUI_SDL_MODEL_STALE;
        }
        if (model->snapshot_pending_valid) snapshot_commit(model);
        model->snapshot_revision = revision;
        model->has_snapshot_revision = 1;
        if (model->needs_lines && model->version != 0u) {
            action->request_lines = 1;
            action->first_line = 0u;
            action->line_count = (uint64_t)model->visible_line_capacity;
            action->version = model->version;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    if (scope_length == 8u && memcmp(scope, "response", 8u) == 0) {
        if (cdin_frontend_protocol_payload_get_uint(
                message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_UNSUPPORTED) {
            return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
        }
        if (model->pending_lines_valid) {
            model->visible = model->pending;
            model->pending_lines_valid = 0;
            model->has_line_request = 0;
            model->needs_lines = 0;
        }
        return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    }
    return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_init(
    cdin_frontend_gui_sdl_model *model,
    const char *view_id,
    size_t visible_line_capacity)
{
    size_t length;
    if (model == NULL || view_id == NULL || visible_line_capacity == 0u ||
        visible_line_capacity > CDIN_FRONTEND_GUI_SDL_MAX_VISIBLE_LINES) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    length = strlen(view_id);
    if (length >= sizeof(model->view_id)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    memset(model, 0, sizeof(*model));
    if (length != 0u) memcpy(model->view_id, view_id, length + 1u);
    model->visible_line_capacity = visible_line_capacity;
    model->needs_lines = 1;
    model->pane_tree.root_index = -1;
    (void)snprintf(model->theme, sizeof(model->theme), "default");
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_expect_lines(
    cdin_frontend_gui_sdl_model *model,
    uint64_t first_line,
    uint64_t line_count,
    uint64_t version)
{
    if (model == NULL || version == 0u || version != model->version ||
        line_count == 0u ||
        line_count > (uint64_t)model->visible_line_capacity ||
        first_line > UINT64_MAX - line_count) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    model->requested_first_line = first_line;
    model->requested_line_count = line_count;
    model->requested_version = version;
    model->has_line_request = 1;
    model->pending_lines_valid = 0;
    return CDIN_FRONTEND_GUI_SDL_MODEL_OK;
}

cdin_frontend_gui_sdl_model_status cdin_frontend_gui_sdl_model_apply(
    cdin_frontend_gui_sdl_model *model,
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_model_action *out_action)
{
    cdin_frontend_gui_sdl_model_status status = CDIN_FRONTEND_GUI_SDL_MODEL_OK;
    if (model == NULL || message == NULL || out_action == NULL ||
        (message->payload == NULL && message->payload_length != 0u)) {
        return CDIN_FRONTEND_GUI_SDL_MODEL_INVALID;
    }
    memset(out_action, 0, sizeof(*out_action));
    if (message->type != CDIN_FRONTEND_PROTOCOL_UI_FLUSH &&
        message->type != CDIN_FRONTEND_PROTOCOL_VIEW_LINES) {
        snapshot_begin(model);
    }
    switch (message->type) {
        case CDIN_FRONTEND_PROTOCOL_VIEW_INVALIDATE:
            status = apply_invalidate(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_VIEW_LINES:
            status = apply_lines(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_CURSOR:
            status = apply_cursor(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_SELECTION:
            /* Selection is Runtime-owned state. The current GUI baseline does
             * not render it, so it deliberately keeps no selection cache. */
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_STATUSLINE:
            status = apply_status(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_OPTIONS:
            optional_text(message, "theme", model->staged_snapshot.theme,
                          sizeof(model->staged_snapshot.theme));
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_LAYOUT:
            status = apply_layout(model, message);
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_NOTIFY:
            optional_text(message, "text", model->staged_snapshot.notification,
                          sizeof(model->staged_snapshot.notification));
            break;
        case CDIN_FRONTEND_PROTOCOL_UI_FLUSH:
            status = apply_flush(model, message, out_action);
            break;
        default:
            break;
    }
    return status;
}
