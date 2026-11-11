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

#define TEST_PORT "19876"

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
        /* Child: execute ./server -p 19876 */
        execl("./build/server", "./build/server", "-p", TEST_PORT, "-l", "warn", (char *)NULL);
        /* If running from build dir */
        execl("./server", "./server", "-p", TEST_PORT, "-l", "warn", (char *)NULL);
        perror("execl server failed");
        exit(1);
    }
    g_server_pid = pid;
    /* Wait 200ms for server to bind */
    usleep(200000);
}

static void stop_test_server(void) {
    if (g_server_pid > 0) {
        kill(g_server_pid, SIGINT);
        int status = 0;
        waitpid(g_server_pid, &status, 0);
        g_server_pid = -1;
    }
}

/* Test 1: Single command mode with echo */
static void test_client_single_cmd_echo(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "./build/client -p %s -c \"echo Hello Tracer Bullet\" 2>&1 || ./client -p %s -c \"echo Hello Tracer Bullet\" 2>&1", TEST_PORT, TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "Hello Tracer Bullet") != NULL, "output contains echoed text");
    TEST_ASSERT(strstr(out, "Goodbye") == NULL, "single command mode must not contaminate stdout with quit message");
    free(out);
}

/* Test 2: Single command mode with help */
static void test_client_single_cmd_help(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "./build/client -p %s -c \"help\" 2>&1 || ./client -p %s -c \"help\" 2>&1", TEST_PORT, TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "echo") != NULL, "help output mentions echo");
    TEST_ASSERT(strstr(out, "quit") != NULL, "help output mentions quit");
    free(out);
}

/* Test 3: Single command mode with quit */
static void test_client_single_cmd_quit(void) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "./build/client -p %s -c \"quit\" 2>&1 || ./client -p %s -c \"quit\" 2>&1", TEST_PORT, TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "client output captured");
    TEST_ASSERT(strstr(out, "Goodbye") != NULL || strstr(out, "BYE") != NULL, "quit output receives farewell response");
    free(out);
}

/* Test 4: Interactive REPL mode via piped stdin */
static void test_client_repl_piped(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "printf \"echo repl_ping\\nhelp\\nquit\\n\" | (./build/client -p %s 2>&1 || ./client -p %s 2>&1)", TEST_PORT, TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "repl output captured");
    TEST_ASSERT(strstr(out, "repl_ping") != NULL, "repl output contains echo response");
    TEST_ASSERT(strstr(out, "help") != NULL || strstr(out, "Available commands") != NULL, "repl output contains help");
    free(out);
}

/* Test 5: Interactive REPL mode terminated by EOF (Ctrl-D) without quit command */
static void test_client_repl_eof(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "printf \"echo repl_eof_ping\\n\" | (./build/client -p %s 2>&1 || ./client -p %s 2>&1)", TEST_PORT, TEST_PORT);
    char *out = run_command_output(cmd);
    TEST_ASSERT(out != NULL, "repl eof output captured");
    TEST_ASSERT(strstr(out, "repl_eof_ping") != NULL, "repl output contains echo response");
    TEST_ASSERT(strstr(out, "Exiting session") != NULL, "repl cleanly handles EOF");
    free(out);
}

/* Test 5: ldd verification of dynamic linking and RPATH */
static void test_ldd_rpath_resolution(void) {
    char *out = run_command_output("ldd ./build/server 2>&1 || ldd ./server 2>&1");
    TEST_ASSERT(out != NULL, "ldd output captured");
    TEST_ASSERT(strstr(out, "libnsp.so") != NULL, "server dynamically links to libnsp.so");
    TEST_ASSERT(strstr(out, "not found") == NULL, "libnsp.so resolved without errors via RPATH");
    free(out);
}

int main(void) {
    printf("=== Running End-to-End Tracer Bullet Tests ===\n");
    start_test_server();

    RUN_TEST(test_client_single_cmd_echo);
    RUN_TEST(test_client_single_cmd_help);
    RUN_TEST(test_client_single_cmd_quit);
    RUN_TEST(test_client_repl_piped);
    RUN_TEST(test_client_repl_eof);
    RUN_TEST(test_ldd_rpath_resolution);

    stop_test_server();
    printf("\nTests run: %d, Failures: %d\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? 0 : 1;
}
