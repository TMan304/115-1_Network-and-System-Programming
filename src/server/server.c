#define _POSIX_C_SOURCE 200809L
#include "server.h"
#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static volatile sig_atomic_t g_shutdown = 0;
static volatile sig_atomic_t g_reap_needed = 0;

static void handle_shutdown_signal(int sig) {
    (void)sig;
    g_shutdown = 1;
}

static void handle_sigchld(int sig) {
    (void)sig;
    g_reap_needed = 1;
}

void nsp_server_config_init_defaults(nsp_server_config_t *cfg) {
    if (!cfg) return;
    cfg->port = NSP_DEFAULT_PORT;
    cfg->max_clients = NSP_DEFAULT_MAX_CLIENTS;
    cfg->timeout_sec = NSP_DEFAULT_TIMEOUT_SEC;
    cfg->reap_zombies = true;       /* Enabled by default */
    cfg->validate_packets = true;   /* Enabled by default */
    cfg->enable_timeout = true;     /* Enabled by default */
    cfg->enable_limit = true;       /* Enabled by default */
    cfg->log_level = NSP_LOG_INFO;
}

static void track_child_pid(nsp_server_context_t *ctx, pid_t pid) {
    for (size_t i = 0; i < ctx->child_pids_cap; i++) {
        if (ctx->child_pids[i] <= 0) {
            ctx->child_pids[i] = pid;
            return;
        }
    }
    /* Grow array if full */
    size_t new_cap = ctx->child_pids_cap == 0 ? 32 : ctx->child_pids_cap * 2;
    pid_t *new_arr = realloc(ctx->child_pids, new_cap * sizeof(pid_t));
    if (!new_arr) {
        LOG_ERROR("Failed to allocate memory for tracking child PID");
        return;
    }
    for (size_t i = ctx->child_pids_cap; i < new_cap; i++) {
        new_arr[i] = 0;
    }
    new_arr[ctx->child_pids_cap] = pid;
    ctx->child_pids = new_arr;
    ctx->child_pids_cap = new_cap;
}

static void untrack_child_pid(nsp_server_context_t *ctx, pid_t pid) {
    if (!ctx->child_pids) return;
    for (size_t i = 0; i < ctx->child_pids_cap; i++) {
        if (ctx->child_pids[i] == pid) {
            ctx->child_pids[i] = 0;
            return;
        }
    }
}

static bool is_pid_zombie(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return true;
    }
    char buf[256];
    bool zombie = false;
    if (fgets(buf, sizeof(buf), fp)) {
        const char *rparen = strrchr(buf, ')');
        if (rparen && rparen[1] == ' ' && rparen[2] == 'Z') {
            zombie = true;
        }
    }
    fclose(fp);
    return zombie;
}

static void reap_zombies_if_needed(nsp_server_context_t *ctx) {
    if (!ctx->config.reap_zombies) {
        /* When reaping is disabled, do NOT call waitpid so children remain <defunct> in ps,
         * but inspect /proc/<pid>/stat so active_clients only counts live non-zombie workers. */
        int live = 0;
        for (size_t i = 0; i < ctx->child_pids_cap; i++) {
            if (ctx->child_pids[i] > 0 && !is_pid_zombie(ctx->child_pids[i])) {
                live++;
            }
        }
        ctx->active_clients = live;
        return;
    }

    int status = 0;
    pid_t reaped;
    while ((reaped = waitpid(-1, &status, WNOHANG)) > 0) {
        untrack_child_pid(ctx, reaped);
        if (ctx->active_clients > 0) {
            ctx->active_clients--;
        }
        if (WIFSIGNALED(status)) {
            LOG_WARN("Child worker [PID %d] terminated abnormally by signal %d (active clients: %d)",
                     (int)reaped, WTERMSIG(status), ctx->active_clients);
        } else if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            LOG_WARN("Child worker [PID %d] exited with error status %d (active clients: %d)",
                     (int)reaped, WEXITSTATUS(status), ctx->active_clients);
        } else {
            LOG_INFO("Reaped child worker [PID %d] (active clients: %d)", (int)reaped, ctx->active_clients);
        }
    }
    g_reap_needed = 0;
}

static void run_child_worker(const nsp_server_context_t *parent_ctx, int client_fd, const char *client_ip, int client_port) {
    /* Create worker-local copy of context */
    nsp_server_context_t worker_ctx = *parent_ctx;
    worker_ctx.listen_fd = -1;
    worker_ctx.active_clients = parent_ctx->active_clients + 1;

    LOG_INFO("Worker child [PID %d] serving client %s:%d (fd %d)",
             (int)getpid(), client_ip, client_port, client_fd);

    /* Restore default signal dispositions in child */
    signal(SIGINT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    /* Apply socket receive timeout if enabled */
    if (worker_ctx.config.enable_timeout && worker_ctx.config.timeout_sec > 0) {
        struct timeval tv;
        tv.tv_sec = worker_ctx.config.timeout_sec;
        tv.tv_usec = 0;
        if (setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
            LOG_WARN("Failed to set SO_RCVTIMEO on client socket: %s", strerror(errno));
        }
    }

    while (1) {
        nsp_header_t hdr;
        char *payload = NULL;
        int rc = nsp_recv_frame(client_fd, &hdr, &payload, worker_ctx.config.validate_packets);

        if (rc == NSP_ERR_EOF) {
            LOG_INFO("Worker [PID %d] client closed connection (EOF)", (int)getpid());
            break;
        }

        if (rc != NSP_SUCCESS) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_WARN("Worker [PID %d] client idle timeout exceeded (%ds)",
                         (int)getpid(), worker_ctx.config.timeout_sec);
                const char *timeout_msg = "Error: Client idle receive timeout exceeded";
                nsp_send_frame(client_fd, NSP_MSG_RESP_ERROR, timeout_msg, (uint32_t)strlen(timeout_msg));
            } else if (!worker_ctx.config.validate_packets) {
                LOG_ERROR("Worker [PID %d] UNGUARDED FATAL: invalid header (magic=0x%04X, len=%u) caused worker failure!",
                          (int)getpid(), hdr.magic, hdr.payload_len);
                close(client_fd);
                _exit(1);
            } else {
                LOG_WARN("Worker [PID %d] frame receive failed: %d (%s)",
                         (int)getpid(), rc, strerror(errno));
                const char *err_msg = "Error: Malformed packet or protocol violation";
                nsp_send_frame(client_fd, NSP_MSG_RESP_ERROR, err_msg, (uint32_t)strlen(err_msg));
            }
            break;
        }

        if (!worker_ctx.config.validate_packets && !nsp_header_is_valid(&hdr)) {
            LOG_ERROR("Worker [PID %d] UNGUARDED FATAL: invalid header (magic=0x%04X, len=%u) caused worker failure!",
                      (int)getpid(), hdr.magic, hdr.payload_len);
            nsp_frame_free(payload);
            close(client_fd);
            _exit(1);
        }

        LOG_DEBUG("Worker [PID %d] received request: %s (len=%u)",
                  (int)getpid(), nsp_msg_type_str(hdr.msg_type), hdr.payload_len);

        nsp_handler_status_t status = nsp_server_dispatch_command(&worker_ctx, client_fd, &hdr, payload);
        nsp_frame_free(payload);

        if (status == NSP_HANDLER_CLOSE) {
            LOG_INFO("Worker [PID %d] session terminated cleanly", (int)getpid());
            break;
        }
        if (status == NSP_HANDLER_ERROR) {
            LOG_WARN("Worker [PID %d] command execution failed, terminating connection", (int)getpid());
            break;
        }
    }

    close(client_fd);
    LOG_INFO("Worker [PID %d] exiting", (int)getpid());
    _exit(0);
}

int nsp_server_run(const nsp_server_config_t *cfg) {
    if (!cfg) return -1;

    nsp_server_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.config = *cfg;
    ctx.parent_pid = getpid();
    ctx.listen_fd = -1;

    nsp_log_set_level(cfg->log_level);

    /* Setup signal handling */
    struct sigaction sa_shutdown;
    memset(&sa_shutdown, 0, sizeof(sa_shutdown));
    sa_shutdown.sa_handler = handle_shutdown_signal;
    sigaction(SIGINT, &sa_shutdown, NULL);
    sigaction(SIGTERM, &sa_shutdown, NULL);

    /* Ignore SIGPIPE */
    signal(SIGPIPE, SIG_IGN);

    /* Setup SIGCHLD handler if zombie reaping is enabled */
    if (ctx.config.reap_zombies) {
        struct sigaction sa_chld;
        memset(&sa_chld, 0, sizeof(sa_chld));
        sa_chld.sa_handler = handle_sigchld;
        /* Do NOT set SA_RESTART: allow accept() to be interrupted so zombies are reaped immediately */
        sa_chld.sa_flags = 0;
        sigaction(SIGCHLD, &sa_chld, NULL);
    } else {
        signal(SIGCHLD, SIG_DFL);
        LOG_WARN("Zombie reaping disabled (--no-reap); child workers will remain <defunct>");
    }

    /* Create TCP socket */
    ctx.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx.listen_fd < 0) {
        LOG_ERROR("Failed to create socket: %s", strerror(errno));
        return -1;
    }

    /* Set SO_REUSEADDR */
    int reuse = 1;
    if (setsockopt(ctx.listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        LOG_WARN("Failed to set SO_REUSEADDR: %s", strerror(errno));
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons((uint16_t)ctx.config.port);

    if (bind(ctx.listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        LOG_ERROR("Failed to bind to port %d: %s", ctx.config.port, strerror(errno));
        close(ctx.listen_fd);
        return -1;
    }

    if (listen(ctx.listen_fd, 64) < 0) {
        LOG_ERROR("Failed to listen on socket: %s", strerror(errno));
        close(ctx.listen_fd);
        return -1;
    }

    LOG_INFO("Server started [PID %d] listening on port %d (max_clients=%d, timeout=%ds)",
             (int)ctx.parent_pid, ctx.config.port, ctx.config.max_clients, ctx.config.timeout_sec);

    while (!g_shutdown) {
        reap_zombies_if_needed(&ctx);

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(ctx.listen_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (errno == EINTR) {
                /* Signal interrupted accept, check shutdown and reaping */
                continue;
            }
            LOG_ERROR("accept() failed: %s", strerror(errno));
            break;
        }

        reap_zombies_if_needed(&ctx);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);

        /* Connection limit check (Robustness 4) */
        if (ctx.config.enable_limit && ctx.active_clients >= ctx.config.max_clients) {
            LOG_WARN("Rejecting connection from %s:%d: active clients (%d) >= limit (%d)",
                     client_ip, client_port, ctx.active_clients, ctx.config.max_clients);
            const char *busy_msg = "Server busy: max concurrent connections reached";
            nsp_send_frame(client_fd, NSP_MSG_RESP_BUSY, busy_msg, (uint32_t)strlen(busy_msg));
            close(client_fd);
            continue;
        }

        /* Fork dedicated child process per client */
        pid_t pid = fork();
        if (pid < 0) {
            LOG_ERROR("fork() failed for incoming connection: %s", strerror(errno));
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            /* Child process: close listening descriptor and run worker loop */
            close(ctx.listen_fd);
            run_child_worker(&ctx, client_fd, client_ip, client_port);
            /* Child exits in run_child_worker */
        } else {
            /* Parent process: close client socket and track child */
            close(client_fd);
            track_child_pid(&ctx, pid);
            ctx.active_clients++;
            LOG_INFO("Spawned child worker [PID %d] for client %s:%d (active clients: %d)",
                     (int)pid, client_ip, client_port, ctx.active_clients);
        }
    }

    LOG_INFO("Server initiating graceful shutdown...");
    close(ctx.listen_fd);

    /* Terminate and reap any remaining child workers */
    if (ctx.child_pids) {
        for (size_t i = 0; i < ctx.child_pids_cap; i++) {
            if (ctx.child_pids[i] > 0) {
                LOG_INFO("Sending SIGTERM to child worker [PID %d]", (int)ctx.child_pids[i]);
                kill(ctx.child_pids[i], SIGTERM);
            }
        }
        for (size_t i = 0; i < ctx.child_pids_cap; i++) {
            if (ctx.child_pids[i] > 0) {
                waitpid(ctx.child_pids[i], NULL, 0);
            }
        }
        free(ctx.child_pids);
    }

    LOG_INFO("Server shutdown complete.");
    return 0;
}
