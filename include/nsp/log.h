#ifndef NSP_LOG_H
#define NSP_LOG_H

#include <stdio.h>
#include <stdbool.h>

#ifndef NSP_COMPILE_DEBUG
#define NSP_COMPILE_DEBUG 1
#endif

typedef enum {
    NSP_LOG_DEBUG = 0,
    NSP_LOG_INFO  = 1,
    NSP_LOG_WARN  = 2,
    NSP_LOG_ERROR = 3,
    NSP_LOG_OFF   = 4
} nsp_log_level_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Set the minimum runtime log level */
void nsp_log_set_level(nsp_log_level_t level);

/* Get the current runtime log level */
nsp_log_level_t nsp_log_get_level(void);

/* Parse string to log level ("debug", "info", "warn", "error") */
nsp_log_level_t nsp_log_level_from_str(const char *str);

/* Initialize log level from LOG_LEVEL environment variable if present */
void nsp_log_init_from_env(void);

/* Internal log write function called by macros */
void nsp_log_write(nsp_log_level_t level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

/* Two-tier logging macros */
#if NSP_COMPILE_DEBUG
#define LOG_DEBUG(...) nsp_log_write(NSP_LOG_DEBUG, __VA_ARGS__)
#else
#define LOG_DEBUG(...) ((void)0)
#endif

#define LOG_INFO(...)  nsp_log_write(NSP_LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...)  nsp_log_write(NSP_LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...) nsp_log_write(NSP_LOG_ERROR, __VA_ARGS__)

#endif /* NSP_LOG_H */
