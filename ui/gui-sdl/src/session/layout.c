#include "layout.h"

#include <string.h>

#define CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_DEPTH 32u

typedef struct cdin_frontend_gui_sdl_layout_parser {
    cdin_frontend_gui_sdl_layout *layout;
} cdin_frontend_gui_sdl_layout_parser;

static int raw_is_null(const uint8_t *raw, size_t length)
{
    return raw != NULL && length == 1u && raw[0] == 0xf6u;
}

static cdin_frontend_gui_sdl_layout_status required_text(
    const uint8_t *map, size_t map_length, const char *field,
    char *output, size_t capacity)
{
    size_t length = 0u;
    cdin_frontend_protocol_status status =
        cdin_frontend_protocol_payload_get_text(
            map, map_length, field, output, capacity, &length);
    if (status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
    }
    if (status != CDIN_FRONTEND_PROTOCOL_OK || length == 0u) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
}

static cdin_frontend_gui_sdl_layout_status nullable_text(
    const uint8_t *map, size_t map_length, const char *field,
    char *output, size_t capacity, int *out_present)
{
    const uint8_t *raw = NULL;
    size_t raw_length = 0u;
    size_t text_length = 0u;
    cdin_frontend_protocol_status status;
    if (output == NULL || capacity == 0u || out_present == NULL) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    status = cdin_frontend_protocol_payload_get_raw(
        map, map_length, field, &raw, &raw_length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    if (raw_is_null(raw, raw_length)) {
        output[0] = '\0';
        *out_present = 0;
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
    }
    status = cdin_frontend_protocol_payload_get_text(
        map, map_length, field, output, capacity, &text_length);
    if (status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
    }
    if (status != CDIN_FRONTEND_PROTOCOL_OK || text_length == 0u) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    *out_present = 1;
    return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
}

static int identifier_exists(
    const cdin_frontend_gui_sdl_layout *layout, size_t before_count,
    const char *id)
{
    size_t index;
    for (index = 0u; index < before_count; ++index) {
        if (strcmp(layout->nodes[index].id, id) == 0) return 1;
    }
    return 0;
}

static cdin_frontend_gui_sdl_layout_status add_view(
    cdin_frontend_gui_sdl_layout *layout, const char *view_id,
    uint8_t *out_index)
{
    size_t index;
    if (view_id == NULL || view_id[0] == '\0' || out_index == NULL) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    for (index = 0u; index < layout->view_count; ++index) {
        if (strcmp(layout->view_ids[index], view_id) == 0) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
    }
    if (layout->view_count >= CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
    }
    (void)memcpy(layout->view_ids[layout->view_count], view_id, strlen(view_id) + 1u);
    *out_index = (uint8_t)layout->view_count;
    layout->view_count++;
    return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
}

static cdin_frontend_gui_sdl_layout_status parse_node(
    cdin_frontend_gui_sdl_layout_parser *parser,
    const uint8_t *raw, size_t raw_length, size_t depth,
    size_t *out_index)
{
    cdin_frontend_gui_sdl_layout *layout;
    cdin_frontend_gui_sdl_layout_node *node;
    char kind[16];
    size_t node_index;
    cdin_frontend_gui_sdl_layout_status status;
    if (parser == NULL || parser->layout == NULL || raw == NULL ||
        raw_length == 0u || out_index == NULL) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    layout = parser->layout;
    if (depth > CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_DEPTH) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
    }
    if (layout->node_count >= CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_NODES) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
    }

    node_index = layout->node_count++;
    node = &layout->nodes[node_index];
    memset(node, 0, sizeof(*node));
    node->first_child = SIZE_MAX;
    node->second_child = SIZE_MAX;
    status = required_text(raw, raw_length, "kind", kind, sizeof(kind));
    if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
    status = required_text(raw, raw_length, "id", node->id, sizeof(node->id));
    if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
    if (identifier_exists(layout, node_index, node->id)) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }

    if (strcmp(kind, "leaf") == 0) {
        const uint8_t *views = NULL;
        const uint8_t *active_raw = NULL;
        size_t views_length = 0u;
        size_t active_raw_length = 0u;
        size_t view_count = 0u;
        size_t index;
        char active_view[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES + 1u];
        int has_active_view = 0;

        if (cdin_frontend_protocol_payload_get_raw(
                raw, raw_length, "views", &views, &views_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_array_count(
                views, views_length, &view_count) != CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        if (view_count > CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS ||
            view_count > CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_VIEWS - layout->view_count) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
        }
        for (index = 0u; index < view_count; ++index) {
            char view_id[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES + 1u];
            size_t view_id_length = 0u;
            uint8_t view_index = 0u;
            cdin_frontend_protocol_status array_status =
                cdin_frontend_protocol_array_get_text(
                    views, views_length, index, view_id, sizeof(view_id),
                    &view_id_length);
            if (array_status == CDIN_FRONTEND_PROTOCOL_BUFFER_TOO_SMALL) {
                return CDIN_FRONTEND_GUI_SDL_LAYOUT_LIMIT;
            }
            if (array_status != CDIN_FRONTEND_PROTOCOL_OK || view_id_length == 0u) {
                return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
            }
            status = add_view(layout, view_id, &view_index);
            if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
            node->view_indices[node->view_count++] = view_index;
        }
        if (cdin_frontend_protocol_payload_get_raw(
                raw, raw_length, "active_view",
                &active_raw, &active_raw_length) != CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        if (!raw_is_null(active_raw, active_raw_length)) {
            status = nullable_text(raw, raw_length, "active_view",
                                   active_view, sizeof(active_view),
                                   &has_active_view);
            if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK || !has_active_view) {
                return status == CDIN_FRONTEND_GUI_SDL_LAYOUT_OK
                    ? CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID : status;
            }
            for (index = 0u; index < node->view_count; ++index) {
                uint8_t candidate = node->view_indices[index];
                if (strcmp(layout->view_ids[candidate], active_view) == 0) {
                    node->active_view_index = candidate;
                    node->has_active_view = 1;
                    break;
                }
            }
            if (!node->has_active_view) return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        node->kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF;
        layout->leaf_count++;
    } else if (strcmp(kind, "split") == 0) {
        char axis[16];
        uint64_t ratio = 0u;
        const uint8_t *first = NULL;
        const uint8_t *second = NULL;
        size_t first_length = 0u;
        size_t second_length = 0u;
        size_t first_index = SIZE_MAX;
        size_t second_index = SIZE_MAX;

        status = required_text(raw, raw_length, "axis", axis, sizeof(axis));
        if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
        if (strcmp(axis, "horizontal") == 0) {
            node->axis = CDIN_FRONTEND_GUI_SDL_LAYOUT_HORIZONTAL;
        } else if (strcmp(axis, "vertical") == 0) {
            node->axis = CDIN_FRONTEND_GUI_SDL_LAYOUT_VERTICAL;
        } else {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        if (cdin_frontend_protocol_payload_get_uint(
                raw, raw_length, "ratio_bp", &ratio) !=
                CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        if (ratio < 1000u || ratio > 8999u) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        node->ratio_bp = (uint16_t)ratio;
        if (cdin_frontend_protocol_payload_get_raw(
                raw, raw_length, "first", &first, &first_length) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_raw(
                raw, raw_length, "second", &second, &second_length) !=
                CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        }
        status = parse_node(parser, first, first_length, depth + 1u, &first_index);
        if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
        status = parse_node(parser, second, second_length, depth + 1u, &second_index);
        if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
        node = &layout->nodes[node_index];
        node->first_child = first_index;
        node->second_child = second_index;
        node->kind = CDIN_FRONTEND_GUI_SDL_LAYOUT_SPLIT;
    } else {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }

    *out_index = node_index;
    return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
}

cdin_frontend_gui_sdl_layout_status cdin_frontend_gui_sdl_layout_parse(
    const cdin_frontend_protocol_message_view *message,
    cdin_frontend_gui_sdl_layout *out_layout)
{
    cdin_frontend_gui_sdl_layout parsed;
    cdin_frontend_gui_sdl_layout_parser parser;
    const uint8_t *layout_map = NULL;
    const uint8_t *root = NULL;
    size_t layout_map_length = 0u;
    size_t root_length = 0u;
    uint64_t revision = 0u;
    int has_active_leaf = 0;
    char active_leaf[CDIN_FRONTEND_GUI_SDL_LAYOUT_MAX_ID_BYTES + 1u];
    size_t index;
    cdin_frontend_gui_sdl_layout_status status;

    if (message == NULL || out_layout == NULL ||
        message->type != CDIN_FRONTEND_PROTOCOL_UI_LAYOUT ||
        message->payload == NULL || message->payload_length == 0u) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    memset(&parsed, 0, sizeof(parsed));
    parsed.root_index = SIZE_MAX;
    parsed.active_leaf_index = SIZE_MAX;
    if (cdin_frontend_protocol_payload_get_uint(
            message->payload, message->payload_length, "revision", &revision) !=
            CDIN_FRONTEND_PROTOCOL_OK || revision == 0u ||
        cdin_frontend_protocol_payload_get_raw(
            message->payload, message->payload_length, "layout",
            &layout_map, &layout_map_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    parsed.revision = revision;
    if (cdin_frontend_protocol_payload_get_raw(
            layout_map, layout_map_length, "root", &root, &root_length) !=
        CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    }
    status = nullable_text(layout_map, layout_map_length, "active_leaf",
                           active_leaf, sizeof(active_leaf), &has_active_leaf);
    if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;

    if (raw_is_null(root, root_length)) {
        if (has_active_leaf) return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
        *out_layout = parsed;
        return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
    }
    if (has_active_leaf == 0) return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;

    parser.layout = &parsed;
    status = parse_node(&parser, root, root_length, 1u, &parsed.root_index);
    if (status != CDIN_FRONTEND_GUI_SDL_LAYOUT_OK) return status;
    parsed.has_root = 1;
    for (index = 0u; index < parsed.node_count; ++index) {
        if (parsed.nodes[index].kind == CDIN_FRONTEND_GUI_SDL_LAYOUT_LEAF &&
            strcmp(parsed.nodes[index].id, active_leaf) == 0) {
            parsed.active_leaf_index = index;
            parsed.has_active_leaf = 1;
            break;
        }
    }
    if (parsed.has_active_leaf == 0) return CDIN_FRONTEND_GUI_SDL_LAYOUT_INVALID;
    *out_layout = parsed;
    return CDIN_FRONTEND_GUI_SDL_LAYOUT_OK;
}
