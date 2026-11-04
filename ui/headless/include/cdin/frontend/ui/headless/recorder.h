#ifndef CDIN_FRONTEND_UI_HEADLESS_RECORDER_H
#define CDIN_FRONTEND_UI_HEADLESS_RECORDER_H

#include "cdin/frontend/core/protocol/wire.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES 8u
#define CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES 20u
#define CDIN_FRONTEND_HEADLESS_TRACE_VERSION 1u

typedef enum cdin_frontend_headless_status {
    CDIN_FRONTEND_HEADLESS_OK = 0,
    CDIN_FRONTEND_HEADLESS_INVALID_ARGUMENT,
    CDIN_FRONTEND_HEADLESS_INVALID_TRACE,
    CDIN_FRONTEND_HEADLESS_UNSUPPORTED_VERSION,
    CDIN_FRONTEND_HEADLESS_BUFFER_TOO_SMALL,
    CDIN_FRONTEND_HEADLESS_CALLBACK_ERROR
} cdin_frontend_headless_status;

typedef enum cdin_frontend_headless_direction {
    CDIN_FRONTEND_HEADLESS_BACKEND_TO_FRONTEND = 1,
    CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND = 2
} cdin_frontend_headless_direction;

typedef struct cdin_frontend_headless_recorder {
    uint8_t *storage;
    size_t capacity;
    size_t length;
    int initialized;
} cdin_frontend_headless_recorder;

typedef int (*cdin_frontend_headless_replay_fn)(
    void *context,
    cdin_frontend_headless_direction direction,
    const cdin_frontend_protocol_message_view *message);

/* The caller owns storage for the recorder lifetime. No allocation occurs. */
cdin_frontend_headless_status cdin_frontend_headless_recorder_init(
    cdin_frontend_headless_recorder *recorder,
    uint8_t *storage,
    size_t capacity);

/* Records one validated core-protocol message without changing its payload. */
cdin_frontend_headless_status cdin_frontend_headless_record(
    cdin_frontend_headless_recorder *recorder,
    cdin_frontend_headless_direction direction,
    const cdin_frontend_protocol_message_view *message);

/* Replays records in insertion order. A non-zero callback result stops replay. */
cdin_frontend_headless_status cdin_frontend_headless_replay(
    const uint8_t *trace,
    size_t trace_length,
    cdin_frontend_headless_direction direction_filter,
    cdin_frontend_headless_replay_fn callback,
    void *context,
    size_t *out_replayed);

size_t cdin_frontend_headless_recorder_size(
    const cdin_frontend_headless_recorder *recorder);

#ifdef __cplusplus
}
#endif
#endif
