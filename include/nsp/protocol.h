#ifndef NSP_PROTOCOL_H
#define NSP_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define NSP_MAGIC_VAL 0x4E53   /* "NS" in ASCII */
#define NSP_VERSION_VAL 1      /* Protocol Version 1 */
#define NSP_HEADER_SIZE 8      /* Fixed 8-byte binary header */
#define NSP_MAX_PAYLOAD 4096   /* Maximum allowed payload bytes */

/* Protocol message types */
typedef enum {
    /* Requests */
    NSP_MSG_SYSINFO = 0x01,
    NSP_MSG_ECHO    = 0x02,
    NSP_MSG_TIME    = 0x03,
    NSP_MSG_PROCS   = 0x04,
    NSP_MSG_SLEEP   = 0x05,
    NSP_MSG_QUIT    = 0x06,

    /* Responses */
    NSP_MSG_RESP_OK    = 0x80,
    NSP_MSG_RESP_ERROR = 0x81,
    NSP_MSG_RESP_BUSY  = 0x82,
    NSP_MSG_RESP_BYE   = 0x83
} nsp_msg_type_t;

/* In-memory representation of protocol header (host byte order) */
typedef struct {
    uint16_t magic;
    uint8_t version;
    uint8_t msg_type;
    uint32_t payload_len;
} nsp_header_t;

/* Error codes for protocol framing & validation */
typedef enum {
    NSP_SUCCESS             =  0,
    NSP_ERR_EOF             = -1,
    NSP_ERR_IO              = -2,
    NSP_ERR_MAGIC           = -3,
    NSP_ERR_VERSION         = -4,
    NSP_ERR_MSG_TYPE        = -5,
    NSP_ERR_PAYLOAD_TOO_BIG = -6,
    NSP_ERR_NOMEM           = -7,
    NSP_ERR_INVALID_PARAM   = -8
} nsp_status_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Pack in-memory header struct into 8-byte network byte order buffer */
void nsp_header_pack(const nsp_header_t *hdr, uint8_t buf[NSP_HEADER_SIZE]);

/* Unpack 8-byte network byte order buffer into in-memory header struct */
void nsp_header_unpack(const uint8_t buf[NSP_HEADER_SIZE], nsp_header_t *hdr);

/* Validate header fields according to protocol specification */
bool nsp_header_is_valid(const nsp_header_t *hdr);

/* Return specific error status if header fails validation, or NSP_SUCCESS */
nsp_status_t nsp_validate_header(const nsp_header_t *hdr);

/* Helper to convert message type enum to human-readable string */
const char *nsp_msg_type_str(uint8_t msg_type);

#ifdef __cplusplus
}
#endif

#endif /* NSP_PROTOCOL_H */
