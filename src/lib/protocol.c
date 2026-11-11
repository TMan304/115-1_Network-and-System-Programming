#include "nsp/protocol.h"
#include <string.h>

void nsp_header_pack(const nsp_header_t *hdr, uint8_t buf[NSP_HEADER_SIZE]) {
    if (!hdr || !buf) {
        return;
    }
    buf[0] = (uint8_t)((hdr->magic >> 8) & 0xFF);
    buf[1] = (uint8_t)(hdr->magic & 0xFF);
    buf[2] = hdr->version;
    buf[3] = hdr->msg_type;
    buf[4] = (uint8_t)((hdr->payload_len >> 24) & 0xFF);
    buf[5] = (uint8_t)((hdr->payload_len >> 16) & 0xFF);
    buf[6] = (uint8_t)((hdr->payload_len >> 8) & 0xFF);
    buf[7] = (uint8_t)(hdr->payload_len & 0xFF);
}

void nsp_header_unpack(const uint8_t buf[NSP_HEADER_SIZE], nsp_header_t *hdr) {
    if (!buf || !hdr) {
        return;
    }
    hdr->magic = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    hdr->version = buf[2];
    hdr->msg_type = buf[3];
    hdr->payload_len = ((uint32_t)buf[4] << 24) |
                       ((uint32_t)buf[5] << 16) |
                       ((uint32_t)buf[6] << 8)  |
                       (uint32_t)buf[7];
}

nsp_status_t nsp_validate_header(const nsp_header_t *hdr) {
    if (!hdr) {
        return NSP_ERR_INVALID_PARAM;
    }
    if (hdr->magic != NSP_MAGIC_VAL) {
        return NSP_ERR_MAGIC;
    }
    if (hdr->version != NSP_VERSION_VAL) {
        return NSP_ERR_VERSION;
    }
    switch (hdr->msg_type) {
        case NSP_MSG_SYSINFO:
        case NSP_MSG_ECHO:
        case NSP_MSG_TIME:
        case NSP_MSG_PROCS:
        case NSP_MSG_SLEEP:
        case NSP_MSG_QUIT:
        case NSP_MSG_RESP_OK:
        case NSP_MSG_RESP_ERROR:
        case NSP_MSG_RESP_BUSY:
        case NSP_MSG_RESP_BYE:
            break;
        default:
            return NSP_ERR_MSG_TYPE;
    }
    if (hdr->payload_len > NSP_MAX_PAYLOAD) {
        return NSP_ERR_PAYLOAD_TOO_BIG;
    }
    return NSP_SUCCESS;
}

bool nsp_header_is_valid(const nsp_header_t *hdr) {
    return nsp_validate_header(hdr) == NSP_SUCCESS;
}

const char *nsp_msg_type_str(uint8_t msg_type) {
    switch (msg_type) {
        case NSP_MSG_SYSINFO:    return "REQ_SYSINFO";
        case NSP_MSG_ECHO:       return "REQ_ECHO";
        case NSP_MSG_TIME:       return "REQ_TIME";
        case NSP_MSG_PROCS:      return "REQ_PROCS";
        case NSP_MSG_SLEEP:      return "REQ_SLEEP";
        case NSP_MSG_QUIT:       return "REQ_QUIT";
        case NSP_MSG_RESP_OK:    return "RESP_OK";
        case NSP_MSG_RESP_ERROR: return "RESP_ERROR";
        case NSP_MSG_RESP_BUSY:  return "RESP_BUSY";
        case NSP_MSG_RESP_BYE:   return "RESP_BYE";
        default:                 return "UNKNOWN";
    }
}
