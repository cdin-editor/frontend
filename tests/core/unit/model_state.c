#include "cdin/frontend/core/model/state.h"

#include <assert.h>
#include <stdint.h>

int main(void)
{
    cdin_frontend_model_state model;
    uint64_t version = 0u;
    int invalidated = 0;

    cdin_frontend_model_init(&model);
    assert(cdin_frontend_model_begin_snapshot(&model, 10u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_stage_view(&model, "view-main", 7u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_commit_flush(&model, CDIN_FRONTEND_FLUSH_SNAPSHOT, 1u) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(model.revision == 10u);
    assert(cdin_frontend_model_view_version(&model, "view-main", &version, &invalidated) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(version == 7u && invalidated == 1);

    assert(cdin_frontend_model_note_lines(&model, "view-main", 7u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_note_lines(&model, "view-main", 6u) == CDIN_FRONTEND_MODEL_STALE);
    assert(cdin_frontend_model_invalidate_view(&model, "view-main", 8u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_note_lines(&model, "view-main", 7u) == CDIN_FRONTEND_MODEL_STALE);
    assert(cdin_frontend_model_view_version(&model, "view-main", &version, &invalidated) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(version == 8u && invalidated == 1);

    assert(cdin_frontend_model_commit_flush(&model, CDIN_FRONTEND_FLUSH_RESPONSE, 2u) ==
           CDIN_FRONTEND_MODEL_OK);
    /* Response flushes keep the last complete snapshot intact. */
    assert(cdin_frontend_model_view_version(&model, "view-main", &version, &invalidated) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(version == 8u && invalidated == 1);
    assert(cdin_frontend_model_begin_snapshot(&model, 10u) == CDIN_FRONTEND_MODEL_STALE);
    assert(cdin_frontend_model_begin_snapshot(&model, 11u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_stage_view(&model, "view-secondary", 0u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_commit_flush(&model, CDIN_FRONTEND_FLUSH_SNAPSHOT, 3u) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_view_version(&model, "view-main", &version, &invalidated) ==
           CDIN_FRONTEND_MODEL_INVALID_ARGUMENT);
    assert(cdin_frontend_model_view_version(&model, "view-secondary", &version, &invalidated) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(version == 0u && invalidated == 1);
    assert(cdin_frontend_model_commit_flush(&model, CDIN_FRONTEND_FLUSH_RESPONSE, 3u) ==
           CDIN_FRONTEND_MODEL_SEQUENCE_ERROR);

    /* An incomplete snapshot can be discarded without mutating the last view. */
    assert(cdin_frontend_model_begin_snapshot(&model, 12u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_stage_view(&model, "temporary", 2u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_abort_snapshot(&model) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_view_version(&model, "view-secondary", &version, &invalidated) ==
           CDIN_FRONTEND_MODEL_OK);
    assert(version == 0u && invalidated == 1);
    assert(cdin_frontend_model_begin_snapshot(&model, 12u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_stage_view(&model, "view-final", 4u) == CDIN_FRONTEND_MODEL_OK);
    assert(cdin_frontend_model_commit_flush(&model, CDIN_FRONTEND_FLUSH_SNAPSHOT, 4u) ==
           CDIN_FRONTEND_MODEL_OK);
    return 0;
}
