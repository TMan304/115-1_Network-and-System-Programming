#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  -p, --port <port>       Listening port (default: %d)\n", NSP_DEFAULT_PORT);
    printf("  -m, --max-clients <num> Maximum concurrent client connections (default: %d)\n", NSP_DEFAULT_MAX_CLIENTS);
    printf("  -t, --timeout <sec>     Client idle socket receive timeout (default: %d)\n", NSP_DEFAULT_TIMEOUT_SEC);
    printf("  -l, --log-level <lvl>   Log level (debug, info, warn, error, off)\n");
    printf("  -v, --verbose           Enable debug logging (sets log-level to debug)\n");
    printf("  --no-reap               Disable SIGCHLD zombie reaping (Robustness #1 demo)\n");
    printf("  --no-validate           Disable binary frame header validation (Robustness #2 demo)\n");
    printf("  --no-timeout            Disable socket receive timeout (Robustness #3 demo)\n");
    printf("  --no-limit              Disable connection limit check (Robustness #4 demo)\n");
    printf("  --no-robust             Disable all four robustness mechanisms simultaneously\n");
    printf("  -h, --help              Show this help message and exit\n");
}

int main(int argc, char **argv) {
    nsp_server_config_t cfg;
    nsp_server_config_init_defaults(&cfg);
    nsp_log_init_from_env();
    cfg.log_level = nsp_log_get_level();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for port\n");
                return 1;
            }
            cfg.port = atoi(argv[i]);
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--max-clients") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for max-clients\n");
                return 1;
            }
            cfg.max_clients = atoi(argv[i]);
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for timeout\n");
                return 1;
            }
            cfg.timeout_sec = atoi(argv[i]);
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log-level") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for log-level\n");
                return 1;
            }
            cfg.log_level = nsp_log_level_from_str(argv[i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            cfg.log_level = NSP_LOG_DEBUG;
        } else if (strcmp(argv[i], "--no-reap") == 0) {
            cfg.reap_zombies = false;
        } else if (strcmp(argv[i], "--no-validate") == 0) {
            cfg.validate_packets = false;
        } else if (strcmp(argv[i], "--no-timeout") == 0) {
            cfg.enable_timeout = false;
        } else if (strcmp(argv[i], "--no-limit") == 0) {
            cfg.enable_limit = false;
        } else if (strcmp(argv[i], "--no-robust") == 0) {
            cfg.reap_zombies = false;
            cfg.validate_packets = false;
            cfg.enable_timeout = false;
            cfg.enable_limit = false;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    return nsp_server_run(&cfg);
}
