#ifndef NSP_HANDLERS_H
#define NSP_HANDLERS_H

#include "nsp/nsp.h"
#include "server.h"

typedef enum {
    NSP_HANDLER_OK = 0,     /* Keep connection open and wait for next command */
    NSP_HANDLER_CLOSE = 1,  /* Close connection cleanly (e.g., quit) */
    NSP_HANDLER_ERROR = -1  /* Protocol or I/O error */
} nsp_handler_status_t;

typedef nsp_handler_status_t (*nsp_command_fn)(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
);

typedef struct {
    uint8_t msg_type;
    const char *name;
    const char *description;
    nsp_command_fn fn;
} nsp_command_entry_t;

/* Dispatch a received request frame to the appropriate command handler */
nsp_handler_status_t nsp_server_dispatch_command(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
);

/* Built-in command handlers */
nsp_handler_status_t handle_cmd_echo(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
);

nsp_handler_status_t handle_cmd_quit(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
);

#endif /* NSP_HANDLERS_H */
