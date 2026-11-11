#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nsp/nsp.h"

int main(void) {
    printf("=== Testing Logging Module ===\n");

    nsp_log_set_level(NSP_LOG_INFO);
    if (nsp_log_get_level() != NSP_LOG_INFO) {
        fprintf(stderr, "Log level setter/getter mismatch\n");
        return 1;
    }

    if (nsp_log_level_from_str("debug") != NSP_LOG_DEBUG ||
        nsp_log_level_from_str("DEBUG") != NSP_LOG_DEBUG ||
        nsp_log_level_from_str("info")  != NSP_LOG_INFO  ||
        nsp_log_level_from_str("warn")  != NSP_LOG_WARN  ||
        nsp_log_level_from_str("error") != NSP_LOG_ERROR) {
        fprintf(stderr, "Log level parsing failed\n");
        return 1;
    }

    /* Test that log macros compile and execute cleanly */
    LOG_DEBUG("This is a debug message (may or may not show based on level)");
    LOG_INFO("This is an info message: %d", 42);
    LOG_WARN("This is a warning: %s", "caution");
    LOG_ERROR("This is an error message");

    printf("Logging tests passed successfully.\n");
    return 0;
}
