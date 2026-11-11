#define _POSIX_C_SOURCE 200809L
#include "nsp/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

static nsp_log_level_t g_current_level = NSP_LOG_INFO;

void nsp_log_set_level(nsp_log_level_t level) {
    g_current_level = level;
}

nsp_log_level_t nsp_log_get_level(void) {
    return g_current_level;
}

nsp_log_level_t nsp_log_level_from_str(const char *str) {
    if (!str) {
        return NSP_LOG_INFO;
    }
    if (strcasecmp(str, "debug") == 0) {
        return NSP_LOG_DEBUG;
    }
    if (strcasecmp(str, "info") == 0) {
        return NSP_LOG_INFO;
    }
    if (strcasecmp(str, "warn") == 0 || strcasecmp(str, "warning") == 0) {
        return NSP_LOG_WARN;
    }
    if (strcasecmp(str, "error") == 0) {
        return NSP_LOG_ERROR;
    }
    if (strcasecmp(str, "off") == 0) {
        return NSP_LOG_OFF;
    }
    return NSP_LOG_INFO;
}

void nsp_log_init_from_env(void) {
    const char *env_level = getenv("LOG_LEVEL");
    if (env_level && *env_level) {
        g_current_level = nsp_log_level_from_str(env_level);
    }
}

static const char *level_to_tag(nsp_log_level_t level) {
    switch (level) {
        case NSP_LOG_DEBUG: return "DEBUG";
        case NSP_LOG_INFO:  return "INFO";
        case NSP_LOG_WARN:  return "WARN";
        case NSP_LOG_ERROR: return "ERROR";
        default:            return "LOG";
    }
}

void nsp_log_write(nsp_log_level_t level, const char *fmt, ...) {
    if (level < g_current_level || level >= NSP_LOG_OFF) {
        return;
    }

    time_t now = time(NULL);
    struct tm tm_buf;
    localtime_r(&now, &tm_buf);

    char time_str[32];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_buf);

    pid_t pid = getpid();

    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    fprintf(stderr, "%s [%d] [%s] %s\n", time_str, (int)pid, level_to_tag(level), msg_buf);
    fflush(stderr);
}
