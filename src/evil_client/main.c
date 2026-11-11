#define _GNU_SOURCE
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

#include "nsp/nsp.h"

#define NSP_DEFAULT_PORT 9000

static void print_usage(const char *prog) {
    printf("Usage: %s [OPTIONS] [MODE]\n", prog);
    printf("Options:\n");
    printf("  -h, --host <host>       Target server host (default: 127.0.0.1)\n");
    printf("  -p, --port <port>       Target server port (default: %d)\n", NSP_DEFAULT_PORT);
    printf("  --hold <ms>             Connection hold time in ms for --flood (default: 500)\n");
    printf("Modes:\n");
    printf("  --bad-magic             Send header with invalid magic (0xDEAD)\n");
    printf("  --oversized             Send header with oversized payload length (0x7FFFFFFF)\n");
    printf("  --truncated             Send header claiming 64B payload, send only 4B, and close write half\n");
    printf("  --half-open             Send 3 bytes of partial header and abruptly disconnect\n");
    printf("  --idle [sec]            Open silent connection and wait up to [sec] seconds (default: 3)\n");
    printf("  --flood <count>         Open <count> simultaneous connections to test connection limit\n");
    printf("  --zombie-burst <count>  Open <count> rapid sequential quit connections\n");
    printf("  --help                  Show this help message\n");
}

static int connect_to_server(const char *host, int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_raw_and_check_error(
    const char *host,
    int port,
    const uint8_t *buf,
    size_t len,
    const char *reject_prefix,
    const char *disconnect_msg
) {
    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        return 1;
    }

    if (nsp_send_all(fd, buf, len) != (ssize_t)len) {
        printf("%s\n", disconnect_msg);
        close(fd);
        return 2;
    }

    nsp_header_t resp_hdr;
    char *resp_payload = NULL;
    int rc = nsp_recv_frame(fd, &resp_hdr, &resp_payload, true);

    if (rc == NSP_SUCCESS && resp_hdr.msg_type == NSP_MSG_RESP_ERROR) {
        printf("%s: %s\n", reject_prefix, resp_payload ? resp_payload : "");
        nsp_frame_free(resp_payload);
        close(fd);
        return 0;
    }

    printf("%s\n", disconnect_msg);
    if (resp_payload) {
        nsp_frame_free(resp_payload);
    }
    close(fd);
    return 2;
}

static int run_bad_magic(const char *host, int port) {
    uint8_t packet[12];
    packet[0] = 0xDE;
    packet[1] = 0xAD;
    packet[2] = NSP_VERSION_VAL;
    packet[3] = NSP_MSG_ECHO;
    uint32_t len = htonl(4);
    memcpy(&packet[4], &len, 4);
    memcpy(&packet[8], "test", 4);

    return send_raw_and_check_error(
        host, port, packet, sizeof(packet),
        "Server rejected bad magic with RESP_ERROR",
        "Server disconnected abruptly (worker crashed/aborted)"
    );
}

static int run_oversized(const char *host, int port) {
    uint8_t header[8];
    uint16_t magic = htons(NSP_MAGIC_VAL);
    memcpy(&header[0], &magic, 2);
    header[2] = NSP_VERSION_VAL;
    header[3] = NSP_MSG_ECHO;
    uint32_t len = htonl(0x7FFFFFFF);
    memcpy(&header[4], &len, 4);

    return send_raw_and_check_error(
        host, port, header, sizeof(header),
        "Server rejected oversized frame with RESP_ERROR",
        "Server disconnected abruptly on oversized frame"
    );
}

static int run_truncated(const char *host, int port) {
    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        return 1;
    }

    uint8_t packet[12];
    uint16_t magic = htons(NSP_MAGIC_VAL);
    memcpy(&packet[0], &magic, 2);
    packet[2] = NSP_VERSION_VAL;
    packet[3] = NSP_MSG_ECHO;
    uint32_t len = htonl(64); /* Claim 64 bytes, only send 4 */
    memcpy(&packet[4], &len, 4);
    memcpy(&packet[8], "part", 4);

    (void)nsp_send_all(fd, packet, sizeof(packet));
    shutdown(fd, SHUT_WR);

    nsp_header_t resp_hdr;
    char *resp_payload = NULL;
    int rc = nsp_recv_frame(fd, &resp_hdr, &resp_payload, true);
    if (rc == NSP_SUCCESS && resp_hdr.msg_type == NSP_MSG_RESP_ERROR) {
        printf("Server rejected truncated frame with RESP_ERROR: %s\n", resp_payload ? resp_payload : "");
        nsp_frame_free(resp_payload);
        close(fd);
        return 0;
    }
    if (resp_payload) nsp_frame_free(resp_payload);
    close(fd);
    printf("Server closed connection on truncated frame\n");
    return 0;
}

static int run_half_open(const char *host, int port) {
    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        return 1;
    }
    const uint8_t partial_hdr[3] = { 0x4E, 0x53, 0x01 };
    (void)nsp_send_all(fd, partial_hdr, sizeof(partial_hdr));
    close(fd);
    printf("Sent 3-byte partial header and disconnected abruptly\n");
    return 0;
}

static int run_idle(const char *host, int port, int sec) {
    if (sec <= 0) sec = 3;

    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%d: %s\n", host, port, strerror(errno));
        return 1;
    }

    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO");
        close(fd);
        return 1;
    }

    nsp_header_t resp_hdr;
    char *resp_payload = NULL;
    int rc = nsp_recv_frame(fd, &resp_hdr, &resp_payload, true);

    if (rc == NSP_SUCCESS && resp_hdr.msg_type == NSP_MSG_RESP_ERROR) {
        printf("Server timed out idle client with RESP_ERROR: %s\n", resp_payload ? resp_payload : "");
        nsp_frame_free(resp_payload);
        close(fd);
        return 0;
    }

    if (rc != NSP_SUCCESS && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        printf("Connection remained open for %ds (no server timeout)\n", sec);
        if (resp_payload) {
            nsp_frame_free(resp_payload);
        }
        close(fd);
        return 3;
    }

    if (resp_payload) {
        nsp_frame_free(resp_payload);
    }
    close(fd);
    printf("Connection closed unexpectedly\n");
    return 1;
}

static int run_flood(const char *host, int port, int count, int hold_ms) {
    if (count <= 0) count = 1;
    if (hold_ms < 0) hold_ms = 500;

    int *fds = malloc((size_t)count * sizeof(int));
    if (!fds) {
        fprintf(stderr, "Failed to allocate memory for flood sockets\n");
        return 1;
    }
    for (int i = 0; i < count; i++) {
        fds[i] = -1;
    }

    int accepted = 0;
    int busy = 0;

    for (int i = 0; i < count; i++) {
        fds[i] = connect_to_server(host, port);
        if (fds[i] < 0) {
            busy++;
            continue;
        }

        /* Check if server immediately sent RESP_BUSY */
        struct pollfd pfd;
        pfd.fd = fds[i];
        pfd.events = POLLIN;
        pfd.revents = 0;
        int pr = poll(&pfd, 1, 50);

        if (pr > 0 && (pfd.revents & POLLIN)) {
            nsp_header_t hdr;
            char *payload = NULL;
            int r_rc = nsp_recv_frame(fds[i], &hdr, &payload, true);
            if (r_rc == NSP_SUCCESS && hdr.msg_type == NSP_MSG_RESP_BUSY) {
                busy++;
                nsp_frame_free(payload);
                continue;
            }
            if (payload) nsp_frame_free(payload);
        }

        /* Not immediately rejected; verify active worker by sending echo request */
        int s_rc = nsp_send_frame(fds[i], NSP_MSG_ECHO, "flood", 5);
        if (s_rc != NSP_SUCCESS) {
            busy++;
            continue;
        }

        struct timeval tv;
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        setsockopt(fds[i], SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        nsp_header_t hdr;
        char *payload = NULL;
        int r_rc = nsp_recv_frame(fds[i], &hdr, &payload, true);
        if (r_rc == NSP_SUCCESS) {
            if (hdr.msg_type == NSP_MSG_RESP_BUSY) {
                busy++;
            } else if (hdr.msg_type == NSP_MSG_RESP_OK) {
                accepted++;
            } else {
                accepted++;
            }
            nsp_frame_free(payload);
        } else {
            busy++;
        }
    }

    if (hold_ms > 0) {
        usleep((useconds_t)hold_ms * 1000);
    }

    for (int i = 0; i < count; i++) {
        if (fds[i] >= 0) {
            close(fds[i]);
        }
    }
    free(fds);

    printf("Flood result: accepted=%d busy=%d\n", accepted, busy);
    return 0;
}

static int run_zombie_burst(const char *host, int port, int count) {
    if (count <= 0) count = 1;

    for (int i = 0; i < count; i++) {
        int fd = connect_to_server(host, port);
        if (fd < 0) {
            fprintf(stderr, "Failed to connect on burst iteration %d\n", i);
            return 1;
        }

        nsp_send_frame(fd, NSP_MSG_QUIT, NULL, 0);

        nsp_header_t hdr;
        char *payload = NULL;
        nsp_recv_frame(fd, &hdr, &payload, true);
        if (payload) {
            nsp_frame_free(payload);
        }
        close(fd);
    }
    return 0;
}

typedef enum {
    MODE_NONE = 0,
    MODE_BAD_MAGIC,
    MODE_OVERSIZED,
    MODE_TRUNCATED,
    MODE_HALF_OPEN,
    MODE_IDLE,
    MODE_FLOOD,
    MODE_ZOMBIE_BURST
} evil_mode_t;

int main(int argc, char **argv) {
    const char *host = "127.0.0.1";
    int port = NSP_DEFAULT_PORT;
    int hold_ms = 500;
    int count = 1;
    int idle_sec = 3;
    evil_mode_t mode = MODE_NONE;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                host = argv[++i];
            } else {
                print_usage(argv[0]);
                return 0;
            }
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for port\n");
                return 1;
            }
            port = atoi(argv[i]);
        } else if (strcmp(argv[i], "--hold") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Error: Missing argument for hold\n");
                return 1;
            }
            hold_ms = atoi(argv[i]);
        } else if (strcmp(argv[i], "--bad-magic") == 0) {
            mode = MODE_BAD_MAGIC;
        } else if (strcmp(argv[i], "--oversized") == 0) {
            mode = MODE_OVERSIZED;
        } else if (strcmp(argv[i], "--truncated") == 0) {
            mode = MODE_TRUNCATED;
        } else if (strcmp(argv[i], "--half-open") == 0) {
            mode = MODE_HALF_OPEN;
        } else if (strcmp(argv[i], "--idle") == 0) {
            mode = MODE_IDLE;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                idle_sec = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--flood") == 0) {
            mode = MODE_FLOOD;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                count = atoi(argv[++i]);
            } else {
                count = 5;
            }
        } else if (strcmp(argv[i], "--zombie-burst") == 0) {
            mode = MODE_ZOMBIE_BURST;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                count = atoi(argv[++i]);
            } else {
                count = 3;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    switch (mode) {
        case MODE_BAD_MAGIC:
            return run_bad_magic(host, port);
        case MODE_OVERSIZED:
            return run_oversized(host, port);
        case MODE_TRUNCATED:
            return run_truncated(host, port);
        case MODE_HALF_OPEN:
            return run_half_open(host, port);
        case MODE_IDLE:
            return run_idle(host, port, idle_sec);
        case MODE_FLOOD:
            return run_flood(host, port, count, hold_ms);
        case MODE_ZOMBIE_BURST:
            return run_zombie_burst(host, port, count);
        default:
            fprintf(stderr, "Error: No evil client mode specified\n");
            print_usage(argv[0]);
            return 1;
    }
}
