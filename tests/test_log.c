#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <regex.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "nsp/nsp.h"

typedef void (*log_action_t)(void *arg);

static int capture_stderr_output(log_action_t action, void *arg, char *out_buf, size_t out_size) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        perror("pipe");
        return -1;
    }

    int saved_stderr = dup(STDERR_FILENO);
    if (saved_stderr < 0) {
        perror("dup");
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (dup2(pipefd[1], STDERR_FILENO) < 0) {
        perror("dup2");
        close(pipefd[0]);
        close(pipefd[1]);
        close(saved_stderr);
        return -1;
    }
    close(pipefd[1]);

    action(arg);
    fflush(stderr);

    dup2(saved_stderr, STDERR_FILENO);
    close(saved_stderr);

    size_t total = 0;
    while (total < out_size - 1) {
        ssize_t n = read(pipefd[0], out_buf + total, out_size - 1 - total);
        if (n <= 0) {
            break;
        }
        total += (size_t)n;
    }
    out_buf[total] = '\0';
    close(pipefd[0]);

    return (int)total;
}

static void emit_all_levels(void *arg) {
    (void)arg;
    LOG_DEBUG("Test debug message 123");
    LOG_INFO("Test info message 456");
    LOG_WARN("Test warn message 789");
    LOG_ERROR("Test error message 000");
}

static void emit_debug_direct(void *arg) {
    (void)arg;
    nsp_log_write(NSP_LOG_DEBUG, "Direct debug message");
}

static int verify_log_format(const char *buffer, pid_t expected_pid) {
    if (!buffer || buffer[0] == '\0') {
        return 0;
    }

    regex_t regex;
    const char *pattern = "^[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2} \\[[0-9]+\\] \\[(DEBUG|INFO|WARN|ERROR)\\] .+$";
    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        fprintf(stderr, "Failed to compile regex\n");
        return -1;
    }

    char *buf_copy = strdup(buffer);
    if (!buf_copy) {
        regfree(&regex);
        return -1;
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "[%d]", (int)expected_pid);

    int line_count = 0;
    char *saveptr = NULL;
    char *line = strtok_r(buf_copy, "\n", &saveptr);
    while (line) {
        if (strlen(line) > 0) {
            line_count++;
            if (regexec(&regex, line, 0, NULL, 0) != 0) {
                fprintf(stderr, "Line failed regex format match: '%s'\n", line);
                free(buf_copy);
                regfree(&regex);
                return -1;
            }
            if (strstr(line, pid_str) == NULL) {
                fprintf(stderr, "Line missing expected PID [%d]: '%s'\n", (int)expected_pid, line);
                free(buf_copy);
                regfree(&regex);
                return -1;
            }
        }
        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(buf_copy);
    regfree(&regex);
    return line_count;
}

static int resolve_client_path(const char *argv0, char *out_path, size_t out_size) {
    if (access("./client", X_OK) == 0) {
        snprintf(out_path, out_size, "./client");
        return 0;
    }
    if (argv0) {
        char dir[1024];
        strncpy(dir, argv0, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';
        char *slash = strrchr(dir, '/');
        if (slash) {
            *slash = '\0';
            snprintf(out_path, out_size, "%s/client", dir);
            if (access(out_path, X_OK) == 0) {
                return 0;
            }
        }
    }
    return -1;
}

static int run_client_subproc(const char *client_path, char *const argv[], const char *env_log_level, char *out_buf, size_t out_size) {
    int pipefd[2];
    if (pipe(pipefd) < 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDERR_FILENO);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        close(pipefd[1]);

        if (env_log_level) {
            setenv("LOG_LEVEL", env_log_level, 1);
        } else {
            unsetenv("LOG_LEVEL");
        }

        execv(client_path, argv);
        _exit(127);
    }

    close(pipefd[1]);
    size_t total = 0;
    while (total < out_size - 1) {
        ssize_t n = read(pipefd[0], out_buf + total, out_size - 1 - total);
        if (n <= 0) break;
        total += (size_t)n;
    }
    out_buf[total] = '\0';
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    return 0;
}

int main(int argc, char **argv) {
    printf("=== Testing Two-Tier Logging Subsystem ===\n");

    /* 1. Setter / Getter & String Parsing tests */
    printf("[Test 1] Level setter, getter, and string parser...\n");
    nsp_log_set_level(NSP_LOG_INFO);
    assert(nsp_log_get_level() == NSP_LOG_INFO);

    nsp_log_set_level(NSP_LOG_DEBUG);
    assert(nsp_log_get_level() == NSP_LOG_DEBUG);

    nsp_log_set_level(NSP_LOG_WARN);
    assert(nsp_log_get_level() == NSP_LOG_WARN);

    nsp_log_set_level(NSP_LOG_ERROR);
    assert(nsp_log_get_level() == NSP_LOG_ERROR);

    nsp_log_set_level(NSP_LOG_OFF);
    assert(nsp_log_get_level() == NSP_LOG_OFF);

    assert(nsp_log_level_from_str("debug") == NSP_LOG_DEBUG);
    assert(nsp_log_level_from_str("DEBUG") == NSP_LOG_DEBUG);
    assert(nsp_log_level_from_str("info") == NSP_LOG_INFO);
    assert(nsp_log_level_from_str("INFO") == NSP_LOG_INFO);
    assert(nsp_log_level_from_str("warn") == NSP_LOG_WARN);
    assert(nsp_log_level_from_str("warning") == NSP_LOG_WARN);
    assert(nsp_log_level_from_str("WARN") == NSP_LOG_WARN);
    assert(nsp_log_level_from_str("error") == NSP_LOG_ERROR);
    assert(nsp_log_level_from_str("ERROR") == NSP_LOG_ERROR);
    assert(nsp_log_level_from_str("off") == NSP_LOG_OFF);
    assert(nsp_log_level_from_str("OFF") == NSP_LOG_OFF);
    assert(nsp_log_level_from_str(NULL) == NSP_LOG_INFO);
    assert(nsp_log_level_from_str("unknown_xyz") == NSP_LOG_INFO);

    /* 2. Environment variable initialization */
    printf("[Test 2] Environment variable LOG_LEVEL initialization...\n");
    setenv("LOG_LEVEL", "debug", 1);
    nsp_log_init_from_env();
    assert(nsp_log_get_level() == NSP_LOG_DEBUG);

    setenv("LOG_LEVEL", "warn", 1);
    nsp_log_init_from_env();
    assert(nsp_log_get_level() == NSP_LOG_WARN);

    setenv("LOG_LEVEL", "error", 1);
    nsp_log_init_from_env();
    assert(nsp_log_get_level() == NSP_LOG_ERROR);

    setenv("LOG_LEVEL", "off", 1);
    nsp_log_init_from_env();
    assert(nsp_log_get_level() == NSP_LOG_OFF);

    unsetenv("LOG_LEVEL");

    /* 3. Compile-Time Gate & Side-Effect Evaluation */
    printf("[Test 3] Compile-time gate and side-effect check...\n");
    int side_effect = 0;
    LOG_DEBUG("Side effect check: %d", ++side_effect);

#if NSP_COMPILE_DEBUG
    printf("  NSP_COMPILE_DEBUG == 1: macro expanded and evaluated\n");
    if (side_effect != 1) {
        fprintf(stderr, "FAIL: Expected side_effect == 1, got %d\n", side_effect);
        return 1;
    }
#else
    printf("  NSP_COMPILE_DEBUG == 0: macro stripped to ((void)0)\n");
    if (side_effect != 0) {
        fprintf(stderr, "FAIL: Expected side_effect == 0, got %d\n", side_effect);
        return 1;
    }
#endif

    /* 4. Compile-time defense-in-depth: direct nsp_log_write(NSP_LOG_DEBUG) when NSP_COMPILE_DEBUG == 0 */
    printf("[Test 4] Compile-time defense-in-depth on direct nsp_log_write()...\n");
    nsp_log_set_level(NSP_LOG_DEBUG);
    char buf[4096];
    memset(buf, 0, sizeof(buf));
    capture_stderr_output(emit_debug_direct, NULL, buf, sizeof(buf));
#if !NSP_COMPILE_DEBUG
    if (strlen(buf) != 0) {
        fprintf(stderr, "FAIL: Expected zero output from nsp_log_write(NSP_LOG_DEBUG) with NSP_COMPILE_DEBUG=0, got: %s\n", buf);
        return 1;
    }
#else
    if (strstr(buf, "[DEBUG]") == NULL || strstr(buf, "Direct debug message") == NULL) {
        fprintf(stderr, "FAIL: Expected debug output with NSP_COMPILE_DEBUG=1, got: %s\n", buf);
        return 1;
    }
#endif

    /* 5. Runtime Level Filtering & Format Verification */
    printf("[Test 5] Runtime level filtering and line format validation...\n");
    pid_t my_pid = getpid();

    /* 5a. Default level: info */
    nsp_log_set_level(NSP_LOG_INFO);
    memset(buf, 0, sizeof(buf));
    capture_stderr_output(emit_all_levels, NULL, buf, sizeof(buf));
    assert(strstr(buf, "Test debug message") == NULL);
    assert(strstr(buf, "Test info message") != NULL);
    assert(strstr(buf, "Test warn message") != NULL);
    assert(strstr(buf, "Test error message") != NULL);
    if (verify_log_format(buf, my_pid) != 3) {
        fprintf(stderr, "FAIL: Format verification failed for level info\n");
        return 1;
    }

    /* 5b. Level: debug */
    nsp_log_set_level(NSP_LOG_DEBUG);
    memset(buf, 0, sizeof(buf));
    capture_stderr_output(emit_all_levels, NULL, buf, sizeof(buf));
#if NSP_COMPILE_DEBUG
    assert(strstr(buf, "Test debug message") != NULL);
    if (verify_log_format(buf, my_pid) != 4) {
        fprintf(stderr, "FAIL: Format verification failed for level debug (ON)\n");
        return 1;
    }
#else
    assert(strstr(buf, "Test debug message") == NULL);
    if (verify_log_format(buf, my_pid) != 3) {
        fprintf(stderr, "FAIL: Format verification failed for level debug (OFF)\n");
        return 1;
    }
#endif
    assert(strstr(buf, "Test info message") != NULL);
    assert(strstr(buf, "Test warn message") != NULL);
    assert(strstr(buf, "Test error message") != NULL);

    /* 5c. Level: warn */
    nsp_log_set_level(NSP_LOG_WARN);
    memset(buf, 0, sizeof(buf));
    capture_stderr_output(emit_all_levels, NULL, buf, sizeof(buf));
    assert(strstr(buf, "Test debug message") == NULL);
    assert(strstr(buf, "Test info message") == NULL);
    assert(strstr(buf, "Test warn message") != NULL);
    assert(strstr(buf, "Test error message") != NULL);
    if (verify_log_format(buf, my_pid) != 2) {
        fprintf(stderr, "FAIL: Format verification failed for level warn\n");
        return 1;
    }

    /* 5d. Level: error */
    nsp_log_set_level(NSP_LOG_ERROR);
    memset(buf, 0, sizeof(buf));
    capture_stderr_output(emit_all_levels, NULL, buf, sizeof(buf));
    assert(strstr(buf, "Test debug message") == NULL);
    assert(strstr(buf, "Test info message") == NULL);
    assert(strstr(buf, "Test warn message") == NULL);
    assert(strstr(buf, "Test error message") != NULL);
    if (verify_log_format(buf, my_pid) != 1) {
        fprintf(stderr, "FAIL: Format verification failed for level error\n");
        return 1;
    }

    /* 5e. Level: off */
    nsp_log_set_level(NSP_LOG_OFF);
    memset(buf, 0, sizeof(buf));
    capture_stderr_output(emit_all_levels, NULL, buf, sizeof(buf));
    if (strlen(buf) != 0) {
        fprintf(stderr, "FAIL: Expected 0 bytes for level off, got: %s\n", buf);
        return 1;
    }

    /* 6. End-to-End CLI Flag & Env Var Check with client binary */
    printf("[Test 6] End-to-end CLI flag and environment variable checks...\n");
    char client_path[1024];
    if (resolve_client_path(argc > 0 ? argv[0] : NULL, client_path, sizeof(client_path)) == 0) {
        printf("  Found client binary at: %s\n", client_path);

        /* 6a. client with -v */
        char *argv_v[] = { client_path, "-h", "127.0.0.1", "-p", "19220", "-v", NULL };
        memset(buf, 0, sizeof(buf));
        run_client_subproc(client_path, argv_v, NULL, buf, sizeof(buf));
#if NSP_COMPILE_DEBUG
        if (strstr(buf, "[DEBUG]") == NULL || strstr(buf, "Client initialized") == NULL) {
            fprintf(stderr, "FAIL: Expected [DEBUG] in client -v output when NSP_COMPILE_DEBUG=1, got: %s\n", buf);
            return 1;
        }
#else
        if (strstr(buf, "[DEBUG]") != NULL) {
            fprintf(stderr, "FAIL: Found [DEBUG] in client -v output when NSP_COMPILE_DEBUG=0, got: %s\n", buf);
            return 1;
        }
#endif
        assert(strstr(buf, "[ERROR]") != NULL);

        /* 6b. client with --verbose */
        char *argv_verbose[] = { client_path, "-h", "127.0.0.1", "-p", "19220", "--verbose", NULL };
        memset(buf, 0, sizeof(buf));
        run_client_subproc(client_path, argv_verbose, NULL, buf, sizeof(buf));
#if NSP_COMPILE_DEBUG
        if (strstr(buf, "[DEBUG]") == NULL || strstr(buf, "Client initialized") == NULL) {
            fprintf(stderr, "FAIL: Expected [DEBUG] in client --verbose output when NSP_COMPILE_DEBUG=1, got: %s\n", buf);
            return 1;
        }
#else
        if (strstr(buf, "[DEBUG]") != NULL) {
            fprintf(stderr, "FAIL: Found [DEBUG] in client --verbose output when NSP_COMPILE_DEBUG=0, got: %s\n", buf);
            return 1;
        }
#endif
        assert(strstr(buf, "[ERROR]") != NULL);

        /* 6c. client with -l error */
        char *argv_err[] = { client_path, "-h", "127.0.0.1", "-p", "19220", "-l", "error", NULL };
        memset(buf, 0, sizeof(buf));
        run_client_subproc(client_path, argv_err, NULL, buf, sizeof(buf));
        assert(strstr(buf, "[DEBUG]") == NULL);
        assert(strstr(buf, "[INFO]") == NULL);
        assert(strstr(buf, "[ERROR]") != NULL);

        /* 6d. client with -l off */
        char *argv_off[] = { client_path, "-h", "127.0.0.1", "-p", "19220", "-l", "off", NULL };
        memset(buf, 0, sizeof(buf));
        run_client_subproc(client_path, argv_off, NULL, buf, sizeof(buf));
        if (strlen(buf) != 0) {
            fprintf(stderr, "FAIL: Expected empty output for client -l off, got: %s\n", buf);
            return 1;
        }

        /* 6e. client with LOG_LEVEL=debug in env */
        char *argv_plain[] = { client_path, "-h", "127.0.0.1", "-p", "19220", NULL };
        memset(buf, 0, sizeof(buf));
        run_client_subproc(client_path, argv_plain, "debug", buf, sizeof(buf));
#if NSP_COMPILE_DEBUG
        if (strstr(buf, "[DEBUG]") == NULL || strstr(buf, "Client initialized") == NULL) {
            fprintf(stderr, "FAIL: Expected [DEBUG] in client with LOG_LEVEL=debug, got: %s\n", buf);
            return 1;
        }
#else
        if (strstr(buf, "[DEBUG]") != NULL) {
            fprintf(stderr, "FAIL: Found [DEBUG] in client with LOG_LEVEL=debug when NSP_COMPILE_DEBUG=0, got: %s\n", buf);
            return 1;
        }
#endif
        assert(strstr(buf, "[ERROR]") != NULL);

        printf("  All CLI flag and env checks passed.\n");
    } else {
        printf("  [Note] Client binary not found from test runner location, skipped CLI subproc check.\n");
    }

    printf("=== All Two-Tier Logging Tests Passed Successfully! ===\n");
    return 0;
}
