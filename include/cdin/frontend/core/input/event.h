#ifndef CDIN_FRONTEND_INPUT_EVENT_H
#define CDIN_FRONTEND_INPUT_EVENT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    CDIN_FRONTEND_MOD_CTRL = 1u << 0,
    CDIN_FRONTEND_MOD_ALT = 1u << 1,
    CDIN_FRONTEND_MOD_SHIFT = 1u << 2,
    CDIN_FRONTEND_MOD_ALTGR = 1u << 3
};

typedef enum cdin_frontend_input_kind {
    CDIN_FRONTEND_INPUT_KEY = 0,
    CDIN_FRONTEND_INPUT_TEXT,
    CDIN_FRONTEND_INPUT_PASTE,
    CDIN_FRONTEND_INPUT_UNKNOWN
} cdin_frontend_input_kind;

#define CDIN_FRONTEND_INPUT_MAX_BYTES 4096u

typedef struct cdin_frontend_input_event {
    cdin_frontend_input_kind kind;
    char key[24];
    uint32_t modifiers;
    uint8_t text[CDIN_FRONTEND_INPUT_MAX_BYTES];
    size_t text_length;
} cdin_frontend_input_event;

#ifdef __cplusplus
}
#endif
#endif
