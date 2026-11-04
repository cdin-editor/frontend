#ifndef CDIN_FRONTEND_MODEL_SEMANTIC_H
#define CDIN_FRONTEND_MODEL_SEMANTIC_H

/* FE-004/FE-005: bounded, UI-neutral cdin.ui-model.v1 snapshot contract.
 * Adapters derive geometry and glyphs; Core retains semantic values only. */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CDIN_FRONTEND_SEMANTIC_MAX_VIEWS 32u
#define CDIN_FRONTEND_SEMANTIC_MAX_PANELS 16u
#define CDIN_FRONTEND_SEMANTIC_MAX_NODES 63u
#define CDIN_FRONTEND_SEMANTIC_MAX_STYLES 128u
#define CDIN_FRONTEND_SEMANTIC_ID_MAX 63u
#define CDIN_FRONTEND_SEMANTIC_TITLE_MAX 255u
#define CDIN_FRONTEND_SEMANTIC_NO_INDEX SIZE_MAX
#define CDIN_FRONTEND_SEMANTIC_OPTION_TITLE (UINT32_C(1)<<0)
#define CDIN_FRONTEND_SEMANTIC_OPTION_TAB_WIDTH (UINT32_C(1)<<1)
#define CDIN_FRONTEND_SEMANTIC_OPTION_WRAP (UINT32_C(1)<<2)
#define CDIN_FRONTEND_SEMANTIC_OPTION_LINE_NUMBERS (UINT32_C(1)<<3)
#define CDIN_FRONTEND_SEMANTIC_OPTION_RELATIVE_NUMBERS (UINT32_C(1)<<4)
#define CDIN_FRONTEND_SEMANTIC_OPTION_CURSOR_SHAPE (UINT32_C(1)<<5)
#define CDIN_FRONTEND_SEMANTIC_OPTION_CURSOR_BLINK (UINT32_C(1)<<6)
#define CDIN_FRONTEND_SEMANTIC_OPTION_ALL ((UINT32_C(1)<<7)-1u)
typedef enum cdin_frontend_semantic_status { CDIN_FRONTEND_SEMANTIC_OK=0, CDIN_FRONTEND_SEMANTIC_INVALID_ARGUMENT, CDIN_FRONTEND_SEMANTIC_INVALID_VALUE, CDIN_FRONTEND_SEMANTIC_DUPLICATE_ID, CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE, CDIN_FRONTEND_SEMANTIC_CYCLE, CDIN_FRONTEND_SEMANTIC_LIMIT_EXCEEDED, CDIN_FRONTEND_SEMANTIC_STALE } cdin_frontend_semantic_status;
typedef enum cdin_frontend_semantic_node_kind { CDIN_FRONTEND_SEMANTIC_LEAF=1, CDIN_FRONTEND_SEMANTIC_SPLIT=2 } cdin_frontend_semantic_node_kind;
typedef enum cdin_frontend_semantic_axis { CDIN_FRONTEND_SEMANTIC_HORIZONTAL=1, CDIN_FRONTEND_SEMANTIC_VERTICAL=2 } cdin_frontend_semantic_axis;
typedef enum cdin_frontend_semantic_cursor_shape { CDIN_FRONTEND_SEMANTIC_CURSOR_BLOCK=1, CDIN_FRONTEND_SEMANTIC_CURSOR_BAR=2, CDIN_FRONTEND_SEMANTIC_CURSOR_UNDERLINE=3 } cdin_frontend_semantic_cursor_shape;
typedef struct cdin_frontend_semantic_view { char id[64]; char document_id[64]; uint64_t document_version, document_length, view_revision, selection_anchor, selection_cursor, scroll_anchor; int visible; } cdin_frontend_semantic_view;
typedef struct cdin_frontend_semantic_node { cdin_frontend_semantic_node_kind kind; char id[64]; union { struct { size_t view_count, views[32], active_view; } leaf; struct { cdin_frontend_semantic_axis axis; uint16_t ratio_bp; size_t first, second; } split; } as; } cdin_frontend_semantic_node;
typedef struct cdin_frontend_semantic_options { uint32_t present; char title[256]; uint8_t tab_width; int wrap, line_numbers, relative_line_numbers; cdin_frontend_semantic_cursor_shape cursor_shape; int cursor_blink; } cdin_frontend_semantic_options;
typedef struct cdin_frontend_semantic_panel { char id[64], kind[64]; int visible; } cdin_frontend_semantic_panel;
typedef struct cdin_frontend_semantic_style { char role[64]; uint32_t foreground_rgba, background_rgba, attributes; } cdin_frontend_semantic_style;
typedef struct cdin_frontend_semantic_snapshot { uint64_t revision; size_t view_count; cdin_frontend_semantic_view views[32]; size_t node_count, root, active_leaf; cdin_frontend_semantic_node nodes[63]; size_t panel_count; cdin_frontend_semantic_panel panels[16]; uint64_t options_revision; char theme[64]; cdin_frontend_semantic_options options; size_t style_count; cdin_frontend_semantic_style styles[128]; } cdin_frontend_semantic_snapshot;
typedef struct cdin_frontend_semantic_store { int has_snapshot; cdin_frontend_semantic_snapshot committed; } cdin_frontend_semantic_store;
cdin_frontend_semantic_status cdin_frontend_semantic_validate(const cdin_frontend_semantic_snapshot *snapshot);
cdin_frontend_semantic_status cdin_frontend_semantic_store_init(cdin_frontend_semantic_store *store);
cdin_frontend_semantic_status cdin_frontend_semantic_store_commit(cdin_frontend_semantic_store *store, const cdin_frontend_semantic_snapshot *snapshot);
#ifdef __cplusplus
}
#endif
#endif
