#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "nsp/nsp.h"

/* Simple test framework macros */
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

/* Test 1: Header pack, unpack, and endianness conversion */
static void test_header_pack_unpack(void) {
    nsp_header_t orig = {
        .magic = NSP_MAGIC_VAL,
        .version = NSP_VERSION_VAL,
        .msg_type = NSP_MSG_ECHO,
        .payload_len = 1234
    };

    uint8_t wire[NSP_HEADER_SIZE];
    nsp_header_pack(&orig, wire);

    /* Verify big-endian wire format: magic 0x4E53 -> wire[0]=0x4E, wire[1]=0x53 */
    TEST_ASSERT(wire[0] == 0x4E, "magic byte 0 should be 0x4E");
    TEST_ASSERT(wire[1] == 0x53, "magic byte 1 should be 0x53");
    TEST_ASSERT(wire[2] == 0x01, "version byte should be 1");
    TEST_ASSERT(wire[3] == NSP_MSG_ECHO, "msg_type byte should match");
    /* 1234 = 0x000004D2 in hex -> wire[4]=0, wire[5]=0, wire[6]=4, wire[7]=0xD2 */
    TEST_ASSERT(wire[4] == 0x00, "len byte 0");
    TEST_ASSERT(wire[5] == 0x00, "len byte 1");
    TEST_ASSERT(wire[6] == 0x04, "len byte 2");
    TEST_ASSERT(wire[7] == 0xD2, "len byte 3");

    nsp_header_t unpacked;
    memset(&unpacked, 0, sizeof(unpacked));
    nsp_header_unpack(wire, &unpacked);

    TEST_ASSERT(unpacked.magic == orig.magic, "unpacked magic must match");
    TEST_ASSERT(unpacked.version == orig.version, "unpacked version must match");
    TEST_ASSERT(unpacked.msg_type == orig.msg_type, "unpacked msg_type must match");
    TEST_ASSERT(unpacked.payload_len == orig.payload_len, "unpacked payload_len must match");
}

/* Test 2: Header validation checks */
static void test_header_validation(void) {
    nsp_header_t valid = {
        .magic = NSP_MAGIC_VAL,
        .version = NSP_VERSION_VAL,
        .msg_type = NSP_MSG_ECHO,
        .payload_len = 100
    };
    TEST_ASSERT(nsp_header_is_valid(&valid), "valid header should pass validation");

    nsp_header_t bad_magic = valid;
    bad_magic.magic = 0x1234;
    TEST_ASSERT(!nsp_header_is_valid(&bad_magic), "bad magic should fail validation");

    nsp_header_t bad_version = valid;
    bad_version.version = 2;
    TEST_ASSERT(!nsp_header_is_valid(&bad_version), "bad version should fail validation");

    nsp_header_t bad_type = valid;
    bad_type.msg_type = 0x7F;
    TEST_ASSERT(!nsp_header_is_valid(&bad_type), "invalid msg_type should fail validation");

    nsp_header_t oversized = valid;
    oversized.payload_len = NSP_MAX_PAYLOAD + 1;
    TEST_ASSERT(!nsp_header_is_valid(&oversized), "oversized payload should fail validation");
}

/* Test 3: Basic frame send and receive over socketpair */
static void test_frame_send_recv_socketpair(void) {
    int sv[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT(rc == 0, "socketpair creation");

    const char *msg = "Hello, Network & System Programming!";
    uint32_t len = (uint32_t)strlen(msg);

    rc = nsp_send_frame(sv[0], NSP_MSG_ECHO, msg, len);
    TEST_ASSERT(rc == NSP_SUCCESS, "nsp_send_frame failed");

    nsp_header_t hdr;
    char *payload = NULL;
    rc = nsp_recv_frame(sv[1], &hdr, &payload, true);
    TEST_ASSERT(rc == NSP_SUCCESS, "nsp_recv_frame failed");
    TEST_ASSERT(hdr.magic == NSP_MAGIC_VAL, "header magic matches");
    TEST_ASSERT(hdr.version == NSP_VERSION_VAL, "header version matches");
    TEST_ASSERT(hdr.msg_type == NSP_MSG_ECHO, "header msg_type matches");
    TEST_ASSERT(hdr.payload_len == len, "header payload_len matches");
    TEST_ASSERT(payload != NULL, "payload buffer allocated");
    TEST_ASSERT(strcmp(payload, msg) == 0, "payload contents match");

    nsp_frame_free(payload);
    close(sv[0]);
    close(sv[1]);
}

/* Test 4: Fragmented 1-byte-at-a-time reassembly across stream */
static void test_fragmented_1byte_stream_reassembly(void) {
    int sv[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT(rc == 0, "socketpair creation");

    const char *payload_text = "Tracer bullet fragmented reassembly test string.";
    uint32_t payload_len = (uint32_t)strlen(payload_text);

    /* Construct wire bytes manually: 8 byte header + payload */
    nsp_header_t hdr_send = {
        .magic = NSP_MAGIC_VAL,
        .version = NSP_VERSION_VAL,
        .msg_type = NSP_MSG_SYSINFO,
        .payload_len = payload_len
    };
    uint8_t wire[NSP_HEADER_SIZE + payload_len];
    nsp_header_pack(&hdr_send, wire);
    memcpy(wire + NSP_HEADER_SIZE, payload_text, payload_len);

    /* Fork a child process to send 1 byte at a time with tiny usleep to force fragmentation */
    pid_t pid = fork();
    TEST_ASSERT(pid >= 0, "fork for fragmented writer");

    if (pid == 0) {
        /* Child writer */
        close(sv[1]);
        size_t total = sizeof(wire);
        for (size_t i = 0; i < total; i++) {
            ssize_t w = write(sv[0], &wire[i], 1);
            if (w != 1) {
                exit(1);
            }
            usleep(200); /* 0.2ms pause between each byte */
        }
        close(sv[0]);
        exit(0);
    }

    /* Parent reader */
    close(sv[0]);
    nsp_header_t hdr_recv;
    char *payload_recv = NULL;

    rc = nsp_recv_frame(sv[1], &hdr_recv, &payload_recv, true);
    TEST_ASSERT(rc == NSP_SUCCESS, "nsp_recv_frame failed on fragmented stream");
    TEST_ASSERT(hdr_recv.msg_type == NSP_MSG_SYSINFO, "correct msg_type received");
    TEST_ASSERT(hdr_recv.payload_len == payload_len, "correct payload length received");
    TEST_ASSERT(payload_recv != NULL, "payload allocated");
    TEST_ASSERT(strcmp(payload_recv, payload_text) == 0, "reassembled payload matches byte-for-byte");

    nsp_frame_free(payload_recv);
    close(sv[1]);

    int status = 0;
    waitpid(pid, &status, 0);
    TEST_ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0, "child writer exited cleanly");
}

/* Test 5: Rejection of bad magic and oversized payload */
static void test_malformed_rejection(void) {
    int sv[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT(rc == 0, "socketpair creation");

    /* Send bad magic header */
    uint8_t bad_wire[NSP_HEADER_SIZE] = { 0x00, 0x00, 1, NSP_MSG_ECHO, 0, 0, 0, 0 };
    rc = write(sv[0], bad_wire, NSP_HEADER_SIZE);
    TEST_ASSERT(rc == NSP_HEADER_SIZE, "write bad magic wire");

    nsp_header_t hdr;
    char *payload = NULL;
    rc = nsp_recv_frame(sv[1], &hdr, &payload, true);
    TEST_ASSERT(rc == NSP_ERR_MAGIC, "recv_frame must reject bad magic with NSP_ERR_MAGIC");
    TEST_ASSERT(payload == NULL, "payload must not be allocated on validation error");

    close(sv[0]);
    close(sv[1]);
}

/* Test 6: Clean EOF handling */
static void test_clean_eof_handling(void) {
    int sv[2];
    int rc = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    TEST_ASSERT(rc == 0, "socketpair creation");

    close(sv[0]); /* Sender immediately closes */

    nsp_header_t hdr;
    char *payload = NULL;
    rc = nsp_recv_frame(sv[1], &hdr, &payload, true);
    TEST_ASSERT(rc == NSP_ERR_EOF, "recv_frame must return NSP_ERR_EOF on closed socket");
    TEST_ASSERT(payload == NULL, "payload must be NULL on EOF");

    close(sv[1]);
}

int main(void) {
    printf("=== Running libnsp Framing & Stream I/O Tests ===\n");
    RUN_TEST(test_header_pack_unpack);
    RUN_TEST(test_header_validation);
    RUN_TEST(test_frame_send_recv_socketpair);
    RUN_TEST(test_fragmented_1byte_stream_reassembly);
    RUN_TEST(test_malformed_rejection);
    RUN_TEST(test_clean_eof_handling);

    printf("\nTests run: %d, Failures: %d\n", g_tests_run, g_tests_failed);
    return (g_tests_failed == 0) ? 0 : 1;
}
