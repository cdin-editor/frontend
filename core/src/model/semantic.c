#include "cdin/frontend/core/model/semantic.h"
#include <string.h>

/* Validate UTF-8 without locale state; semantic identifiers are wire text. */
static int valid_utf8(const unsigned char *text, size_t length)
{
    size_t i = 0u;
    while (i < length) {
        unsigned char first = text[i];
        size_t extra;
        uint32_t codepoint;
        uint32_t minimum;
        size_t j;
        if (first <= 0x7fu) { i++; continue; }
        if (first >= 0xc2u && first <= 0xdfu) {
            extra = 1u; codepoint = (uint32_t)(first & 0x1fu); minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            extra = 2u; codepoint = (uint32_t)(first & 0x0fu); minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            extra = 3u; codepoint = (uint32_t)(first & 0x07u); minimum = 0x10000u;
        } else return 0;
        if (extra > length - i - 1u) return 0;
        for (j = 1u; j <= extra; ++j) {
            unsigned char next = text[i + j];
            if ((next & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6u) | (uint32_t)(next & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) return 0;
        i += extra + 1u;
    }
    return 1;
}
static int bounded_text_length(const char *s, size_t capacity, size_t *out_length)
{
    size_t i;
    if (s == NULL || out_length == NULL) return 0;
    for (i = 0u; i < capacity; ++i) {
        if (s[i] == '\0') { *out_length = i; return 1; }
    }
    return 0;
}
static int valid_id(const char *s)
{
    size_t length;
    return bounded_text_length(s, 64u, &length) && length != 0u &&
           valid_utf8((const unsigned char *)s, length);
}
static int valid_optional_id(const char *s)
{
    size_t length;
    return bounded_text_length(s, 64u, &length) &&
           (length == 0u || valid_utf8((const unsigned char *)s, length));
}
static int valid_text(const char *s, size_t capacity)
{
    size_t length;
    return bounded_text_length(s, capacity, &length) &&
           valid_utf8((const unsigned char *)s, length);
}
static int unique_id(const char *id, const char *const *ids, size_t count)
{
    size_t i;
    for (i=0u;i<count;++i) if (strcmp(id,ids[i])==0) return 0;
    return 1;
}
static int walk(const cdin_frontend_semantic_snapshot *s, size_t at,
                unsigned char *marks, unsigned char *placed)
{
    const cdin_frontend_semantic_node *n;
    size_t i;
    if (at >= s->node_count) return 0;
    if (marks[at] == 1u) return -1;
    if (marks[at] == 2u) return 0;
    marks[at]=1u; n=&s->nodes[at];
    if (n->kind == CDIN_FRONTEND_SEMANTIC_LEAF) {
        if (n->as.leaf.view_count > 32u) return 0;
        if (n->as.leaf.active_view != CDIN_FRONTEND_SEMANTIC_NO_INDEX) {
            int found=0;
            for (i=0u;i<n->as.leaf.view_count;++i) if (n->as.leaf.views[i]==n->as.leaf.active_view) found=1;
            if (!found) return 0;
        }
        for (i=0u;i<n->as.leaf.view_count;++i) {
            size_t v=n->as.leaf.views[i];
            if (v>=s->view_count || placed[v]) return 0;
            placed[v]=1u;
        }
    } else if (n->kind == CDIN_FRONTEND_SEMANTIC_SPLIT) {
        int a,b;
        if ((n->as.split.axis!=CDIN_FRONTEND_SEMANTIC_HORIZONTAL && n->as.split.axis!=CDIN_FRONTEND_SEMANTIC_VERTICAL) || n->as.split.ratio_bp<1000u || n->as.split.ratio_bp>8999u || n->as.split.first==n->as.split.second) return 0;
        a=walk(s,n->as.split.first,marks,placed); if(a!=1) return a;
        b=walk(s,n->as.split.second,marks,placed); if(b!=1) return b;
    } else return 0;
    marks[at]=2u; return 1;
}
cdin_frontend_semantic_status cdin_frontend_semantic_validate(const cdin_frontend_semantic_snapshot *s)
{
    const char *ids[128]; unsigned char marks[63]={0}, placed[32]={0}; size_t i,j; int result;
    if(s==NULL) return CDIN_FRONTEND_SEMANTIC_INVALID_ARGUMENT;
    if(s->view_count>32u || s->panel_count>16u || s->node_count>63u || s->style_count>128u) return CDIN_FRONTEND_SEMANTIC_LIMIT_EXCEEDED;
    if(!valid_optional_id(s->theme) || (s->options.present & ~CDIN_FRONTEND_SEMANTIC_OPTION_ALL)!=0u) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE;
    if((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_TITLE)!=0u && !valid_text(s->options.title, sizeof(s->options.title))) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE;
    if((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_TAB_WIDTH)!=0u && (s->options.tab_width<1u || s->options.tab_width>64u)) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE;
    if(((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_WRAP)!=0u && s->options.wrap!=0 && s->options.wrap!=1) || ((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_LINE_NUMBERS)!=0u && s->options.line_numbers!=0 && s->options.line_numbers!=1) || ((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_RELATIVE_NUMBERS)!=0u && s->options.relative_line_numbers!=0 && s->options.relative_line_numbers!=1) || ((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_CURSOR_BLINK)!=0u && s->options.cursor_blink!=0 && s->options.cursor_blink!=1)) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE;
    if((s->options.present & CDIN_FRONTEND_SEMANTIC_OPTION_CURSOR_SHAPE)!=0u && s->options.cursor_shape!=CDIN_FRONTEND_SEMANTIC_CURSOR_BLOCK && s->options.cursor_shape!=CDIN_FRONTEND_SEMANTIC_CURSOR_BAR && s->options.cursor_shape!=CDIN_FRONTEND_SEMANTIC_CURSOR_UNDERLINE) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE;
    for(i=0u;i<s->view_count;++i) {
        const cdin_frontend_semantic_view *v=&s->views[i];
        if(!valid_id(v->id) || !valid_id(v->document_id) || v->visible<0 || v->visible>1 || v->selection_anchor>v->document_length || v->selection_cursor>v->document_length || v->scroll_anchor>v->document_length) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE;
        ids[i]=v->id; if(!unique_id(v->id,ids,i)) return CDIN_FRONTEND_SEMANTIC_DUPLICATE_ID;
    }
    for(i=0u;i<s->node_count;++i) { if(!valid_id(s->nodes[i].id)) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE; ids[i]=s->nodes[i].id; for(j=0u;j<i;++j) if(strcmp(ids[i],ids[j])==0) return CDIN_FRONTEND_SEMANTIC_DUPLICATE_ID; }
    if(s->root==CDIN_FRONTEND_SEMANTIC_NO_INDEX) { if(s->node_count!=0u || s->active_leaf!=CDIN_FRONTEND_SEMANTIC_NO_INDEX) return CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE; }
    else {
        if(s->root>=s->node_count || (s->active_leaf!=CDIN_FRONTEND_SEMANTIC_NO_INDEX && s->active_leaf>=s->node_count)) return CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE;
        result=walk(s,s->root,marks,placed); if(result<0) return CDIN_FRONTEND_SEMANTIC_CYCLE; if(result==0) return CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE;
        if(s->active_leaf!=CDIN_FRONTEND_SEMANTIC_NO_INDEX && (s->nodes[s->active_leaf].kind!=CDIN_FRONTEND_SEMANTIC_LEAF || marks[s->active_leaf]!=2u)) return CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE;
        for(i=0u;i<s->node_count;++i) if(marks[i]!=2u) return CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE;
        for(i=0u;i<s->view_count;++i) if(!placed[i]) return CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE;
    }
    for(i=0u;i<s->panel_count;++i) { if(!valid_id(s->panels[i].id)||!valid_id(s->panels[i].kind)||s->panels[i].visible<0||s->panels[i].visible>1) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE; for(j=0u;j<i;++j) if(strcmp(s->panels[i].id,s->panels[j].id)==0) return CDIN_FRONTEND_SEMANTIC_DUPLICATE_ID; }
    for(i=0u;i<s->style_count;++i) { if(!valid_id(s->styles[i].role)) return CDIN_FRONTEND_SEMANTIC_INVALID_VALUE; for(j=0u;j<i;++j) if(strcmp(s->styles[i].role,s->styles[j].role)==0) return CDIN_FRONTEND_SEMANTIC_DUPLICATE_ID; }
    return CDIN_FRONTEND_SEMANTIC_OK;
}
cdin_frontend_semantic_status cdin_frontend_semantic_store_init(cdin_frontend_semantic_store *store)
{
    if(store==NULL) return CDIN_FRONTEND_SEMANTIC_INVALID_ARGUMENT;
    memset(store,0,sizeof(*store)); return CDIN_FRONTEND_SEMANTIC_OK;
}
cdin_frontend_semantic_status cdin_frontend_semantic_store_commit(cdin_frontend_semantic_store *store,const cdin_frontend_semantic_snapshot *snapshot)
{
    cdin_frontend_semantic_status status;
    if(store==NULL || snapshot==NULL) return CDIN_FRONTEND_SEMANTIC_INVALID_ARGUMENT;
    status=cdin_frontend_semantic_validate(snapshot); if(status!=CDIN_FRONTEND_SEMANTIC_OK) return status;
    if(store->has_snapshot) {
        if(snapshot->revision<=store->committed.revision ||
           snapshot->options_revision<store->committed.options_revision) return CDIN_FRONTEND_SEMANTIC_STALE;
        /* A stable view may not move its own revision or document version back. */
        for(size_t i=0u;i<snapshot->view_count;++i) {
            for(size_t j=0u;j<store->committed.view_count;++j) {
                const cdin_frontend_semantic_view *next=&snapshot->views[i];
                const cdin_frontend_semantic_view *previous=&store->committed.views[j];
                if(strcmp(next->id,previous->id)==0 &&
                   (next->view_revision<previous->view_revision ||
                    (strcmp(next->document_id,previous->document_id)==0 &&
                     next->document_version<previous->document_version))) {
                    return CDIN_FRONTEND_SEMANTIC_STALE;
                }
            }
        }
    }
    memcpy(&store->committed,snapshot,sizeof(*snapshot)); store->has_snapshot=1; return CDIN_FRONTEND_SEMANTIC_OK;
}
