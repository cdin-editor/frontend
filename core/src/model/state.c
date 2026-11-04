/*
 * FE-004/FE-005: only disposable view-version metadata is cached here. The
 * Backend/Runtime retain canonical editor state; line data is rejected when
 * it is older than the newest version observed for that view.
 */
#include "cdin/frontend/core/model/state.h"

#include <stdio.h>
#include <string.h>

static int valid_id(const char *view_id)
{
    size_t length;
    if (view_id == NULL || view_id[0] == '\0') return 0;
    length = strlen(view_id);
    return length <= CDIN_FRONTEND_VIEW_ID_MAX;
}

static size_t find_view(const cdin_frontend_view_version *views,
                        size_t count,
                        const char *view_id)
{
    size_t index;
    for (index = 0u; index < count; index++) {
        if (strcmp(views[index].id, view_id) == 0) return index;
    }
    return count;
}

void cdin_frontend_model_init(cdin_frontend_model_state *state)
{
    if (state != NULL) memset(state, 0, sizeof(*state));
}

cdin_frontend_model_status cdin_frontend_model_begin_snapshot(cdin_frontend_model_state *state,
                                                               uint64_t revision)
{
    if (state == NULL) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (state->snapshot_pending) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    if (state->has_snapshot && revision <= state->revision) return CDIN_FRONTEND_MODEL_STALE;
    state->snapshot_pending = 1;
    state->pending_revision = revision;
    state->pending_view_count = 0u;
    return CDIN_FRONTEND_MODEL_OK;
}

cdin_frontend_model_status cdin_frontend_model_stage_view(cdin_frontend_model_state *state,
                                                            const char *view_id,
                                                            uint64_t document_version)
{
    size_t index;
    cdin_frontend_view_version *entry;
    if (state == NULL || !valid_id(view_id)) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (!state->snapshot_pending) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    index = find_view(state->pending_views, state->pending_view_count, view_id);
    if (index != state->pending_view_count) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    if (state->pending_view_count >= CDIN_FRONTEND_MODEL_MAX_VIEWS) {
        return CDIN_FRONTEND_MODEL_LIMIT_EXCEEDED;
    }
    entry = &state->pending_views[state->pending_view_count];
    (void)snprintf(entry->id, sizeof(entry->id), "%s", view_id);
    entry->version = document_version;
    entry->lines_invalidated = 1;
    state->pending_view_count++;
    return CDIN_FRONTEND_MODEL_OK;
}

cdin_frontend_model_status cdin_frontend_model_abort_snapshot(cdin_frontend_model_state *state)
{
    if (state == NULL) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (!state->snapshot_pending) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    memset(state->pending_views, 0, sizeof(state->pending_views));
    state->pending_view_count = 0u;
    state->pending_revision = 0u;
    state->snapshot_pending = 0;
    return CDIN_FRONTEND_MODEL_OK;
}

cdin_frontend_model_status cdin_frontend_model_commit_flush(cdin_frontend_model_state *state,
                                                             cdin_frontend_flush_scope scope,
                                                             uint64_t sequence)
{
    if (state == NULL) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (sequence == 0u || sequence <= state->last_flush_sequence) {
        return CDIN_FRONTEND_MODEL_SEQUENCE_ERROR;
    }
    if (scope == CDIN_FRONTEND_FLUSH_SNAPSHOT) {
        if (!state->snapshot_pending) return CDIN_FRONTEND_MODEL_INVALID_STATE;
        memcpy(state->views, state->pending_views,
               state->pending_view_count * sizeof(state->views[0]));
        state->view_count = state->pending_view_count;
        state->revision = state->pending_revision;
        state->has_snapshot = 1;
        state->snapshot_pending = 0;
        state->pending_revision = 0u;
        state->pending_view_count = 0u;
    } else if (scope == CDIN_FRONTEND_FLUSH_RESPONSE) {
        if (state->snapshot_pending) return CDIN_FRONTEND_MODEL_INVALID_STATE;
        if (!state->has_snapshot) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    } else {
        return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    }
    state->last_flush_sequence = sequence;
    return CDIN_FRONTEND_MODEL_OK;
}

cdin_frontend_model_status cdin_frontend_model_note_lines(cdin_frontend_model_state *state,
                                                           const char *view_id,
                                                           uint64_t document_version)
{
    size_t index;
    if (state == NULL || !valid_id(view_id)) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (!state->has_snapshot) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    index = find_view(state->views, state->view_count, view_id);
    if (index == state->view_count) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (document_version < state->views[index].version) return CDIN_FRONTEND_MODEL_STALE;
    if (document_version > state->views[index].version) state->views[index].version = document_version;
    state->views[index].lines_invalidated = 0;
    return CDIN_FRONTEND_MODEL_OK;
}

cdin_frontend_model_status cdin_frontend_model_invalidate_view(cdin_frontend_model_state *state,
                                                               const char *view_id,
                                                               uint64_t document_version)
{
    size_t index;
    if (state == NULL || !valid_id(view_id)) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (!state->has_snapshot) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    index = find_view(state->views, state->view_count, view_id);
    if (index == state->view_count) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    if (document_version < state->views[index].version) return CDIN_FRONTEND_MODEL_STALE;
    state->views[index].version = document_version;
    state->views[index].lines_invalidated = 1;
    return CDIN_FRONTEND_MODEL_OK;
}

cdin_frontend_model_status cdin_frontend_model_view_version(const cdin_frontend_model_state *state,
                                                            const char *view_id,
                                                            uint64_t *out_version,
                                                            int *out_invalidated)
{
    size_t index;
    if (state == NULL || !valid_id(view_id) || out_version == NULL || out_invalidated == NULL) {
        return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    }
    if (!state->has_snapshot) return CDIN_FRONTEND_MODEL_INVALID_STATE;
    index = find_view(state->views, state->view_count, view_id);
    if (index == state->view_count) return CDIN_FRONTEND_MODEL_INVALID_ARGUMENT;
    *out_version = state->views[index].version;
    *out_invalidated = state->views[index].lines_invalidated;
    return CDIN_FRONTEND_MODEL_OK;
}
