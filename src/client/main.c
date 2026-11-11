#include "client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  -h, --host <host>       Server hostname or IP (default: 127.0.0.1)\n");
    printf("  -p, --port <port>       Server port (default: 9000)\n");
    printf("  -c, --command <cmd>     Execute a single command non-interactively and exit\n");
    printf("  -l, --log-level <lvl>   Log level (debug, info, warn, error, off)\n");
    printf("  -v, --verbose           Enable debug logging (alias for -l debug)\n");
    printf("  --help                  Show this help message and exit\n");
}

int main(int argc, char **argv) {
    nsp_client_config_t cfg = {
        .host = "127.0.0.1",
        .port = 9000,
        .single_command = NULL,
        .log_level = NSP_LOG_INFO
    };

    nsp_log_init_from_env();
    cfg.log_level = nsp_log_get_level();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for host\n");
                return 1;
            }
            cfg.host = argv[i];
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for port\n");
                return 1;
            }
            cfg.port = atoi(argv[i]);
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--command") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for command\n");
                return 1;
            }
            cfg.single_command = argv[i];
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--log-level") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for log-level\n");
                return 1;
            }
            cfg.log_level = nsp_log_level_from_str(argv[i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            cfg.log_level = NSP_LOG_DEBUG;
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    nsp_log_set_level(cfg.log_level);
    LOG_DEBUG("Client initialized (host=%s, port=%d)", cfg.host, cfg.port);

    return nsp_client_run(&cfg);
}
