#define _POSIX_C_SOURCE 200809L
#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef int (*nsp_cli_fn)(int sock_fd, const char *arg);

typedef struct {
    const char *name;
    const char *args;
    const char *help_desc;
    nsp_cli_fn fn;
} nsp_cli_command_t;

/* Forward declarations */
static int cli_cmd_sysinfo(int sock_fd, const char *arg);
static int cli_cmd_echo(int sock_fd, const char *arg);
static int cli_cmd_time(int sock_fd, const char *arg);
static int cli_cmd_procs(int sock_fd, const char *arg);
static int cli_cmd_sleep(int sock_fd, const char *arg);
static int cli_cmd_help(int sock_fd, const char *arg);
static int cli_cmd_quit(int sock_fd, const char *arg);

static const nsp_cli_command_t g_cli_commands[] = {
    {
        .name = "sysinfo",
        .args = "",
        .help_desc = "Display host system information",
        .fn = cli_cmd_sysinfo
    },
    {
        .name = "echo",
        .args = "<text>",
        .help_desc = "Echoes back the provided text",
        .fn = cli_cmd_echo
    },
    {
        .name = "time",
        .args = "",
        .help_desc = "Display server clock and timezone",
        .fn = cli_cmd_time
    },
    {
        .name = "procs",
        .args = "",
        .help_desc = "Display server worker and process stats",
        .fn = cli_cmd_procs
    },
    {
        .name = "sleep",
        .args = "<seconds>",
        .help_desc = "Pause worker child for N seconds (1-60)",
        .fn = cli_cmd_sleep
    },
    {
        .name = "help",
        .args = "",
        .help_desc = "Display this help message",
        .fn = cli_cmd_help
    },
    {
        .name = "quit",
        .args = "",
        .help_desc = "Disconnect and exit session",
        .fn = cli_cmd_quit
    }
};

static const size_t g_num_cli_commands = sizeof(g_cli_commands) / sizeof(g_cli_commands[0]);

static int cli_send_simple_cmd(int sock_fd, uint8_t msg_type, const char *arg) {
    const char *msg = arg ? arg : "";
    uint32_t len = (uint32_t)strlen(msg);

    int rc = nsp_send_frame(sock_fd, msg_type, msg, len);
    if (rc != NSP_SUCCESS) {
        fprintf(stderr, "Failed to send request: %d\n", rc);
        return -1;
    }

    nsp_header_t resp_hdr;
    char *resp_payload = NULL;
    rc = nsp_recv_frame(sock_fd, &resp_hdr, &resp_payload, true);
    if (rc != NSP_SUCCESS) {
        fprintf(stderr, "Failed to receive response: %d\n", rc);
        return -1;
    }

    if (resp_hdr.msg_type == NSP_MSG_RESP_OK) {
        printf("%s\n", resp_payload ? resp_payload : "");
    } else {
        if (resp_payload && strlen(resp_payload) > 0) {
            printf("%s\n", resp_payload);
        } else {
            printf("Error response (0x%02X)\n", resp_hdr.msg_type);
        }
    }

    nsp_frame_free(resp_payload);
    return 0;
}

static int cli_cmd_sysinfo(int sock_fd, const char *arg) {
    (void)arg;
    return cli_send_simple_cmd(sock_fd, NSP_MSG_SYSINFO, NULL);
}

static int cli_cmd_time(int sock_fd, const char *arg) {
    (void)arg;
    return cli_send_simple_cmd(sock_fd, NSP_MSG_TIME, NULL);
}

static int cli_cmd_procs(int sock_fd, const char *arg) {
    (void)arg;
    return cli_send_simple_cmd(sock_fd, NSP_MSG_PROCS, NULL);
}

static int cli_cmd_sleep(int sock_fd, const char *arg) {
    return cli_send_simple_cmd(sock_fd, NSP_MSG_SLEEP, arg ? arg : "");
}

static int cli_cmd_echo(int sock_fd, const char *arg) {
    const char *msg = arg ? arg : "";
    uint32_t len = (uint32_t)strlen(msg);

    int rc = nsp_send_frame(sock_fd, NSP_MSG_ECHO, msg, len);
    if (rc != NSP_SUCCESS) {
        fprintf(stderr, "Failed to send echo request: %d\n", rc);
        return -1;
    }

    nsp_header_t resp_hdr;
    char *resp_payload = NULL;
    rc = nsp_recv_frame(sock_fd, &resp_hdr, &resp_payload, true);
    if (rc != NSP_SUCCESS) {
        fprintf(stderr, "Failed to receive response: %d\n", rc);
        return -1;
    }

    if (resp_hdr.msg_type == NSP_MSG_RESP_OK) {
        printf("%s\n", resp_payload ? resp_payload : "");
    } else {
        printf("Error response (0x%02X): %s\n", resp_hdr.msg_type, resp_payload ? resp_payload : "");
    }

    nsp_frame_free(resp_payload);
    return 0;
}

static int cli_cmd_quit(int sock_fd, const char *arg) {
    (void)arg;
    int rc = nsp_send_frame(sock_fd, NSP_MSG_QUIT, NULL, 0);
    if (rc != NSP_SUCCESS) {
        fprintf(stderr, "Failed to send quit request: %d\n", rc);
        return 1;
    }

    nsp_header_t resp_hdr;
    char *resp_payload = NULL;
    rc = nsp_recv_frame(sock_fd, &resp_hdr, &resp_payload, true);
    if (rc == NSP_SUCCESS) {
        if (resp_payload && strlen(resp_payload) > 0) {
            printf("%s\n", resp_payload);
        } else {
            printf("Connection closed by server (BYE).\n");
        }
        nsp_frame_free(resp_payload);
    }

    return 1; /* Signals exit from loop */
}

static int cli_cmd_help(int sock_fd, const char *arg) {
    (void)sock_fd;
    (void)arg;
    printf("\nAvailable commands:\n");
    for (size_t i = 0; i < g_num_cli_commands; i++) {
        if (strlen(g_cli_commands[i].args) > 0) {
            printf("  %-10s %-12s - %s\n",
                   g_cli_commands[i].name,
                   g_cli_commands[i].args,
                   g_cli_commands[i].help_desc);
        } else {
            printf("  %-10s %-12s - %s\n",
                   g_cli_commands[i].name,
                   "",
                   g_cli_commands[i].help_desc);
        }
    }
    printf("\n");
    return 0;
}

static int dispatch_cli_line(int sock_fd, const char *line) {
    if (!line || !*line) {
        return 0;
    }

    /* Extract command word */
    char cmd[64];
    size_t i = 0;
    while (*line && *line != ' ' && *line != '\t' && i < sizeof(cmd) - 1) {
        cmd[i++] = *line++;
    }
    cmd[i] = '\0';

    /* Skip whitespace before argument */
    while (*line == ' ' || *line == '\t') {
        line++;
    }

    for (size_t j = 0; j < g_num_cli_commands; j++) {
        if (strcmp(cmd, g_cli_commands[j].name) == 0) {
            return g_cli_commands[j].fn(sock_fd, line);
        }
    }

    printf("Unknown command: '%s'. Type 'help' for available commands.\n", cmd);
    return 0;
}

int nsp_client_run(const nsp_client_config_t *cfg) {
    if (!cfg) return -1;

    nsp_log_set_level(cfg->log_level);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        LOG_ERROR("Failed to create client socket: %s", strerror(errno));
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)cfg->port);

    if (inet_pton(AF_INET, cfg->host, &serv_addr.sin_addr) <= 0) {
        /* Fallback to gethostbyname if hostname given */
        struct hostent *he = gethostbyname(cfg->host);
        if (!he) {
            LOG_ERROR("Unknown host '%s'", cfg->host);
            close(sock_fd);
            return -1;
        }
        memcpy(&serv_addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
    }

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG_ERROR("Failed to connect to %s:%d: %s", cfg->host, cfg->port, strerror(errno));
        close(sock_fd);
        return -1;
    }

    /* Non-interactive single command mode */
    if (cfg->single_command && strlen(cfg->single_command) > 0) {
        int rc = dispatch_cli_line(sock_fd, cfg->single_command);
        if (rc == 0) {
            /* Silently notify server to cleanly terminate worker without stdout noise */
            nsp_send_frame(sock_fd, NSP_MSG_QUIT, NULL, 0);
            nsp_header_t bye_hdr;
            char *bye_payload = NULL;
            (void)nsp_recv_frame(sock_fd, &bye_hdr, &bye_payload, true);
            nsp_frame_free(bye_payload);
        }
        close(sock_fd);
        return 0;
    }

    /* Interactive REPL mode */
    printf("Connected to NSP server at %s:%d\n", cfg->host, cfg->port);
    printf("Type 'help' for commands, or 'quit' / Ctrl-D to exit.\n");

    char line_buf[1024];
    while (1) {
        printf("nsp> ");
        fflush(stdout);

        if (!fgets(line_buf, sizeof(line_buf), stdin)) {
            /* EOF received (Ctrl-D) */
            printf("\nExiting session...\n");
            cli_cmd_quit(sock_fd, NULL);
            break;
        }

        /* Trim newline characters */
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r')) {
            line_buf[--len] = '\0';
        }

        if (len == 0) {
            continue;
        }

        int rc = dispatch_cli_line(sock_fd, line_buf);
        if (rc == 1) {
            /* Quit commanded */
            break;
        }
    }

    close(sock_fd);
    return 0;
}
