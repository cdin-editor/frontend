#ifndef CDIN_FRONTEND_INPUT_PARSER_H
#define CDIN_FRONTEND_INPUT_PARSER_H

#include "cdin/frontend/core/input/event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CDIN_FRONTEND_INPUT_PARSER_CAPACITY (CDIN_FRONTEND_INPUT_MAX_BYTES + 64u)

typedef enum cdin_frontend_input_parser_status {
    CDIN_FRONTEND_INPUT_PARSER_EVENT = 0,
    CDIN_FRONTEND_INPUT_PARSER_NEED_MORE,
    CDIN_FRONTEND_INPUT_PARSER_INVALID,
    CDIN_FRONTEND_INPUT_PARSER_LIMIT_EXCEEDED
} cdin_frontend_input_parser_status;

typedef struct cdin_frontend_input_parser {
    uint8_t bytes[CDIN_FRONTEND_INPUT_PARSER_CAPACITY];
    size_t length;
    int dropping_oversized_paste;
} cdin_frontend_input_parser;

void cdin_frontend_input_parser_init(cdin_frontend_input_parser *parser);
cdin_frontend_input_parser_status cdin_frontend_input_parser_feed(
    cdin_frontend_input_parser *parser, const uint8_t *bytes, size_t length);
/* flush_escape is set after the platform's short ESC disambiguation timeout. */
cdin_frontend_input_parser_status cdin_frontend_input_parser_next(
    cdin_frontend_input_parser *parser,
    int flush_escape,
    cdin_frontend_input_event *out_event);

#ifdef __cplusplus
}
#endif
#endif
