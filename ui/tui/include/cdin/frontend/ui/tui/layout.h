#ifndef CDIN_FRONTEND_TUI_LAYOUT_H
#define CDIN_FRONTEND_TUI_LAYOUT_H

#include "cdin/frontend/core/protocol/wire.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_TUI_LAYOUT_MAX_NODES 63u
#define CDIN_FRONTEND_TUI_LAYOUT_MAX_VIEWS 32u
#define CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY 64u

typedef enum cdin_frontend_tui_layout_status {
    CDIN_FRONTEND_TUI_LAYOUT_OK = 0,
    CDIN_FRONTEND_TUI_LAYOUT_INVALID_ARGUMENT,
    CDIN_FRONTEND_TUI_LAYOUT_INVALID_MESSAGE,
    CDIN_FRONTEND_TUI_LAYOUT_LIMIT_EXCEEDED,
    CDIN_FRONTEND_TUI_LAYOUT_DUPLICATE_ID,
    CDIN_FRONTEND_TUI_LAYOUT_BAD_REFERENCE
} cdin_frontend_tui_layout_status;

typedef enum cdin_frontend_tui_layout_node_kind {
    CDIN_FRONTEND_TUI_LAYOUT_LEAF = 1,
    CDIN_FRONTEND_TUI_LAYOUT_SPLIT = 2
} cdin_frontend_tui_layout_node_kind;

typedef enum cdin_frontend_tui_layout_axis {
    CDIN_FRONTEND_TUI_LAYOUT_HORIZONTAL = 1,
    CDIN_FRONTEND_TUI_LAYOUT_VERTICAL = 2
} cdin_frontend_tui_layout_axis;

/* A bounded, immutable semantic PaneTree. Indices refer to nodes/views stored
 * in this value; no pointer in the parsed tree aliases the Protocol payload. */
typedef struct cdin_frontend_tui_layout_node {
    cdin_frontend_tui_layout_node_kind kind;
    char id[CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
    union {
        struct {
            uint8_t view_count;
            uint8_t view_indexes[CDIN_FRONTEND_TUI_LAYOUT_MAX_VIEWS];
            uint8_t active_view_index; /* UINT8_MAX means no active view. */
        } leaf;
        struct {
            cdin_frontend_tui_layout_axis axis;
            uint16_t ratio_bp;
            uint8_t first;
            uint8_t second;
        } split;
    } as;
} cdin_frontend_tui_layout_node;

typedef struct cdin_frontend_tui_layout {
    uint64_t revision;
    size_t node_count;
    size_t root;
    int has_root;
    size_t active_leaf;
    int has_active_leaf;
    size_t view_count;
    char view_ids[CDIN_FRONTEND_TUI_LAYOUT_MAX_VIEWS][CDIN_FRONTEND_TUI_LAYOUT_ID_CAPACITY];
    cdin_frontend_tui_layout_node nodes[CDIN_FRONTEND_TUI_LAYOUT_MAX_NODES];
} cdin_frontend_tui_layout;

/* Parses the Protocol 1.0.0 ui.layout payload. The output changes only when the
 * complete PaneTree, nullable references, IDs, view membership and split
 * constraints pass validation. */
cdin_frontend_tui_layout_status cdin_frontend_tui_layout_parse(
    cdin_frontend_tui_layout *out_layout,
    const uint8_t *payload,
    size_t payload_length);

#ifdef __cplusplus
}
#endif
#endif
