#ifndef CDIN_FRONTEND_DIAGNOSTICS_LOG_H
#define CDIN_FRONTEND_DIAGNOSTICS_LOG_H

/* Frontend diagnostics are backed by the standalone m-mdy-m/log.c library. */
#include <m-mdy-m/log.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef mmlog_logger_t cdin_frontend_logger;

/* Create one logger instance per component. The caller owns the returned
 * instance and destroys it only after all component workers have stopped. */
mmlog_status_t cdin_frontend_logger_create(mmlog_level_t minimum_level,
                                             cdin_frontend_logger **out_logger);
void cdin_frontend_logger_destroy(cdin_frontend_logger *logger);

#if defined(__GNUC__) || defined(__clang__)
#define CDIN_FRONTEND_PRINTF_FORMAT(format_index, argument_index) \
    __attribute__((format(printf, format_index, argument_index)))
#else
#define CDIN_FRONTEND_PRINTF_FORMAT(format_index, argument_index)
#endif

void cdin_frontend_logf(cdin_frontend_logger *logger,
                        mmlog_level_t level,
                        const char *area,
                        const char *file,
                        uint32_t line,
                        const char *function,
                        const char *format,
                        ...) CDIN_FRONTEND_PRINTF_FORMAT(7, 8);

#define CDIN_FRONTEND_TRACE(logger, area, ...) \
    cdin_frontend_logf((logger), MMLLOG_LEVEL_TRACE, (area), __FILE__, \
                       (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define CDIN_FRONTEND_DEBUG(logger, area, ...) \
    cdin_frontend_logf((logger), MMLLOG_LEVEL_DEBUG, (area), __FILE__, \
                       (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define CDIN_FRONTEND_INFO(logger, area, ...) \
    cdin_frontend_logf((logger), MMLLOG_LEVEL_INFO, (area), __FILE__, \
                       (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define CDIN_FRONTEND_WARN(logger, area, ...) \
    cdin_frontend_logf((logger), MMLLOG_LEVEL_WARN, (area), __FILE__, \
                       (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define CDIN_FRONTEND_ERROR(logger, area, ...) \
    cdin_frontend_logf((logger), MMLLOG_LEVEL_ERROR, (area), __FILE__, \
                       (uint32_t)__LINE__, __func__, __VA_ARGS__)
#define CDIN_FRONTEND_FATAL(logger, area, ...) \
    cdin_frontend_logf((logger), MMLLOG_LEVEL_FATAL, (area), __FILE__, \
                       (uint32_t)__LINE__, __func__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif
