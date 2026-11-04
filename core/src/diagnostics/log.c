/*
 * The Frontend diagnostic adapter writes to an independent m-mdy-m/log.c
 * logger instance. It never forwards through the Kernel logger.
 */
#include "cdin/frontend/core/diagnostics/log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define CDIN_FRONTEND_LOG_MESSAGE_CAPACITY 4096u

static int valid_area(const char *area)
{
    size_t index;
    if (area == NULL || area[0] == '\0' || strlen(area) > 24u) return 0;
    for (index = 0u; area[index] != '\0'; index++) {
        const unsigned char value = (unsigned char)area[index];
        if (!((value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
              value == '_' || value == '-')) return 0;
    }
    return 1;
}

mmlog_status_t cdin_frontend_logger_create(mmlog_level_t minimum_level,
                                            cdin_frontend_logger **out_logger)
{
    mmlog_logger_config_t config;
    mmlog_status_t status;
    if (out_logger == NULL) return MMLLOG_EINVAL;
    *out_logger = NULL;
    status = mmlog_logger_config_init(&config);
    if (status != MMLLOG_OK) return status;
    config.min_level = (uint32_t)minimum_level;
    config.color_mode = MMLLOG_COLOR_AUTO;
    config.output = MMLLOG_OUTPUT_STDERR;
    config.format_flags = MMLLOG_F_TIMESTAMP | MMLLOG_F_LAYER | MMLLOG_F_COMPONENT |
                          MMLLOG_F_SOURCE | MMLLOG_F_FUNCTION;
    config.max_message_bytes = CDIN_FRONTEND_LOG_MESSAGE_CAPACITY;
    return mmlog_logger_create(&config, out_logger);
}

void cdin_frontend_logger_destroy(cdin_frontend_logger *logger)
{
    mmlog_logger_destroy(logger);
}

void cdin_frontend_logf(cdin_frontend_logger *logger,
                        mmlog_level_t level,
                        const char *area,
                        const char *file,
                        uint32_t line,
                        const char *function,
                        const char *format,
                        ...)
{
    char detail[CDIN_FRONTEND_LOG_MESSAGE_CAPACITY];
    char message[CDIN_FRONTEND_LOG_MESSAGE_CAPACITY];
    char component[48];
    mmlog_event_t event;
    va_list arguments;
    int detail_length;
    int message_length;
    if (logger == NULL || !valid_area(area) || format == NULL ||
        level < MMLLOG_LEVEL_TRACE || level > MMLLOG_LEVEL_FATAL) return;

    va_start(arguments, format);
    detail_length = vsnprintf(detail, sizeof(detail), format, arguments);
    va_end(arguments);
    if (detail_length < 0) {
        (void)snprintf(detail, sizeof(detail), "diagnostic message formatting failed");
    } else if ((size_t)detail_length >= sizeof(detail)) {
        static const char suffix[] = "... [truncated]";
        memcpy(detail + sizeof(detail) - sizeof(suffix), suffix, sizeof(suffix));
    }

    message_length = snprintf(message, sizeof(message), "[FRONTEND][%s] %s", area, detail);
    if (message_length < 0) return;
    if ((size_t)message_length >= sizeof(message)) message_length = (int)(sizeof(message) - 1u);
    (void)snprintf(component, sizeof(component), "frontend.%s", area);

    if (mmlog_event_init(&event) != MMLLOG_OK) return;
    event.struct_size = MMLLOG_EVENT_V1_SIZE;
    event.level = level;
    event.line = line;
    event.layer = "cdin";
    event.component = component;
    event.file = file;
    event.function = function;
    event.message = message;
    event.message_length = (size_t)message_length;
    (void)mmlog_logger_write(logger, &event);
}
