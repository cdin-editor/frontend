#include "cdin/frontend/ui/headless/recorder.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct replay_state {
    size_t calls;
    uint64_t last_sequence;
    cdin_frontend_headless_direction last_direction;
} replay_state;

static int replay_fail(void *opaque,
                       cdin_frontend_headless_direction direction,
                       const cdin_frontend_protocol_message_view *message)
{
    (void)opaque;
    (void)direction;
    (void)message;
    return 1;
}

static int replay_one(void *opaque,
                      cdin_frontend_headless_direction direction,
                      const cdin_frontend_protocol_message_view *message)
{
    replay_state *state = (replay_state *)opaque;
    assert(message != NULL);
    assert(message->name_space != NULL && strcmp(message->name_space, "core") == 0);
    assert(message->type == CDIN_FRONTEND_PROTOCOL_ACK);
    state->calls++;
    state->last_sequence = message->sequence;
    state->last_direction = direction;
    return 0;
}

int main(void)
{
    static const uint8_t ack_payload[] = {
        0xa1u, 0x68u, 's','e','q','u','e','n','c','e', 0x01u
    };
    uint8_t trace[128];
    uint8_t small[7];
    uint8_t exact[CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES +
                  CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES + sizeof(ack_payload)];
    cdin_frontend_headless_recorder recorder;
    cdin_frontend_headless_recorder too_small;
    cdin_frontend_headless_recorder exact_recorder;
    cdin_frontend_protocol_message_view message;
    replay_state state;
    size_t replayed = 0u;
    size_t trace_length;

    assert(cdin_frontend_headless_recorder_init(&too_small, small, sizeof(small)) ==
           CDIN_FRONTEND_HEADLESS_BUFFER_TOO_SMALL);
    assert(cdin_frontend_headless_recorder_init(&recorder, trace, sizeof(trace)) ==
           CDIN_FRONTEND_HEADLESS_OK);
    assert(cdin_frontend_headless_recorder_init(&exact_recorder, exact, sizeof(exact)) ==
           CDIN_FRONTEND_HEADLESS_OK);

    memset(&message, 0, sizeof(message));
    message.major = CDIN_FRONTEND_PROTOCOL_VERSION_MAJOR;
    message.minor = CDIN_FRONTEND_PROTOCOL_VERSION_MINOR;
    message.sequence = 7u;
    message.type = CDIN_FRONTEND_PROTOCOL_ACK;
    message.name_space = "core";
    message.payload = ack_payload;
    message.payload_length = sizeof(ack_payload);

    assert(cdin_frontend_headless_record(&recorder,
        CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND, &message) == CDIN_FRONTEND_HEADLESS_OK);
    assert(cdin_frontend_headless_record(&exact_recorder,
        CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND, &message) == CDIN_FRONTEND_HEADLESS_OK);
    assert(cdin_frontend_headless_record(&exact_recorder,
        CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND, &message) ==
        CDIN_FRONTEND_HEADLESS_BUFFER_TOO_SMALL);
    assert(cdin_frontend_headless_record(&recorder,
        CDIN_FRONTEND_HEADLESS_BACKEND_TO_FRONTEND, &message) ==
        CDIN_FRONTEND_HEADLESS_INVALID_TRACE);
    assert(cdin_frontend_headless_recorder_size(&recorder) ==
           CDIN_FRONTEND_HEADLESS_TRACE_HEADER_BYTES +
           CDIN_FRONTEND_HEADLESS_RECORD_HEADER_BYTES + sizeof(ack_payload));

    memset(&state, 0, sizeof(state));
    trace_length = cdin_frontend_headless_recorder_size(&recorder);
    assert(cdin_frontend_headless_replay(trace, trace_length,
        CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND, replay_one, &state, &replayed) ==
        CDIN_FRONTEND_HEADLESS_OK);
    assert(replayed == 1u && state.calls == 1u);
    assert(state.last_sequence == 7u);
    assert(state.last_direction == CDIN_FRONTEND_HEADLESS_FRONTEND_TO_BACKEND);
    assert(cdin_frontend_headless_replay(trace, trace_length, 0,
        replay_fail, NULL, &replayed) == CDIN_FRONTEND_HEADLESS_CALLBACK_ERROR);
    assert(replayed == 0u);

    state.calls = 0u;
    assert(cdin_frontend_headless_replay(trace, trace_length,
        CDIN_FRONTEND_HEADLESS_BACKEND_TO_FRONTEND, replay_one, &state, &replayed) ==
        CDIN_FRONTEND_HEADLESS_OK);
    assert(replayed == 0u && state.calls == 0u);

    assert(cdin_frontend_headless_replay(trace, trace_length - 1u, 0,
        replay_one, &state, &replayed) == CDIN_FRONTEND_HEADLESS_INVALID_TRACE);
    trace[4] = 0u;
    trace[5] = 2u;
    assert(cdin_frontend_headless_replay(trace, trace_length, 0,
        replay_one, &state, &replayed) == CDIN_FRONTEND_HEADLESS_UNSUPPORTED_VERSION);
    return 0;
}
