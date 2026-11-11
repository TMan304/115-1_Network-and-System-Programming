#ifndef NSP_IO_H
#define NSP_IO_H

#include <sys/types.h>
#include <stddef.h>
#include <stdbool.h>
#include "nsp/protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Send exactly len bytes over fd, handling partial writes and retrying on EINTR.
 *
 * @param fd Socket file descriptor.
 * @param buf Pointer to data to send.
 * @param len Total number of bytes to send.
 * @return Number of bytes sent (len) on success, or -1 on error with errno set.
 */
ssize_t nsp_send_all(int fd, const void *buf, size_t len);

/**
 * Receive exactly len bytes from fd, handling partial reads and retrying on EINTR.
 *
 * @param fd Socket file descriptor.
 * @param buf Destination buffer.
 * @param len Total number of bytes to read.
 * @return len on success, 0 on clean EOF before any bytes read, or -1 on error/timeout.
 */
ssize_t nsp_recv_all(int fd, void *buf, size_t len);

/**
 * Pack an 8-byte header and send header + payload reliably over fd.
 *
 * @param fd Socket file descriptor.
 * @param msg_type Protocol message opcode.
 * @param payload Pointer to payload buffer (can be NULL if payload_len == 0).
 * @param payload_len Payload length in bytes (must not exceed NSP_MAX_PAYLOAD under normal operation).
 * @return NSP_SUCCESS (0) on success, or negative error code (e.g. NSP_ERR_IO).
 */
int nsp_send_frame(int fd, uint8_t msg_type, const void *payload, uint32_t payload_len);

/**
 * Receive a binary frame over fd, unpacking the header and allocating payload.
 *
 * If validate_enabled is true, verifies magic == 0x4E53, version == 1, valid message type,
 * and payload_len <= NSP_MAX_PAYLOAD before allocating memory or reading payload.
 *
 * On success, *payload_out is allocated with malloc(hdr.payload_len + 1) and null-terminated.
 * Caller is responsible for calling nsp_frame_free(*payload_out).
 *
 * @param fd Socket file descriptor.
 * @param hdr_out Pointer to header struct to fill.
 * @param payload_out Pointer to char* to store allocated payload buffer.
 * @param validate_enabled Whether to enforce strict header and length validation.
 * @return NSP_SUCCESS (0) on success, or negative error code (e.g. NSP_ERR_EOF, NSP_ERR_MAGIC).
 */
int nsp_recv_frame(int fd, nsp_header_t *hdr_out, char **payload_out, bool validate_enabled);

/**
 * Free a payload buffer allocated by nsp_recv_frame.
 *
 * @param payload Pointer to payload buffer (safe to pass NULL).
 */
void nsp_frame_free(char *payload);

#ifdef __cplusplus
}
#endif

#endif /* NSP_IO_H */
