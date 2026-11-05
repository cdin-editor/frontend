#include "cdin/frontend/ui/tui/layout.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const uint8_t split_layout[] = {
    0xa2u,
    0x66u, 'l','a','y','o','u','t',
    0xa2u,
      0x64u, 'r','o','o','t',
      0xa6u,
        0x62u, 'i','d', 0x6au, 's','p','l','i','t','-','r','o','o','t',
        0x64u, 'a','x','i','s', 0x68u, 'v','e','r','t','i','c','a','l',
        0x64u, 'k','i','n','d', 0x65u, 's','p','l','i','t',
        0x65u, 'f','i','r','s','t',
        0xa4u,
          0x62u, 'i','d', 0x69u, 'l','e','a','f','-','m','a','i','n',
          0x64u, 'k','i','n','d', 0x64u, 'l','e','a','f',
          0x65u, 'v','i','e','w','s', 0x81u, 0x66u, 'v','i','e','w','-','1',
          0x6bu, 'a','c','t','i','v','e','_','v','i','e','w', 0x66u, 'v','i','e','w','-','1',
        0x66u, 's','e','c','o','n','d',
        0xa4u,
          0x62u, 'i','d', 0x69u, 'l','e','a','f','-','s','i','d','e',
          0x64u, 'k','i','n','d', 0x64u, 'l','e','a','f',
          0x65u, 'v','i','e','w','s', 0x80u,
          0x6bu, 'a','c','t','i','v','e','_','v','i','e','w', 0xf6u,
        0x68u, 'r','a','t','i','o','_','b','p', 0x19u, 0x13u, 0x88u,
      0x6bu, 'a','c','t','i','v','e','_','l','e','a','f',
      0x69u, 'l','e','a','f','-','m','a','i','n',
    0x68u, 'r','e','v','i','s','i','o','n', 0x01u
};

static const uint8_t empty_layout[] = {
    0xa2u,
    0x66u, 'l','a','y','o','u','t',
    0xa2u,
      0x64u, 'r','o','o','t', 0xf6u,
      0x6bu, 'a','c','t','i','v','e','_','l','e','a','f', 0xf6u,
    0x68u, 'r','e','v','i','s','i','o','n', 0x02u
};

int main(void)
{
    cdin_frontend_tui_layout layout;
    cdin_frontend_tui_layout_node *root;
    assert(cdin_frontend_protocol_validate_payload(CDIN_FRONTEND_PROTOCOL_UI_LAYOUT,
           split_layout, sizeof(split_layout)) == CDIN_FRONTEND_PROTOCOL_OK);
    assert(cdin_frontend_tui_layout_parse(&layout, split_layout,
           sizeof(split_layout)) == CDIN_FRONTEND_TUI_LAYOUT_OK);
    assert(layout.revision == 1u);
    assert(layout.has_root && layout.root == 0u);
    assert(layout.has_active_leaf && layout.active_leaf == 1u);
    assert(layout.node_count == 3u && layout.view_count == 1u);
    assert(strcmp(layout.view_ids[0], "view-1") == 0);
    root = &layout.nodes[layout.root];
    assert(root->kind == CDIN_FRONTEND_TUI_LAYOUT_SPLIT);
    assert(strcmp(root->id, "split-root") == 0);
    assert(root->as.split.axis == CDIN_FRONTEND_TUI_LAYOUT_VERTICAL);
    assert(root->as.split.ratio_bp == 5000u);
    assert(root->as.split.first == 1u && root->as.split.second == 2u);
    assert(layout.nodes[1].kind == CDIN_FRONTEND_TUI_LAYOUT_LEAF);
    assert(layout.nodes[1].as.leaf.view_count == 1u);
    assert(layout.nodes[1].as.leaf.active_view_index == 0u);
    assert(layout.nodes[2].as.leaf.view_count == 0u);
    assert(layout.nodes[2].as.leaf.active_view_index == UINT8_MAX);
    assert(cdin_frontend_tui_layout_parse(&layout, empty_layout,
           sizeof(empty_layout)) == CDIN_FRONTEND_TUI_LAYOUT_OK);
    assert(layout.revision == 2u && !layout.has_root && !layout.has_active_leaf);
    assert(layout.node_count == 0u && layout.view_count == 0u);
    assert(cdin_frontend_tui_layout_parse(NULL, split_layout,
           sizeof(split_layout)) == CDIN_FRONTEND_TUI_LAYOUT_INVALID_ARGUMENT);
    return 0;
}
