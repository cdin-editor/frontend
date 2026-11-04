/* Test-only ABI shim. It is never compiled into an installable Frontend target. */
#include <m-mdy-m/log.h>

#include <stdlib.h>
#include <string.h>

struct mmlog_logger {
    uint32_t min_level;
    int quiet;
    mmlog_sink_fn callback;
    void *callback_context;
    int32_t callback_min_level;
};

mmlog_status_t mmlog_logger_config_init(mmlog_logger_config_t *config)
{
    if (config == NULL) return MMLLOG_EINVAL;
    memset(config, 0, sizeof(*config));
    config->struct_size = (uint32_t)sizeof(*config);
    config->min_level = MMLLOG_LEVEL_INFO;
    config->color_mode = MMLLOG_COLOR_AUTO;
    config->output = MMLLOG_OUTPUT_STDERR;
    config->format_flags = MMLLOG_F_ALL;
    config->max_message_bytes = MMLLOG_DEFAULT_MESSAGE_BYTES;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_fields_init(mmlog_fields_t *fields)
{
    if (fields == NULL) return MMLLOG_EINVAL;
    memset(fields, 0, sizeof(*fields));
    fields->struct_size = MMLLOG_FIELDS_V1_SIZE;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_event_init(mmlog_event_t *event)
{
    if (event == NULL) return MMLLOG_EINVAL;
    memset(event, 0, sizeof(*event));
    event->struct_size = MMLLOG_EVENT_V1_SIZE;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_logger_create(const mmlog_logger_config_t *config,
                                   mmlog_logger_t **out_logger)
{
    if (config == NULL || out_logger == NULL || config->struct_size != sizeof(*config)) return MMLLOG_EINVAL;
    *out_logger = (mmlog_logger_t *)calloc(1u, sizeof(**out_logger));
    if (*out_logger == NULL) return MMLLOG_ENOMEM;
    (*out_logger)->min_level = config->min_level;
    (*out_logger)->callback_min_level = MMLLOG_LEVEL_TRACE;
    return MMLLOG_OK;
}

void mmlog_logger_destroy(mmlog_logger_t *logger) { free(logger); }

mmlog_status_t mmlog_logger_set_min_level(mmlog_logger_t *logger, int32_t min_level)
{
    if (logger == NULL || min_level < MMLLOG_LEVEL_TRACE || min_level > MMLLOG_LEVEL_FATAL) return MMLLOG_EINVAL;
    logger->min_level = (uint32_t)min_level;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_logger_set_color_mode(mmlog_logger_t *logger, mmlog_color_mode_t mode)
{
    if (logger == NULL || mode < MMLLOG_COLOR_AUTO || mode > MMLLOG_COLOR_NEVER) return MMLLOG_EINVAL;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_logger_set_quiet(mmlog_logger_t *logger, int quiet)
{
    if (logger == NULL) return MMLLOG_EINVAL;
    logger->quiet = quiet != 0;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_logger_add_callback(mmlog_logger_t *logger,
                                         mmlog_sink_fn callback,
                                         void *user_data,
                                         int32_t min_level)
{
    if (logger == NULL || callback == NULL || min_level < MMLLOG_LEVEL_TRACE ||
        min_level > MMLLOG_LEVEL_FATAL) return MMLLOG_EINVAL;
    logger->callback = callback;
    logger->callback_context = user_data;
    logger->callback_min_level = min_level;
    return MMLLOG_OK;
}

mmlog_status_t mmlog_logger_flush(mmlog_logger_t *logger)
{
    return logger == NULL ? MMLLOG_EINVAL : MMLLOG_OK;
}

mmlog_status_t mmlog_logger_write(mmlog_logger_t *logger, const mmlog_event_t *event)
{
    if (logger == NULL || event == NULL || event->struct_size < MMLLOG_EVENT_V1_SIZE ||
        event->message == NULL || event->message_length > MMLLOG_MAX_MESSAGE_BYTES) return MMLLOG_EINVAL;
    if (event->level < (int32_t)logger->min_level) return MMLLOG_OK;
    if (logger->callback != NULL && event->level >= logger->callback_min_level) {
        logger->callback(logger->callback_context, event->level, event->message, event->message_length);
    }
    return MMLLOG_OK;
}
