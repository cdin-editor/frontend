#include "cdin/frontend/core/model/semantic.h"
#include <assert.h>
#include <string.h>

static void base(cdin_frontend_semantic_snapshot *s)
{
    memset(s,0,sizeof(*s)); s->revision=1u; s->root=0u; s->active_leaf=0u; s->node_count=1u;
    s->nodes[0].kind=CDIN_FRONTEND_SEMANTIC_LEAF; strcpy(s->nodes[0].id,"leaf-main");
    s->nodes[0].as.leaf.active_view=CDIN_FRONTEND_SEMANTIC_NO_INDEX;
}
int main(void)
{
    cdin_frontend_semantic_snapshot s; cdin_frontend_semantic_store store;
    base(&s); assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_OK);
    assert(cdin_frontend_semantic_store_init(&store)==CDIN_FRONTEND_SEMANTIC_OK);
    assert(cdin_frontend_semantic_store_commit(&store,&s)==CDIN_FRONTEND_SEMANTIC_OK);
    s.revision=2u; s.view_count=1u; strcpy(s.views[0].id,"view-a"); strcpy(s.views[0].document_id,"doc-a");
    s.nodes[0].as.leaf.view_count=1u; s.nodes[0].as.leaf.views[0]=0u; s.nodes[0].as.leaf.active_view=0u;
    s.views[0].document_length=10u; s.views[0].selection_anchor=3u; s.views[0].selection_cursor=7u;
    assert(cdin_frontend_semantic_store_commit(&store,&s)==CDIN_FRONTEND_SEMANTIC_OK);
    assert(store.committed.revision==2u && store.committed.view_count==1u);
    s.revision=3u; s.views[0].selection_cursor=11u;
    assert(cdin_frontend_semantic_store_commit(&store,&s)==CDIN_FRONTEND_SEMANTIC_INVALID_VALUE);
    assert(store.committed.revision==2u); /* invalid update is atomic */
    s.views[0].selection_cursor=7u; s.nodes[0].as.leaf.views[0]=32u;
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE);
    s.nodes[0].as.leaf.views[0]=0u;
    s.views[0].document_id[0]=(char)0xc0; s.views[0].document_id[1]=(char)0xaf;
    s.views[0].document_id[2]='\0';
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_INVALID_VALUE);
    strcpy(s.views[0].document_id,"doc-a");
    s.nodes[0].as.leaf.views[0]=0u; s.nodes[0].as.leaf.active_view=CDIN_FRONTEND_SEMANTIC_NO_INDEX;
    s.options.present=CDIN_FRONTEND_SEMANTIC_OPTION_TAB_WIDTH; s.options.tab_width=65u;
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_INVALID_VALUE);
    s.options.tab_width=4u; s.options.present=CDIN_FRONTEND_SEMANTIC_OPTION_TITLE; memset(s.options.title, 'T', 200u); s.options.title[200]='\0';
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_OK);
    s.options.present=0u; s.revision=2u;
    assert(cdin_frontend_semantic_store_commit(&store,&s)==CDIN_FRONTEND_SEMANTIC_STALE);
    s.revision=3u; s.views[0].document_version=5u; s.views[0].view_revision=2u;
    assert(cdin_frontend_semantic_store_commit(&store,&s)==CDIN_FRONTEND_SEMANTIC_OK);
    s.revision=4u; s.views[0].document_version=4u;
    assert(cdin_frontend_semantic_store_commit(&store,&s)==CDIN_FRONTEND_SEMANTIC_STALE);
    assert(store.committed.revision==3u);
    /* Null root and null active leaf are valid empty layout values. */
    memset(&s,0,sizeof(s)); s.revision=4u; s.root=CDIN_FRONTEND_SEMANTIC_NO_INDEX; s.active_leaf=CDIN_FRONTEND_SEMANTIC_NO_INDEX;
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_OK);
    /* Normative model bounds fail closed before any store mutation. */
    s.view_count=33u;
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_LIMIT_EXCEEDED);
    s.view_count=0u;
    /* Cycles fail closed. */
    s.root=0u; s.active_leaf=CDIN_FRONTEND_SEMANTIC_NO_INDEX; s.node_count=1u;
    s.nodes[0].kind=CDIN_FRONTEND_SEMANTIC_SPLIT; strcpy(s.nodes[0].id,"split");
    s.nodes[0].as.split.axis=CDIN_FRONTEND_SEMANTIC_HORIZONTAL; s.nodes[0].as.split.ratio_bp=5000u;
    s.nodes[0].as.split.first=0u; s.nodes[0].as.split.second=0u;
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_BAD_REFERENCE);
    s.node_count=3u; s.nodes[0].as.split.first=1u; s.nodes[0].as.split.second=2u;
    s.nodes[1].kind=CDIN_FRONTEND_SEMANTIC_SPLIT; strcpy(s.nodes[1].id,"cycle");
    s.nodes[1].as.split.axis=CDIN_FRONTEND_SEMANTIC_VERTICAL; s.nodes[1].as.split.ratio_bp=5000u;
    s.nodes[1].as.split.first=1u; s.nodes[1].as.split.second=2u;
    s.nodes[2].kind=CDIN_FRONTEND_SEMANTIC_LEAF; strcpy(s.nodes[2].id,"leaf");
    s.nodes[2].as.leaf.active_view=CDIN_FRONTEND_SEMANTIC_NO_INDEX;
    assert(cdin_frontend_semantic_validate(&s)==CDIN_FRONTEND_SEMANTIC_CYCLE);
    return 0;
}
