#ifndef NSP_SERVER_H
#define NSP_SERVER_H

#include <stdbool.h>
#include <sys/types.h>
#include "nsp/nsp.h"

#define NSP_DEFAULT_PORT 9000
#define NSP_DEFAULT_MAX_CLIENTS 16
#define NSP_DEFAULT_TIMEOUT_SEC 30

typedef struct {
    int port;
    int max_clients;
    int timeout_sec;
    bool reap_zombies;       /* Robustness 1: false if --no-reap */
    bool validate_packets;   /* Robustness 2: false if --no-validate */
    bool enable_timeout;     /* Robustness 3: false if --no-timeout */
    bool enable_limit;       /* Robustness 4: false if --no-limit */
    nsp_log_level_t log_level;
} nsp_server_config_t;

typedef struct {
    nsp_server_config_t config;
    int listen_fd;
    pid_t parent_pid;
    int active_clients;
    pid_t *child_pids;
    size_t child_pids_cap;
} nsp_server_context_t;

void nsp_server_config_init_defaults(nsp_server_config_t *cfg);
int nsp_server_run(const nsp_server_config_t *cfg);

#endif /* NSP_SERVER_H */
