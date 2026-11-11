#ifndef NSP_CLIENT_H
#define NSP_CLIENT_H

#include "nsp/nsp.h"

typedef struct {
    const char *host;
    int port;
    const char *single_command;
    nsp_log_level_t log_level;
} nsp_client_config_t;

int nsp_client_run(const nsp_client_config_t *cfg);

#endif /* NSP_CLIENT_H */
