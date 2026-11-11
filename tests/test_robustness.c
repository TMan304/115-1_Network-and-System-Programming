#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <errno.h>

#include "nsp/nsp.h"

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
    fflush(stdout); \
    int prev_failed = g_tests_failed; \
    fn(); \
    g_tests_run++; \
    if (g_tests_failed == prev_failed) { \
        printf("PASSED\n"); \
    } else { \
        printf("FAILED\n"); \
    } \
    fflush(stdout); \
} while (0)

static const char *get_server_bin(void) {
    if (access("./build/server", X_OK) == 0) return "./build/server";
    return "./server";
}

static const char *get_evil_client_bin(void) {
    if (access("./build/evil_client", X_OK) == 0) return "./build/evil_client";
    return "./evil_client";
}

static int count_zombies_of_pid(pid_t parent_pid) {
    DIR *proc = opendir("/proc");
    if (!proc) return -1;
    struct dirent *entry;
    int zombie_count = 0;
    while ((entry = readdir(proc)) != NULL) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        char stat_path[256];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
        FILE *f = fopen(stat_path, "r");
        if (!f) continue;
        char buf[512];
        if (fgets(buf, sizeof(buf), f)) {
            char *closing_paren = strrchr(buf, ')');
            if (closing_paren && closing_paren[1] == ' ') {
                char state = closing_paren[2];
                int ppid = 0;
                if (sscanf(closing_paren + 3, " %d", &ppid) == 1) {
                    if (ppid == parent_pid && state == 'Z') {
                        zombie_count++;
                    }
                }
            }
        }
        fclose(f);
    }
    closedir(proc);
    return zombie_count;
}

static pid_t start_server(const char *port, const char *arg1, const char *arg2, const char *arg3) {
    pid_t pid = fork();
    if (pid == 0) {
        const char *bin = get_server_bin();
        const char *argv[10];
        int idx = 0;
        argv[idx++] = bin;
        argv[idx++] = "-p";
        argv[idx++] = port;
        argv[idx++] = "-l";
        argv[idx++] = "warn";
        if (arg1) argv[idx++] = arg1;
        if (arg2) argv[idx++] = arg2;
        if (arg3) argv[idx++] = arg3;
        argv[idx] = NULL;

        execv(bin, (char *const *)argv);
        perror("execv server failed");
        exit(1);
    }
    /* Wait 200ms for server to bind and start listening */
    usleep(200000);
    return pid;
}

static void stop_server(pid_t pid) {
    if (pid > 0) {
        kill(pid, SIGINT);
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

typedef struct {
    int exit_code;
    char output[1024];
} exec_result_t;

static exec_result_t run_evil(const char *args) {
    exec_result_t res;
    memset(&res, 0, sizeof(res));
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s 2>&1", get_evil_client_bin(), args);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        res.exit_code = -1;
        return res;
    }
    size_t len = 0;
    while (fgets(res.output + len, (int)(sizeof(res.output) - len), fp) != NULL) {
        len = strlen(res.output);
    }
    int status = pclose(fp);
    if (WIFEXITED(status)) {
        res.exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        res.exit_code = 128 + WTERMSIG(status);
    } else {
        res.exit_code = -1;
    }
    return res;
}

/* Test 1a: Guarded Zombie Reaping (default) */
static void test_guarded_zombie_reaping(void) {
    pid_t spid = start_server("19321", NULL, NULL, NULL);
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res = run_evil("-p 19321 --zombie-burst 3");
    TEST_ASSERT(res.exit_code == 0, "zombie-burst succeeded");

    /* Wait 150ms for SIGCHLD and waitpid */
    usleep(150000);

    int zombies = count_zombies_of_pid(spid);
    stop_server(spid);

    TEST_ASSERT(zombies == 0, "guarded mode must leave zero defunct zombie processes");
}

/* Test 1b: Unguarded Zombie Reaping (--no-reap) */
static void test_unguarded_zombie_reaping(void) {
    pid_t spid = start_server("19322", "--no-reap", NULL, NULL);
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res = run_evil("-p 19322 --zombie-burst 3");
    TEST_ASSERT(res.exit_code == 0, "zombie-burst succeeded");

    /* Wait 150ms for children to exit and become zombies */
    usleep(150000);

    int zombies = count_zombies_of_pid(spid);
    stop_server(spid);

    TEST_ASSERT(zombies >= 3, "unguarded --no-reap mode must leave at least 3 defunct zombie processes");
}

/* Test 2a: Guarded Packet Validation (default) */
static void test_guarded_packet_validation(void) {
    pid_t spid = start_server("19323", NULL, NULL, NULL);
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res_magic = run_evil("-p 19323 --bad-magic");
    TEST_ASSERT(res_magic.exit_code == 0, "bad-magic exit code should be 0 in guarded mode");
    TEST_ASSERT(strstr(res_magic.output, "Server rejected bad magic with RESP_ERROR") != NULL,
                "bad-magic output matches");

    exec_result_t res_oversized = run_evil("-p 19323 --oversized");
    TEST_ASSERT(res_oversized.exit_code == 0, "oversized exit code should be 0 in guarded mode");
    TEST_ASSERT(strstr(res_oversized.output, "Server rejected oversized frame with RESP_ERROR") != NULL,
                "oversized output matches");

    stop_server(spid);
}

/* Test 2b: Unguarded Packet Validation (--no-validate) */
static void test_unguarded_packet_validation(void) {
    pid_t spid = start_server("19324", "--no-validate", NULL, NULL);
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res_magic = run_evil("-p 19324 --bad-magic");
    TEST_ASSERT(res_magic.exit_code == 2, "bad-magic exit code should be 2 in --no-validate mode");
    TEST_ASSERT(strstr(res_magic.output, "Server disconnected abruptly (worker crashed/aborted)") != NULL,
                "bad-magic worker crashed/aborted message");

    exec_result_t res_oversized = run_evil("-p 19324 --oversized");
    TEST_ASSERT(res_oversized.exit_code == 2, "oversized exit code should be 2 in --no-validate mode");
    TEST_ASSERT(strstr(res_oversized.output, "Server disconnected abruptly on oversized frame") != NULL,
                "oversized worker disconnected abruptly message");

    stop_server(spid);
}

/* Test 3a: Guarded Client Idle Timeout (-t 1) */
static void test_guarded_idle_timeout(void) {
    pid_t spid = start_server("19325", "-t", "1", NULL);
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res = run_evil("-p 19325 --idle 2");
    TEST_ASSERT(res.exit_code == 0, "idle exit code should be 0 in guarded mode");
    TEST_ASSERT(strstr(res.output, "Server timed out idle client with RESP_ERROR") != NULL,
                "server timed out idle client message");

    stop_server(spid);
}

/* Test 3b: Unguarded Client Idle Timeout (--no-timeout -t 1) */
static void test_unguarded_idle_timeout(void) {
    pid_t spid = start_server("19326", "--no-timeout", "-t", "1");
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res = run_evil("-p 19326 --idle 2");
    TEST_ASSERT(res.exit_code == 3, "idle exit code should be 3 in --no-timeout mode");
    TEST_ASSERT(strstr(res.output, "Connection remained open for 2s (no server timeout)") != NULL,
                "connection remained open message");

    stop_server(spid);
}

/* Test 4a: Guarded Connection Limit (-m 3) */
static void test_guarded_connection_limit(void) {
    pid_t spid = start_server("19327", "-m", "3", NULL);
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res = run_evil("-p 19327 --flood 5");
    TEST_ASSERT(res.exit_code == 0, "flood exit code should be 0");
    TEST_ASSERT(strstr(res.output, "Flood result: accepted=3 busy=2") != NULL,
                "guarded limit output accepted=3 busy=2");

    stop_server(spid);
}

/* Test 4b: Unguarded Connection Limit (--no-limit -m 3) */
static void test_unguarded_connection_limit(void) {
    pid_t spid = start_server("19328", "--no-limit", "-m", "3");
    TEST_ASSERT(spid > 0, "server started");

    exec_result_t res = run_evil("-p 19328 --flood 5");
    TEST_ASSERT(res.exit_code == 0, "flood exit code should be 0");
    TEST_ASSERT(strstr(res.output, "Flood result: accepted=5 busy=0") != NULL,
                "unguarded limit output accepted=5 busy=0");

    stop_server(spid);
}

int main(void) {
    printf("=== Running Four Toggleable Robustness Tests ===\n");
    RUN_TEST(test_guarded_zombie_reaping);
    RUN_TEST(test_unguarded_zombie_reaping);
    RUN_TEST(test_guarded_packet_validation);
    RUN_TEST(test_unguarded_packet_validation);
    RUN_TEST(test_guarded_idle_timeout);
    RUN_TEST(test_unguarded_idle_timeout);
    RUN_TEST(test_guarded_connection_limit);
    RUN_TEST(test_unguarded_connection_limit);

    printf("\nTests run: %d, Failures: %d\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? 0 : 1;
}
