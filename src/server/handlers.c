#include "handlers.h"
#include <string.h>

/* Built-in command registry */
static const nsp_command_entry_t g_commands[] = {
    {
        .msg_type = NSP_MSG_ECHO,
        .name = "echo",
        .description = "Echoes back the received payload",
        .fn = handle_cmd_echo
    },
    {
        .msg_type = NSP_MSG_QUIT,
        .name = "quit",
        .description = "Acknowledges session termination",
        .fn = handle_cmd_quit
    }
};

static const size_t g_num_commands = sizeof(g_commands) / sizeof(g_commands[0]);

nsp_handler_status_t handle_cmd_echo(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    (void)ctx;
    uint32_t len = (req_payload && req_hdr) ? req_hdr->payload_len : 0;
    const char *data = req_payload ? req_payload : "";

    LOG_DEBUG("Handling ECHO request (%u bytes)", len);
    int rc = nsp_send_frame(client_fd, NSP_MSG_RESP_OK, data, len);
    if (rc != NSP_SUCCESS) {
        LOG_ERROR("Failed to send ECHO response");
        return NSP_HANDLER_ERROR;
    }
    return NSP_HANDLER_OK;
}

nsp_handler_status_t handle_cmd_quit(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    (void)ctx;
    (void)req_hdr;
    (void)req_payload;

    const char *farewell = "Goodbye! Session closed.";
    uint32_t len = (uint32_t)strlen(farewell);

    LOG_INFO("Handling QUIT request from client");
    int rc = nsp_send_frame(client_fd, NSP_MSG_RESP_BYE, farewell, len);
    if (rc != NSP_SUCCESS) {
        LOG_WARN("Failed to send QUIT farewell response");
    }
    return NSP_HANDLER_CLOSE;
}

nsp_handler_status_t nsp_server_dispatch_command(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    if (!req_hdr) {
        return NSP_HANDLER_ERROR;
    }

    for (size_t i = 0; i < g_num_commands; i++) {
        if (g_commands[i].msg_type == req_hdr->msg_type) {
            return g_commands[i].fn(ctx, client_fd, req_hdr, req_payload);
        }
    }

    /* Command not implemented or unknown */
    const char *err_msg = "Error: Unsupported or unknown command";
    LOG_WARN("Received unrecognized command msg_type 0x%02X", req_hdr->msg_type);
    nsp_send_frame(client_fd, NSP_MSG_RESP_ERROR, err_msg, (uint32_t)strlen(err_msg));
    return NSP_HANDLER_OK;
}
