/* FE-004/T3: bounded Protocol PaneTree parser for the TUI adapter. */
#include "cdin/frontend/ui/tui/layout.h"

#include <stdio.h>
#include <string.h>

#define TUI_LAYOUT_MAX_DEPTH 32u

static cdin_frontend_tui_layout_status read_text(
    const uint8_t *payload, size_t payload_length, const char *field,
    char output[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY])
{
    size_t length = 0u;
    cdin_frontend_protocol_status status =
        cdin_frontend_protocol_payload_get_text(payload, payload_length,
                                                 field, output,
                                                 CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY,
                                                 &length);
    if (status != CDIN_FRONTEND_PROTOCOL_OK || length == 0u) {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
    }
    return CDIN_FRONTEND_TUI_LAYOUT_OK;
}

static cdin_frontend_tui_layout_status raw_text(
    const uint8_t *raw, size_t raw_length,
    char output[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY])
{
    uint8_t array[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY + 4u];
    size_t text_length = 0u;
    if (raw == NULL || raw_length == 0u ||
        raw_length + 1u > sizeof(array)) {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
    }
    /* Reuse the public canonical-CBOR array text decoder for one raw text
     * item, rather than implementing a second string decoder here. */
    array[0] = 0x81u;
    memcpy(array + 1u, raw, raw_length);
    return cdin_frontend_protocol_array_get_text(
        array, raw_length + 1u, 0u, output,
        CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY, &text_length) ==
        CDIN_FRONTEND_PROTOCOL_OK && text_length != 0u
        ? CDIN_FRONTEND_TUI_LAYOUT_OK
        : CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
}

static size_t find_node(const cdin_frontend_tui_layout *layout, const char *id)
{
    size_t index;
    for (index = 0u; index < layout->node_count; index++) {
        if (strcmp(layout->nodes[index].id, id) == 0) return index;
    }
    return layout->node_count;
}

static size_t find_view(const cdin_frontend_tui_layout *layout, const char *id)
{
    size_t index;
    for (index = 0u; index < layout->view_count; index++) {
        if (strcmp(layout->view_ids[index], id) == 0) return index;
    }
    return layout->view_count;
}

static cdin_frontend_tui_layout_status parse_node(
    cdin_frontend_tui_layout *layout,
    const uint8_t *raw,
    size_t raw_length,
    size_t depth,
    size_t *out_index)
{
    char kind[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
    char id[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
    size_t index;
    cdin_frontend_tui_layout_node *node;

    if (layout == NULL || raw == NULL || out_index == NULL) {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_ARGUMENT;
    }
    if (depth > TUI_LAYOUT_MAX_DEPTH) return CDIN_FRONTEND_TUI_LAYOUT_LIMIT_EXCEEDED;
    if (layout->node_count >= CDIN_FRONTEND_TUI_LAYOUT_MAX_NODES) {
        return CDIN_FRONTEND_TUI_LAYOUT_LIMIT_EXCEEDED;
    }
    if (read_text(raw, raw_length, "kind", kind) != CDIN_FRONTEND_TUI_LAYOUT_OK ||
        read_text(raw, raw_length, "id", id) != CDIN_FRONTEND_TUI_LAYOUT_OK) {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
    }
    if (find_node(layout, id) != layout->node_count) {
        return CDIN_FRONTEND_TUI_LAYOUT_DUPLICATE_ID;
    }

    index = layout->node_count++;
    node = &layout->nodes[index];
    memset(node, 0, sizeof(*node));
    (void)snprintf(node->id, sizeof(node->id), "%s", id);

    if (strcmp(kind, "leaf") == 0) {
        const uint8_t *views;
        const uint8_t *active_raw;
        size_t views_length;
        size_t active_length;
        size_t count;
        size_t view_index;
        cdin_frontend_protocol_status wire_status;

        node->kind = CDIN_FRONTEND_TUI_LAYOUT_LEAF;
        if (cdin_frontend_protocol_payload_get_raw(raw, raw_length, "views",
                &views, &views_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_array_count(views, views_length, &count) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            count > CDIN_FRONTEND_TUI_LAYOUT_MAX_VIEWS ||
            count > CDIN_FRONTEND_TUI_LAYOUT_MAX_VIEWS - layout->view_count) {
            return CDIN_FRONTEND_TUI_LAYOUT_LIMIT_EXCEEDED;
        }
        for (view_index = 0u; view_index < count; view_index++) {
            char view_id[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
            size_t global_index;
            size_t text_length = 0u;
            if (cdin_frontend_protocol_array_get_text(
                    views, views_length, view_index, view_id, sizeof(view_id),
                    &text_length) != CDIN_FRONTEND_PROTOCOL_OK ||
                text_length == 0u) {
                return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
            }
            if (find_view(layout, view_id) != layout->view_count) {
                return CDIN_FRONTEND_TUI_LAYOUT_DUPLICATE_ID;
            }
            global_index = layout->view_count++;
            (void)snprintf(layout->view_ids[global_index],
                           sizeof(layout->view_ids[global_index]), "%s", view_id);
            node->as.leaf.view_indexes[view_index] = (uint8_t)global_index;
            node->as.leaf.view_count = (uint8_t)(view_index + 1u);
        }
        wire_status = cdin_frontend_protocol_payload_get_raw(
            raw, raw_length, "active_view", &active_raw, &active_length);
        if (wire_status != CDIN_FRONTEND_PROTOCOL_OK || active_length == 0u) {
            return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
        }
        node->as.leaf.active_view_index = UINT8_MAX;
        if (!(active_length == 1u && active_raw[0] == 0xf6u)) {
            char active_id[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
            size_t active_index;
            size_t leaf_index;
            if (raw_text(active_raw, active_length, active_id) !=
                CDIN_FRONTEND_TUI_LAYOUT_OK) {
                return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
            }
            active_index = find_view(layout, active_id);
            if (active_index == layout->view_count) {
                return CDIN_FRONTEND_TUI_LAYOUT_BAD_REFERENCE;
            }
            for (leaf_index = 0u; leaf_index < count; leaf_index++) {
                if (node->as.leaf.view_indexes[leaf_index] == active_index) break;
            }
            if (leaf_index == count) return CDIN_FRONTEND_TUI_LAYOUT_BAD_REFERENCE;
            node->as.leaf.active_view_index = (uint8_t)active_index;
        }
    } else if (strcmp(kind, "split") == 0) {
        char axis[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
        const uint8_t *first_raw;
        const uint8_t *second_raw;
        size_t first_length;
        size_t second_length;
        uint64_t ratio;
        size_t first_index;
        size_t second_index;
        cdin_frontend_tui_layout_status status;

        node->kind = CDIN_FRONTEND_TUI_LAYOUT_SPLIT;
        if (read_text(raw, raw_length, "axis", axis) != CDIN_FRONTEND_TUI_LAYOUT_OK ||
            cdin_frontend_protocol_payload_get_uint(raw, raw_length, "ratio_bp", &ratio) !=
                CDIN_FRONTEND_PROTOCOL_OK ||
            ratio < 1000u || ratio > 8999u ||
            cdin_frontend_protocol_payload_get_raw(raw, raw_length, "first",
                &first_raw, &first_length) != CDIN_FRONTEND_PROTOCOL_OK ||
            cdin_frontend_protocol_payload_get_raw(raw, raw_length, "second",
                &second_raw, &second_length) != CDIN_FRONTEND_PROTOCOL_OK) {
            return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
        }
        if (strcmp(axis, "horizontal") == 0) {
            node->as.split.axis = CDIN_FRONTEND_TUI_LAYOUT_HORIZONTAL;
        } else if (strcmp(axis, "vertical") == 0) {
            node->as.split.axis = CDIN_FRONTEND_TUI_LAYOUT_VERTICAL;
        } else {
            return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
        }
        node->as.split.ratio_bp = (uint16_t)ratio;
        status = parse_node(layout, first_raw, first_length, depth + 1u, &first_index);
        if (status != CDIN_FRONTEND_TUI_LAYOUT_OK) return status;
        status = parse_node(layout, second_raw, second_length, depth + 1u, &second_index);
        if (status != CDIN_FRONTEND_TUI_LAYOUT_OK) return status;
        if (first_index == second_index || first_index > UINT8_MAX ||
            second_index > UINT8_MAX) {
            return CDIN_FRONTEND_TUI_LAYOUT_BAD_REFERENCE;
        }
        node->as.split.first = (uint8_t)first_index;
        node->as.split.second = (uint8_t)second_index;
    } else {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
    }

    *out_index = index;
    return CDIN_FRONTEND_TUI_LAYOUT_OK;
}

cdin_frontend_tui_layout_status cdin_frontend_tui_layout_parse(
    cdin_frontend_tui_layout *out_layout,
    const uint8_t *payload,
    size_t payload_length)
{
    cdin_frontend_tui_layout parsed;
    const uint8_t *layout_raw;
    const uint8_t *root_raw;
    const uint8_t *active_raw;
    size_t layout_length;
    size_t root_length;
    size_t active_length;
    char active_id[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
    uint64_t revision;
    size_t root_index;
    size_t active_index;
    cdin_frontend_protocol_status wire_status;
    cdin_frontend_tui_layout_status status;

    if (out_layout == NULL || payload == NULL || payload_length == 0u) {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_ARGUMENT;
    }
    if (cdin_frontend_protocol_validate_payload(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
            payload, payload_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_uint(payload, payload_length, "revision",
            &revision) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(payload, payload_length, "layout",
            &layout_raw, &layout_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(layout_raw, layout_length, "root",
            &root_raw, &root_length) != CDIN_FRONTEND_PROTOCOL_OK ||
        cdin_frontend_protocol_payload_get_raw(layout_raw, layout_length, "active_leaf",
            &active_raw, &active_length) != CDIN_FRONTEND_PROTOCOL_OK) {
        return CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE;
    }

    memset(&parsed, 0, sizeof(parsed));
    parsed.revision = revision;
    parsed.root = SIZE_MAX;
    parsed.active_leaf = SIZE_MAX;

    if (!(root_length == 1u && root_raw[0] == 0xf6u)) {
        status = parse_node(&parsed, root_raw, root_length, 1u, &root_index);
        if (status != CDIN_FRONTEND_TUI_LAYOUT_OK) return status;
        parsed.root = root_index;
        parsed.has_root = 1;
    }

    wire_status = CDIN_FRONTEND_PROTOCOL_OK;
    if (active_length == 1u && active_raw[0] == 0xf6u) {
        parsed.has_active_leaf = 0;
    } else {
        if (!parsed.has_root || raw_text(active_raw, active_length, active_id) !=
            CDIN_FRONTEND_TUI_LAYOUT_OK) {
            return CDIN_FRONTEND_TUI_LAYOUT_BAD_REFERENCE;
        }
        active_index = find_node(&parsed, active_id);
        if (active_index == parsed.node_count ||
            parsed.nodes[active_index].kind != CDIN_FRONTEND_TUI_LAYOUT_LEAF) {
            return CDIN_FRONTEND_TUI_LAYOUT_BAD_REFERENCE;
        }
        parsed.active_leaf = active_index;
        parsed.has_active_leaf = 1;
    }
    (void)wire_status;
    *out_layout = parsed;
    return CDIN_FRONTEND_TUI_LAYOUT_OK;
}
