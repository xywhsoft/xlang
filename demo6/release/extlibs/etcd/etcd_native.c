/* etcd_native.c - etcd v3 API helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* etcd API method name */
const char* xetcd_method_name(long long method) {
    switch (method) {
        case 0: return "Range";
        case 1: return "Put";
        case 2: return "DeleteRange";
        case 3: return "Txn";
        case 4: return "Compact";
        case 5: return "LeaseGrant";
        case 6: return "LeaseRevoke";
        case 7: return "LeaseKeepAlive";
        case 8: return "Watch";
        case 9: return "MemberList";
        case 10: return "MemberAdd";
        case 11: return "MemberRemove";
        case 12: return "Status";
        case 13: return "Defragment";
        case 14: return "Snapshot";
        default: return "Unknown";
    }
}

/* Sort order name */
const char* xetcd_sort_order(long long order) {
    switch (order) {
        case 0: return "NONE";
        case 1: return "ASCEND";
        case 2: return "DESCEND";
        default: return "UNKNOWN";
    }
}

/* Sort target name */
const char* xetcd_sort_target(long long target) {
    switch (target) {
        case 0: return "KEY";
        case 1: return "VERSION";
        case 2: return "CREATE";
        case 3: return "MOD";
        case 4: return "VALUE";
        default: return "UNKNOWN";
    }
}

/* Watch event type */
const char* xetcd_event_type(long long type) {
    switch (type) {
        case 0: return "PUT";
        case 1: return "DELETE";
        default: return "UNKNOWN";
    }
}

/* Compare result name */
const char* xetcd_compare_result(long long result) {
    switch (result) {
        case 0: return "EQUAL";
        case 1: return "GREATER";
        case 2: return "LESS";
        case 3: return "NOT_EQUAL";
        default: return "UNKNOWN";
    }
}

/* Compare target name */
const char* xetcd_compare_target(long long target) {
    switch (target) {
        case 0: return "VERSION";
        case 1: return "CREATE";
        case 2: return "MOD";
        case 3: return "VALUE";
        case 4: return "LEASE";
        default: return "UNKNOWN";
    }
}

/* Encode key as base64-like hex for etcd API */
const char* xetcd_encode_key(const char *key) {
    int pos = 0;
    for (const char *p = key; *p; p++) {
        pos += sprintf(str_buf + pos, "%02x", (unsigned char)*p);
    }
    return str_buf;
}

/* Decode hex key back to string */
const char* xetcd_decode_key(const char *hex) {
    int pos = 0;
    const char *p = hex;
    while (*p) {
        unsigned int v;
        if (sscanf(p, "%02x", &v) != 1) break;
        str_buf[pos++] = (char)v;
        p += 2;
    }
    str_buf[pos] = '\0';
    return str_buf;
}

/* Validate etcd key */
long long xetcd_valid_key(const char *key) {
    int len = (int)strlen(key);
    if (len == 0) return 0;
    if (len > 2048) return 0;
    /* Key cannot start with \0 */
    if (key[0] == '\0') return 0;
    return 1;
}

/* Format etcd endpoint */
const char* xetcd_endpoint(const char *host, int port, int tls) {
    snprintf(str_buf, sizeof(str_buf), "%s://%s:%d", tls ? "https" : "http", host, port);
    return str_buf;
}

/* Alarm type name */
const char* xetcd_alarm_name(long long alarm) {
    switch (alarm) {
        case 0: return "NONE";
        case 1: return "NOSPACE";
        case 2: return "CORRUPT";
        default: return "UNKNOWN";
    }
}
