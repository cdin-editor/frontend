#ifndef CDIN_FRONTEND_MODEL_STATE_H
#define CDIN_FRONTEND_MODEL_STATE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_VIEW_ID_MAX 63u
#define CDIN_FRONTEND_MODEL_MAX_VIEWS 32u

typedef enum cdin_frontend_model_status {
    CDIN_FRONTEND_MODEL_OK = 0,
    CDIN_FRONTEND_MODEL_INVALID_ARGUMENT,
    CDIN_FRONTEND_MODEL_INVALID_STATE,
    CDIN_FRONTEND_MODEL_STALE,
    CDIN_FRONTEND_MODEL_LIMIT_EXCEEDED,
    CDIN_FRONTEND_MODEL_SEQUENCE_ERROR
} cdin_frontend_model_status;

typedef enum cdin_frontend_flush_scope {
    CDIN_FRONTEND_FLUSH_SNAPSHOT = 0,
    CDIN_FRONTEND_FLUSH_RESPONSE = 1
} cdin_frontend_flush_scope;

typedef struct cdin_frontend_view_version {
    char id[CDIN_FRONTEND_VIEW_ID_MAX + 1u];
    uint64_t version;
    int lines_invalidated;
} cdin_frontend_view_version;

typedef struct cdin_frontend_model_state {
    uint64_t revision;
    uint64_t last_flush_sequence;
    int has_snapshot;
    int snapshot_pending;
    uint64_t pending_revision;
    size_t view_count;
    size_t pending_view_count;
    cdin_frontend_view_version views[CDIN_FRONTEND_MODEL_MAX_VIEWS];
    cdin_frontend_view_version pending_views[CDIN_FRONTEND_MODEL_MAX_VIEWS];
} cdin_frontend_model_state;

void cdin_frontend_model_init(cdin_frontend_model_state *state);
cdin_frontend_model_status cdin_frontend_model_begin_snapshot(cdin_frontend_model_state *state,
                                                               uint64_t revision);
cdin_frontend_model_status cdin_frontend_model_stage_view(cdin_frontend_model_state *state,
                                                            const char *view_id,
                                                            uint64_t document_version);
cdin_frontend_model_status cdin_frontend_model_abort_snapshot(cdin_frontend_model_state *state);
cdin_frontend_model_status cdin_frontend_model_commit_flush(cdin_frontend_model_state *state,
                                                             cdin_frontend_flush_scope scope,
                                                             uint64_t sequence);
cdin_frontend_model_status cdin_frontend_model_note_lines(cdin_frontend_model_state *state,
                                                           const char *view_id,
                                                           uint64_t document_version);
cdin_frontend_model_status cdin_frontend_model_invalidate_view(cdin_frontend_model_state *state,
                                                               const char *view_id,
                                                               uint64_t document_version);
cdin_frontend_model_status cdin_frontend_model_view_version(const cdin_frontend_model_state *state,
                                                            const char *view_id,
                                                            uint64_t *out_version,
                                                            int *out_invalidated);

#ifdef __cplusplus
}
#endif
#endif
