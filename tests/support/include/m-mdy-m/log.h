/* Test-only ABI declarations copied from m-mdy-m/log.c v0.1.0. */
#ifndef M_MDY_M_LOG_LOG_H
#define M_MDY_M_LOG_LOG_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t mmlog_level_t;
enum {
    MMLLOG_LEVEL_TRACE = 0,
    MMLLOG_LEVEL_DEBUG = 1,
    MMLLOG_LEVEL_INFO = 2,
    MMLLOG_LEVEL_WARN = 3,
    MMLLOG_LEVEL_ERROR = 4,
    MMLLOG_LEVEL_FATAL = 5
};

typedef int32_t mmlog_status_t;
enum {
    MMLLOG_OK = 0,
    MMLLOG_EINVAL = -1,
    MMLLOG_ENOMEM = -2,
    MMLLOG_EIO = -3,
    MMLLOG_ELIMIT = -4,
    MMLLOG_EFULL = -5
};

typedef int32_t mmlog_color_mode_t;
enum { MMLLOG_COLOR_AUTO = 0, MMLLOG_COLOR_ALWAYS = 1, MMLLOG_COLOR_NEVER = 2 };
typedef int32_t mmlog_output_t;
enum { MMLLOG_OUTPUT_STDERR = 0, MMLLOG_OUTPUT_STDOUT = 1, MMLLOG_OUTPUT_NONE = 2 };

enum {
    MMLLOG_F_TIMESTAMP = 1u << 0,
    MMLLOG_F_LAYER = 1u << 1,
    MMLLOG_F_COMPONENT = 1u << 2,
    MMLLOG_F_SOURCE = 1u << 3,
    MMLLOG_F_FUNCTION = 1u << 4,
    MMLLOG_F_REQUEST_ID = 1u << 5,
    MMLLOG_F_TRACE_ID = 1u << 6,
    MMLLOG_F_DURATION = 1u << 7,
    MMLLOG_F_STATUS = 1u << 8
};
#define MMLLOG_F_ALL ((1u << 9) - 1u)
#define MMLLOG_MAX_MESSAGE_BYTES (1024u * 1024u)
#define MMLLOG_DEFAULT_MESSAGE_BYTES (16u * 1024u)

typedef struct mmlog_logger mmlog_logger_t;
typedef struct mmlog_logger_config {
    uint32_t struct_size;
    uint32_t min_level;
    uint32_t color_mode;
    uint32_t output;
    uint32_t format_flags;
    uint32_t max_message_bytes;
} mmlog_logger_config_t;

typedef struct mmlog_fields {
    uint32_t struct_size;
    uint32_t reserved;
    uint64_t timestamp_unix_ns;
    uint64_t duration_ns;
    const char *layer;
    const char *component;
    const char *request_id;
    const char *trace_id;
    const char *status;
} mmlog_fields_t;

typedef struct mmlog_event {
    uint32_t struct_size;
    int32_t level;
    uint64_t timestamp_unix_ns;
    uint64_t duration_ns;
    uint32_t line;
    uint32_t reserved;
    const char *layer;
    const char *component;
    const char *file;
    const char *function;
    const char *request_id;
    const char *trace_id;
    const char *status;
    const char *message;
    size_t message_length;
} mmlog_event_t;

#define MMLLOG_CONFIG_V1_SIZE (offsetof(mmlog_logger_config_t, max_message_bytes) + sizeof(uint32_t))
#define MMLLOG_FIELDS_V1_SIZE (offsetof(mmlog_fields_t, status) + sizeof(const char *))
#define MMLLOG_EVENT_V1_SIZE (offsetof(mmlog_event_t, message_length) + sizeof(size_t))
typedef void (*mmlog_sink_fn)(void *user_data, int32_t level,
                              const char *line, size_t line_length);

mmlog_status_t mmlog_logger_config_init(mmlog_logger_config_t *config);
mmlog_status_t mmlog_fields_init(mmlog_fields_t *fields);
mmlog_status_t mmlog_event_init(mmlog_event_t *event);
mmlog_status_t mmlog_logger_create(const mmlog_logger_config_t *config,
                                   mmlog_logger_t **out_logger);
void mmlog_logger_destroy(mmlog_logger_t *logger);
mmlog_status_t mmlog_logger_set_min_level(mmlog_logger_t *logger, int32_t min_level);
mmlog_status_t mmlog_logger_set_color_mode(mmlog_logger_t *logger, mmlog_color_mode_t mode);
mmlog_status_t mmlog_logger_set_quiet(mmlog_logger_t *logger, int quiet);
mmlog_status_t mmlog_logger_add_callback(mmlog_logger_t *logger,
                                         mmlog_sink_fn callback,
                                         void *user_data,
                                         int32_t min_level);
mmlog_status_t mmlog_logger_flush(mmlog_logger_t *logger);
mmlog_status_t mmlog_logger_write(mmlog_logger_t *logger, const mmlog_event_t *event);

#endif
