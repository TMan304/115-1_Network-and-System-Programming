#include "nsp/io.h"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

ssize_t nsp_send_all(int fd, const void *buf, size_t len) {
    if (fd < 0 || (!buf && len > 0)) {
        errno = EINVAL;
        return -1;
    }

    const char *ptr = (const char *)buf;
    size_t total_sent = 0;

    while (total_sent < len) {
        ssize_t n = send(fd, ptr + total_sent, len - total_sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            /* Peer closed connection unexpectedly */
            errno = EPIPE;
            return -1;
        }
        total_sent += (size_t)n;
    }

    return (ssize_t)total_sent;
}

ssize_t nsp_recv_all(int fd, void *buf, size_t len) {
    if (fd < 0 || (!buf && len > 0)) {
        errno = EINVAL;
        return -1;
    }
    if (len == 0) {
        return 0;
    }

    char *ptr = (char *)buf;
    size_t total_read = 0;

    while (total_read < len) {
        ssize_t n = recv(fd, ptr + total_read, len - total_read, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            if (total_read == 0) {
                /* Clean EOF before reading any bytes */
                return 0;
            }
            /* Truncated stream */
            errno = ECONNRESET;
            return -1;
        }
        total_read += (size_t)n;
    }

    return (ssize_t)total_read;
}

int nsp_send_frame(int fd, uint8_t msg_type, const void *payload, uint32_t payload_len) {
    if (fd < 0 || (payload_len > 0 && !payload)) {
        return NSP_ERR_INVALID_PARAM;
    }

    nsp_header_t hdr = {
        .magic = NSP_MAGIC_VAL,
        .version = NSP_VERSION_VAL,
        .msg_type = msg_type,
        .payload_len = payload_len
    };

    uint8_t wire[NSP_HEADER_SIZE];
    nsp_header_pack(&hdr, wire);

    ssize_t sent = nsp_send_all(fd, wire, NSP_HEADER_SIZE);
    if (sent != NSP_HEADER_SIZE) {
        return NSP_ERR_IO;
    }

    if (payload_len > 0) {
        sent = nsp_send_all(fd, payload, payload_len);
        if (sent != (ssize_t)payload_len) {
            return NSP_ERR_IO;
        }
    }

    return NSP_SUCCESS;
}

int nsp_recv_frame(int fd, nsp_header_t *hdr_out, char **payload_out, bool validate_enabled) {
    if (fd < 0 || !hdr_out || !payload_out) {
        return NSP_ERR_INVALID_PARAM;
    }

    *payload_out = NULL;

    uint8_t wire[NSP_HEADER_SIZE];
    ssize_t r = nsp_recv_all(fd, wire, NSP_HEADER_SIZE);
    if (r == 0) {
        return NSP_ERR_EOF;
    }
    if (r != NSP_HEADER_SIZE) {
        return NSP_ERR_IO;
    }

    nsp_header_unpack(wire, hdr_out);

    if (validate_enabled) {
        int status = nsp_validate_header(hdr_out);
        if (status != NSP_SUCCESS) {
            return status;
        }
    }

    /* Guard against 32-bit integer overflow if validate_enabled is false */
    if (hdr_out->payload_len >= (uint32_t)(~0U)) {
        return NSP_ERR_NOMEM;
    }

    size_t alloc_size = (size_t)hdr_out->payload_len + 1;
    char *buf = malloc(alloc_size);
    if (!buf) {
        return NSP_ERR_NOMEM;
    }

    if (hdr_out->payload_len > 0) {
        r = nsp_recv_all(fd, buf, hdr_out->payload_len);
        if (r != (ssize_t)hdr_out->payload_len) {
            free(buf);
            return NSP_ERR_IO;
        }
    }

    buf[hdr_out->payload_len] = '\0';
    *payload_out = buf;

    return NSP_SUCCESS;
}

void nsp_frame_free(char *payload) {
    if (payload) {
        free(payload);
    }
}
