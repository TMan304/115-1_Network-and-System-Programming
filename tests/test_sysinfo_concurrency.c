#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#define TEST_PORT "19120"
#define CONCURRENT_CLIENTS 12

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAILED [%s:%d]: %s (%s)\n", __FILE__, __LINE__, #cond, msg); \
        g_tests_failed++; \
        return; \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    printf("Running %s... ", #fn); \
    int prev_failed = g_tests_failed; \
    fn(); \
    g_tests_run++; \
    if (g_tests_failed == prev_failed) { \
        printf("PASSED\n"); \
    } else { \
        printf("FAILED\n"); \
    } \
} while (0)

static const char *get_server_bin(void) {
    if (access("./build/server", X_OK) == 0) return "./build/server";
    return "./server";
}

static const char *get_client_bin(void) {
    if (access("./build/client", X_OK) == 0) return "./build/client";
    return "./client";
}

static char *run_command_output(const char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        return NULL;
    }
    size_t cap = 2048;
    size_t len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        pclose(fp);
        return NULL;
    }

    while (fgets(buf + len, (int)(cap - len), fp) != NULL) {
        len = strlen(buf);
        if (cap - len < 256) {
            cap *= 2;
            char *new_buf = realloc(buf, cap);
            if (!new_buf) {
                break;
            }
            buf = new_buf;
        }
    }
    pclose(fp);
    return buf;
}

static pid_t g_server_pid = -1;

static void start_test_server(void) {
    pid_t pid = fork();
    if (pid == 0) {
        const char *s_bin = get_server_bin();
        execl(s_bin, s_bin, "-p", TEST_PORT, "-l", "warn", (char *)NULL);
        perror("execl server failed");
        exit(1);
    }
    g_server_pid = pid;
    usleep(250000); /* 250ms for server to bind */
}

static void stop_test_server(void) {
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGINT);
        int status = 0;
        waitpid(g_server_pid, &status, 0);
        g_server_pid = -1;
    }
}

/* Test 1: sysinfo command verification */
static void test_cmd_sysinfo(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sysinfo\" 2>&1", get_client_bin(), TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "Hostname:") != NULL, "sysinfo contains Hostname");
    TEST_ASSERT(strstr(out, "OS:") != NULL || strstr(out, "Linux") != NULL, "sysinfo contains OS / Kernel info");
    TEST_ASSERT(strstr(out, "Uptime:") != NULL && strstr(out, "seconds") != NULL, "sysinfo contains Uptime");
    TEST_ASSERT(strstr(out, "Load Average:") != NULL, "sysinfo contains Load Average");
    TEST_ASSERT(strstr(out, "CPU Model:") != NULL, "sysinfo contains CPU Model");
    TEST_ASSERT(strstr(out, "CPU Cores:") != NULL, "sysinfo contains CPU Cores");
    TEST_ASSERT(strstr(out, "Memory Total:") != NULL, "sysinfo contains Memory Total");
    TEST_ASSERT(strstr(out, "Memory Free:") != NULL, "sysinfo contains Memory Free");
    TEST_ASSERT(strstr(out, "Memory Available:") != NULL, "sysinfo contains Memory Available");
    free(out);
}

/* Test 2: time command verification */
static void test_cmd_time(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"time\" 2>&1", get_client_bin(), TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "Epoch:") != NULL, "time output contains Epoch timestamp");
    TEST_ASSERT(strstr(out, "Local:") != NULL, "time output contains Local formatted time");
    TEST_ASSERT(strstr(out, "UTC:") != NULL, "time output contains UTC formatted time");

    long epoch_val = 0;
    char *ep = strstr(out, "Epoch:");
    if (ep) sscanf(ep, "Epoch: %ld", &epoch_val);
    TEST_ASSERT(epoch_val > 1700000000L, "epoch timestamp is valid recent epoch");
    free(out);
}

/* Test 3: procs command verification */
static void test_cmd_procs(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"procs\" 2>&1", get_client_bin(), TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "Worker PID:") != NULL, "procs output contains Worker PID");
    TEST_ASSERT(strstr(out, "Parent PID:") != NULL, "procs output contains Parent PID");
    TEST_ASSERT(strstr(out, "Active Clients:") != NULL, "procs output contains Active Clients");
    TEST_ASSERT(strstr(out, "System Processes:") != NULL, "procs output contains System Processes");

    int worker_pid = 0;
    int parent_pid = 0;
    int active_clients = 0;
    int sys_procs = 0;
    char *w = strstr(out, "Worker PID:");
    if (w) sscanf(w, "Worker PID: %d", &worker_pid);
    char *p = strstr(out, "Parent PID:");
    if (p) sscanf(p, "Parent PID: %d", &parent_pid);
    char *a = strstr(out, "Active Clients:");
    if (a) sscanf(a, "Active Clients: %d", &active_clients);
    char *s = strstr(out, "System Processes:");
    if (s) sscanf(s, "System Processes: %d", &sys_procs);

    TEST_ASSERT(worker_pid > 0, "worker PID is positive");
    TEST_ASSERT(parent_pid > 0, "parent PID is positive");
    TEST_ASSERT(worker_pid != parent_pid, "worker PID is distinct from parent PID");
    TEST_ASSERT(parent_pid == g_server_pid, "parent PID matches server PID");
    TEST_ASSERT(active_clients >= 1, "active clients count is at least 1");
    TEST_ASSERT(sys_procs > 0, "system processes count is positive");
    free(out);
}

/* Test 4: sleep command verification */
static void test_cmd_sleep(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sleep 1\" 2>&1", get_client_bin(), TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "slept for 1") != NULL, "sleep output mentions elapsed sleep time");
    TEST_ASSERT(strstr(out, "Worker [PID") != NULL, "sleep output mentions worker PID");
    free(out);

    /* Test invalid sleep parameters */
    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sleep 999\" 2>&1", get_client_bin(), TEST_PORT);
    out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured for sleep 999");
    TEST_ASSERT(strstr(out, "Error") != NULL, "sleep 999 returns error");
    free(out);

    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sleep 0\" 2>&1", get_client_bin(), TEST_PORT);
    out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured for sleep 0");
    TEST_ASSERT(strstr(out, "Error") != NULL, "sleep 0 returns error");
    free(out);

    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sleep -5\" 2>&1", get_client_bin(), TEST_PORT);
    out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured for sleep -5");
    TEST_ASSERT(strstr(out, "Error") != NULL, "sleep -5 returns error");
    free(out);

    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sleep invalid\" 2>&1", get_client_bin(), TEST_PORT);
    out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured for sleep invalid");
    TEST_ASSERT(strstr(out, "Error") != NULL, "sleep invalid returns error");
    free(out);

    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"sleep\" 2>&1", get_client_bin(), TEST_PORT);
    out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured for sleep without arg");
    TEST_ASSERT(strstr(out, "Error") != NULL, "sleep without arg returns error");
    free(out);
}

/* Test 5: Client help displays all 7 commands */
static void test_client_help_all_commands(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s -p %s -c \"help\" 2>&1", get_client_bin(), TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "help output captured");
    TEST_ASSERT(strstr(out, "sysinfo") != NULL, "help lists sysinfo");
    TEST_ASSERT(strstr(out, "echo") != NULL, "help lists echo");
    TEST_ASSERT(strstr(out, "time") != NULL, "help lists time");
    TEST_ASSERT(strstr(out, "procs") != NULL, "help lists procs");
    TEST_ASSERT(strstr(out, "sleep") != NULL, "help lists sleep");
    TEST_ASSERT(strstr(out, "help") != NULL, "help lists help");
    TEST_ASSERT(strstr(out, "quit") != NULL, "help lists quit");
    free(out);
}

/* Test 6: Interactive REPL session running all monitoring commands */
static void test_client_interactive_repl_all_commands(void) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "printf \"sysinfo\\ntime\\nprocs\\nsleep 1\\nhelp\\nquit\\n\" | %s -p %s 2>&1",
        get_client_bin(), TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "repl output captured");
    TEST_ASSERT(strstr(out, "Hostname:") != NULL, "repl output contains sysinfo");
    TEST_ASSERT(strstr(out, "Epoch:") != NULL, "repl output contains time");
    TEST_ASSERT(strstr(out, "Worker PID:") != NULL, "repl output contains procs");
    TEST_ASSERT(strstr(out, "slept for 1") != NULL, "repl output contains sleep");
    TEST_ASSERT(strstr(out, "Available commands:") != NULL, "repl output contains help");
    TEST_ASSERT(strstr(out, "Goodbye") != NULL || strstr(out, "BYE") != NULL, "repl output contains quit farewell");
    free(out);
}

/* Test 7: Concurrency test with 10+ concurrent clients */
static void test_concurrency_12_clients(void) {
    printf("\n  Spawning %d concurrent clients holding simultaneous connections...\n", CONCURRENT_CLIENTS);

    struct timespec ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts_start);

    pid_t client_pids[CONCURRENT_CLIENTS];
    int pipe_fds[CONCURRENT_CLIENTS][2];

    for (int i = 0; i < CONCURRENT_CLIENTS; i++) {
        if (pipe(pipe_fds[i]) < 0) {
            TEST_ASSERT(false, "pipe creation failed");
        }

        pid_t pid = fork();
        if (pid < 0) {
            TEST_ASSERT(false, "fork client failed");
        }

        if (pid == 0) {
            /* Child process: redirect stdout to pipe and run client */
            close(pipe_fds[i][0]);
            dup2(pipe_fds[i][1], STDOUT_FILENO);
            dup2(pipe_fds[i][1], STDERR_FILENO);
            close(pipe_fds[i][1]);

            const char *c_bin = get_client_bin();
            execl(c_bin, c_bin, "-p", TEST_PORT, "-c", "sleep 1", (char *)NULL);
            perror("execl failed");
            _exit(1);
        }

        /* Parent */
        close(pipe_fds[i][1]);
        client_pids[i] = pid;
    }

    /* Wait for all 12 clients to finish and collect worker PIDs */
    int worker_pids[CONCURRENT_CLIENTS];
    memset(worker_pids, 0, sizeof(worker_pids));

    for (int i = 0; i < CONCURRENT_CLIENTS; i++) {
        int status = 0;
        pid_t waited = waitpid(client_pids[i], &status, 0);
        TEST_ASSERT(waited == client_pids[i], "waitpid succeeded for client child");
        TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "client exited successfully with status 0");

        char buf[512];
        ssize_t bytes_read = read(pipe_fds[i][0], buf, sizeof(buf) - 1);
        close(pipe_fds[i][0]);
        TEST_ASSERT(bytes_read > 0, "read output from client pipe");
        buf[bytes_read] = '\0';

        char *pid_ptr = strstr(buf, "Worker [PID ");
        TEST_ASSERT(pid_ptr != NULL, "client output contains worker PID pattern");
        int wpid = 0;
        sscanf(pid_ptr, "Worker [PID %d]", &wpid);
        TEST_ASSERT(wpid > 0, "extracted valid worker PID");
        worker_pids[i] = wpid;
    }

    clock_gettime(CLOCK_MONOTONIC, &ts_end);
    double elapsed_sec = (double)(ts_end.tv_sec - ts_start.tv_sec) +
                         (double)(ts_end.tv_nsec - ts_start.tv_nsec) / 1e9;

    /* Verify all 12 worker PIDs are distinct */
    printf("  Observed %d worker PIDs: ", CONCURRENT_CLIENTS);
    for (int i = 0; i < CONCURRENT_CLIENTS; i++) {
        printf("%d ", worker_pids[i]);
        for (int j = i + 1; j < CONCURRENT_CLIENTS; j++) {
            TEST_ASSERT(worker_pids[i] != worker_pids[j], "each concurrent client must be served by a distinct child worker PID");
        }
    }
    printf("\n  All %d clients served by distinct dedicated child worker processes!\n", CONCURRENT_CLIENTS);
    printf("  Total elapsed time for %d concurrent 1s clients: %.2f seconds\n", CONCURRENT_CLIENTS, elapsed_sec);
    TEST_ASSERT(elapsed_sec < 4.0, "concurrent clients ran in parallel (elapsed time < 4.0s)");
}

int main(void) {
    printf("=== Running System Monitoring and Concurrency Tests ===\n");
    start_test_server();

    RUN_TEST(test_cmd_sysinfo);
    RUN_TEST(test_cmd_time);
    RUN_TEST(test_cmd_procs);
    RUN_TEST(test_cmd_sleep);
    RUN_TEST(test_client_help_all_commands);
    RUN_TEST(test_client_interactive_repl_all_commands);
    RUN_TEST(test_concurrency_12_clients);

    stop_test_server();
    printf("\nTests run: %d, Failures: %d\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? 0 : 1;
}
