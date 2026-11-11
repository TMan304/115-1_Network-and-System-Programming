#define _GNU_SOURCE
#include "handlers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>

/* Forward declarations */
static void get_cpu_info(char *model, size_t model_sz, int *cores);
static void get_mem_info(unsigned long *total_kb, unsigned long *free_kb, unsigned long *avail_kb);

/* Built-in command registry */
static const nsp_command_entry_t g_commands[] = {
    {
        .msg_type = NSP_MSG_SYSINFO,
        .name = "sysinfo",
        .description = "Returns host system and hardware metrics",
        .fn = handle_cmd_sysinfo
    },
    {
        .msg_type = NSP_MSG_ECHO,
        .name = "echo",
        .description = "Echoes back the received payload",
        .fn = handle_cmd_echo
    },
    {
        .msg_type = NSP_MSG_TIME,
        .name = "time",
        .description = "Returns server epoch and formatted timestamps",
        .fn = handle_cmd_time
    },
    {
        .msg_type = NSP_MSG_PROCS,
        .name = "procs",
        .description = "Returns worker PID, parent PID, active and system procs",
        .fn = handle_cmd_procs
    },
    {
        .msg_type = NSP_MSG_SLEEP,
        .name = "sleep",
        .description = "Pauses worker process for specified seconds",
        .fn = handle_cmd_sleep
    },
    {
        .msg_type = NSP_MSG_QUIT,
        .name = "quit",
        .description = "Acknowledges session termination",
        .fn = handle_cmd_quit
    }
};

static const size_t g_num_commands = sizeof(g_commands) / sizeof(g_commands[0]);

static void get_cpu_info(char *model, size_t model_sz, int *cores) {
    if (model && model_sz > 0) {
        strncpy(model, "Generic CPU", model_sz);
        model[model_sz - 1] = '\0';
    }
    if (cores) {
        long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        *cores = (nprocs > 0) ? (int)nprocs : 1;
    }

    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return;

    char line[256];
    bool found_model = false;
    int counted_cores = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (!found_model && strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                colon++;
                while (*colon == ' ' || *colon == '\t') colon++;
                size_t len = strlen(colon);
                while (len > 0 && (colon[len - 1] == '\n' || colon[len - 1] == '\r')) {
                    colon[--len] = '\0';
                }
                if (model && model_sz > 0 && len > 0) {
                    strncpy(model, colon, model_sz - 1);
                    model[model_sz - 1] = '\0';
                    found_model = true;
                }
            }
        }
        if (strncmp(line, "processor", 9) == 0) {
            counted_cores++;
        }
    }
    fclose(fp);

    if (cores && counted_cores > 0) {
        *cores = counted_cores;
    }
}

static void get_mem_info(unsigned long *total_kb, unsigned long *free_kb, unsigned long *avail_kb) {
    *total_kb = 0;
    *free_kb = 0;
    *avail_kb = 0;

    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                sscanf(line + 9, "%lu", total_kb);
            } else if (strncmp(line, "MemFree:", 8) == 0) {
                sscanf(line + 8, "%lu", free_kb);
            } else if (strncmp(line, "MemAvailable:", 13) == 0) {
                sscanf(line + 13, "%lu", avail_kb);
            }
        }
        fclose(fp);
    }

    if (*avail_kb == 0) {
        *avail_kb = *free_kb;
    }
}

nsp_handler_status_t handle_cmd_sysinfo(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    (void)ctx;
    (void)req_hdr;
    (void)req_payload;

    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        strncpy(hostname, "unknown", sizeof(hostname));
    }
    hostname[sizeof(hostname) - 1] = '\0';

    struct utsname uts;
    if (uname(&uts) != 0) {
        memset(&uts, 0, sizeof(uts));
        strncpy(uts.sysname, "Linux", sizeof(uts.sysname) - 1);
        strncpy(uts.release, "unknown", sizeof(uts.release) - 1);
        strncpy(uts.machine, "unknown", sizeof(uts.machine) - 1);
    }

    struct sysinfo si;
    long uptime_sec = 0;
    if (sysinfo(&si) == 0) {
        uptime_sec = si.uptime;
    }
    long days = uptime_sec / 86400;
    long hours = (uptime_sec % 86400) / 3600;
    long mins = (uptime_sec % 3600) / 60;
    long secs = uptime_sec % 60;

    double load[3] = {0.0, 0.0, 0.0};
    FILE *fpload = fopen("/proc/loadavg", "r");
    if (fpload) {
        if (fscanf(fpload, "%lf %lf %lf", &load[0], &load[1], &load[2]) != 3) {
            load[0] = load[1] = load[2] = 0.0;
        }
        fclose(fpload);
    }

    char cpu_model[128];
    int cpu_cores = 1;
    get_cpu_info(cpu_model, sizeof(cpu_model), &cpu_cores);

    unsigned long total_kb = 0, free_kb = 0, avail_kb = 0;
    get_mem_info(&total_kb, &free_kb, &avail_kb);

    char report[1024];
    int len = snprintf(report, sizeof(report),
        "Hostname: %s\n"
        "OS: %s %s (%s)\n"
        "Uptime: %ld seconds (%ldd %ldh %ldm %lds)\n"
        "Load Average: %.2f, %.2f, %.2f\n"
        "CPU Model: %s\n"
        "CPU Cores: %d\n"
        "Memory Total: %lu kB (%lu MiB)\n"
        "Memory Free: %lu kB (%lu MiB)\n"
        "Memory Available: %lu kB (%lu MiB)",
        hostname,
        uts.sysname, uts.release, uts.machine,
        uptime_sec, days, hours, mins, secs,
        load[0], load[1], load[2],
        cpu_model, cpu_cores,
        total_kb, total_kb / 1024,
        free_kb, free_kb / 1024,
        avail_kb, avail_kb / 1024
    );

    LOG_DEBUG("Handling SYSINFO request");
    int rc = nsp_send_frame(client_fd, NSP_MSG_RESP_OK, report, (uint32_t)len);
    if (rc != NSP_SUCCESS) {
        LOG_ERROR("Failed to send SYSINFO response");
        return NSP_HANDLER_ERROR;
    }
    return NSP_HANDLER_OK;
}

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

nsp_handler_status_t handle_cmd_time(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    (void)ctx;
    (void)req_hdr;
    (void)req_payload;

    time_t now = time(NULL);
    struct tm tm_local, tm_utc;
    localtime_r(&now, &tm_local);
    gmtime_r(&now, &tm_utc);

    char buf_local[64];
    char buf_utc[64];
    strftime(buf_local, sizeof(buf_local), "%Y-%m-%d %H:%M:%S %Z", &tm_local);
    strftime(buf_utc, sizeof(buf_utc), "%Y-%m-%d %H:%M:%S UTC", &tm_utc);

    char resp[256];
    int len = snprintf(resp, sizeof(resp),
        "Epoch: %ld\n"
        "Local: %s\n"
        "UTC:   %s",
        (long)now, buf_local, buf_utc);

    LOG_DEBUG("Handling TIME request");
    int rc = nsp_send_frame(client_fd, NSP_MSG_RESP_OK, resp, (uint32_t)len);
    if (rc != NSP_SUCCESS) {
        LOG_ERROR("Failed to send TIME response");
        return NSP_HANDLER_ERROR;
    }
    return NSP_HANDLER_OK;
}

nsp_handler_status_t handle_cmd_procs(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    (void)req_hdr;
    (void)req_payload;

    struct sysinfo si;
    int sys_procs = 0;
    if (sysinfo(&si) == 0) {
        sys_procs = (int)si.procs;
    }

    pid_t worker_pid = getpid();
    pid_t parent_pid = (ctx && ctx->parent_pid > 0) ? ctx->parent_pid : getppid();
    int active_clients = ctx ? ctx->active_clients : 1;

    char resp[256];
    int len = snprintf(resp, sizeof(resp),
        "Worker PID: %d\n"
        "Parent PID: %d\n"
        "Active Clients: %d\n"
        "System Processes: %d",
        (int)worker_pid, (int)parent_pid, active_clients, sys_procs);

    LOG_DEBUG("Handling PROCS request (worker=%d, parent=%d, active=%d, procs=%d)",
              (int)worker_pid, (int)parent_pid, active_clients, sys_procs);
    int rc = nsp_send_frame(client_fd, NSP_MSG_RESP_OK, resp, (uint32_t)len);
    if (rc != NSP_SUCCESS) {
        LOG_ERROR("Failed to send PROCS response");
        return NSP_HANDLER_ERROR;
    }
    return NSP_HANDLER_OK;
}

nsp_handler_status_t handle_cmd_sleep(
    const nsp_server_context_t *ctx,
    int client_fd,
    const nsp_header_t *req_hdr,
    const char *req_payload
) {
    (void)ctx;
    (void)req_hdr;

    bool valid = false;
    long sec = 0;

    if (req_payload && *req_payload) {
        const char *p = req_payload;
        while (*p == ' ' || *p == '\t') p++;
        if (*p != '\0') {
            char *endptr = NULL;
            errno = 0;
            sec = strtol(p, &endptr, 10);
            if (errno == 0 && endptr != p) {
                while (*endptr == ' ' || *endptr == '\t' || *endptr == '\r' || *endptr == '\n') {
                    endptr++;
                }
                if (*endptr == '\0' && sec >= 1 && sec <= 60) {
                    valid = true;
                }
            }
        }
    }

    if (!valid) {
        const char *err_msg = "Error: Invalid sleep duration. Must be an integer between 1 and 60 seconds.";
        LOG_WARN("Worker [PID %d] invalid sleep request payload", (int)getpid());
        nsp_send_frame(client_fd, NSP_MSG_RESP_ERROR, err_msg, (uint32_t)strlen(err_msg));
        return NSP_HANDLER_OK;
    }

    LOG_INFO("Worker [PID %d] sleeping for %ld second(s)...", (int)getpid(), sec);
    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    unsigned int rem = (unsigned int)sec;
    while (rem > 0) {
        rem = sleep(rem);
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    long elapsed = (long)(ts_end.tv_sec - ts_start.tv_sec);
    if (elapsed < sec && (ts_end.tv_nsec - ts_start.tv_nsec) >= 500000000L) {
        elapsed++;
    }

    char resp[128];
    int len = snprintf(resp, sizeof(resp), "Worker [PID %d] slept for %ld second(s)", (int)getpid(), elapsed);

    int rc = nsp_send_frame(client_fd, NSP_MSG_RESP_OK, resp, (uint32_t)len);
    if (rc != NSP_SUCCESS) {
        LOG_ERROR("Failed to send SLEEP response");
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
