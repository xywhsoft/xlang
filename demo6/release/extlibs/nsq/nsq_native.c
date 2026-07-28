/*
 * nsq_native.c - NSQ messaging protocol helpers
 * Tier 3: realtime distributed messaging platform protocol utilities
 */
#include <string.h>
#include <stdio.h>

static char buf[512];

/* NSQ command */
const char* xnsq_cmd_name(long long cmd) {
    switch (cmd) {
        case 0: return "IDENTIFY";
        case 1: return "SUB";
        case 2: return "PUB";
        case 3: return "MPUB";
        case 4: return "DPUB";
        case 5: return "RDY";
        case 6: return "FIN";
        case 7: return "REQ";
        case 8: return "CLS";
        case 9: return "NOP";
        case 10: return "TOUCH";
        case 11: return "AUTH";
        default: return "UNKNOWN";
    }
}

/* Frame type */
const char* xnsq_frame_type(long long type) {
    switch (type) {
        case 0: return "RESPONSE";
        case 1: return "ERROR";
        case 2: return "MESSAGE";
        default: return "UNKNOWN";
    }
}

/* Error type */
const char* xnsq_error_name(long long code) {
    switch (code) {
        case 0: return "E_INVALID";
        case 1: return "E_BAD_TOPIC";
        case 2: return "E_BAD_CHANNEL";
        case 3: return "E_BAD_MESSAGE";
        case 4: return "E_PUB_FAILED";
        case 5: return "E_MPUB_FAILED";
        case 6: return "E_DPUB_FAILED";
        case 7: return "E_FIN_FAILED";
        case 8: return "E_REQ_FAILED";
        case 9: return "E_TOUCH_FAILED";
        case 10: return "E_AUTH_FIRST";
        case 11: return "E_AUTH_FAILED";
        case 12: return "E_AUTH_DISABLED";
        default: return "E_UNKNOWN";
    }
}

/* Topic name validation */
long long xnsq_valid_topic(const char *name) {
    int len = (int)strlen(name);
    if (len < 1 || len > 64) return 0;
    for (int i = 0; i < len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') continue;
        if (c >= 'A' && c <= 'Z') continue;
        if (c >= '0' && c <= '9') continue;
        if (c == '.' || c == '-' || c == '_' || c == '#') continue;
        return 0;
    }
    return 1;
}

/* Channel name validation (same rules as topic) */
long long xnsq_valid_channel(const char *name) {
    return xnsq_valid_topic(name);
}

/* Message ID validation: 16 hex chars */
long long xnsq_valid_msg_id(const char *id) {
    int len = (int)strlen(id);
    if (len != 16) return 0;
    for (int i = 0; i < 16; i++) {
        char c = id[i];
        if (c >= '0' && c <= '9') continue;
        if (c >= 'a' && c <= 'f') continue;
        if (c >= 'A' && c <= 'F') continue;
        return 0;
    }
    return 1;
}

/* Magic identifier */
const char* xnsq_magic() {
    return "  V2";
}

/* Feature negotiation: check if feature is supported */
long long xnsq_feature_supported(const char *feature) {
    if (strcmp(feature, "client_id") == 0) return 1;
    if (strcmp(feature, "hostname") == 0) return 1;
    if (strcmp(feature, "heartbeat_interval") == 0) return 1;
    if (strcmp(feature, "output_buffer_size") == 0) return 1;
    if (strcmp(feature, "output_buffer_timeout") == 0) return 1;
    if (strcmp(feature, "feature_negotiation") == 0) return 1;
    if (strcmp(feature, "sample_rate") == 0) return 1;
    if (strcmp(feature, "deflate") == 0) return 1;
    if (strcmp(feature, "snappy") == 0) return 1;
    if (strcmp(feature, "tls_v1") == 0) return 1;
    if (strcmp(feature, "msg_timeout") == 0) return 1;
    return 0;
}
