/* mongodb_native.c - MongoDB wire protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* MongoDB opcode name */
const char* xmongo_opcode(long long op) {
    switch (op) {
        case 1: return "OP_REPLY";
        case 2001: return "OP_UPDATE";
        case 2002: return "OP_INSERT";
        case 2004: return "OP_QUERY";
        case 2005: return "OP_GET_MORE";
        case 2006: return "OP_DELETE";
        case 2007: return "OP_KILL_CURSORS";
        case 2010: return "OP_COMPRESSED";
        case 2013: return "OP_MSG";
        default: return "UNKNOWN";
    }
}

/* BSON type name */
const char* xmongo_bson_type(long long type) {
    switch (type) {
        case 0x01: return "double";
        case 0x02: return "string";
        case 0x03: return "document";
        case 0x04: return "array";
        case 0x05: return "binary";
        case 0x06: return "undefined";
        case 0x07: return "objectId";
        case 0x08: return "boolean";
        case 0x09: return "datetime";
        case 0x0A: return "null";
        case 0x0B: return "regex";
        case 0x0D: return "javascript";
        case 0x10: return "int32";
        case 0x11: return "timestamp";
        case 0x12: return "int64";
        case 0x13: return "decimal128";
        case 0xFF: return "minkey";
        case 0x7F: return "maxkey";
        default: return "unknown";
    }
}

/* Parse mongodb:// connection URI - extract component */
const char* xmongo_uri_part(const char *uri, const char *part) {
    static char buf[512];
    const char *p = uri;
    if (strncmp(p, "mongodb://", 10) == 0) p += 10;
    else if (strncmp(p, "mongodb+srv://", 14) == 0) p += 14;
    else return "";
    if (strcmp(part, "user") == 0) {
        int i = 0;
        while (*p && *p != ':' && *p != '@' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    if (strcmp(part, "password") == 0) {
        while (*p && *p != ':') p++;
        if (*p != ':') return "";
        p++;
        int i = 0;
        while (*p && *p != '@' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    /* skip credentials */
    while (*p && *p != '@') p++;
    if (*p == '@') p++;
    if (strcmp(part, "host") == 0) {
        int i = 0;
        while (*p && *p != ':' && *p != '/' && *p != ',' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    if (strcmp(part, "port") == 0) {
        while (*p && *p != ':') p++;
        if (*p != ':') return "27017";
        p++;
        int i = 0;
        while (*p && *p != '/' && *p != ',' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    if (strcmp(part, "db") == 0) {
        while (*p && *p != '/') p++;
        if (*p != '/') return "";
        p++;
        int i = 0;
        while (*p && *p != '?' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    return "";
}

/* Write concern name */
const char* xmongo_w_name(long long w) {
    switch (w) {
        case 0: return "unacknowledged";
        case 1: return "acknowledged";
        case 2: return "majority_or_2";
        default: return "custom";
    }
}

/* Read preference name */
const char* xmongo_read_pref(long long pref) {
    switch (pref) {
        case 0: return "primary";
        case 1: return "primaryPreferred";
        case 2: return "secondary";
        case 3: return "secondaryPreferred";
        case 4: return "nearest";
        default: return "unknown";
    }
}

/* Generate ObjectId-like hex from timestamp + counter (simplified) */
const char* xmongo_objectid(long long timestamp, int counter) {
    snprintf(str_buf, sizeof(str_buf), "%08x%016x", (unsigned int)timestamp, (unsigned long long)counter & 0xFFFFFFFFFFFFFFFFULL);
    /* Trim to 24 hex chars */
    str_buf[24] = '\0';
    return str_buf;
}

/* Validate collection name */
long long xmongo_valid_collection(const char *name) {
    int len = (int)strlen(name);
    if (len == 0 || len > 120) return 0;
    if (name[0] == '.' || name[len-1] == '.') return 0;
    if (strstr(name, "..") != NULL) return 0;
    for (int i = 0; i < len; i++) {
        if (name[i] == '\0' || name[i] == '$') return 0;
    }
    if (strncmp(name, "system.", 7) == 0) return 0;
    return 1;
}

/* Cursor flag name */
const char* xmongo_cursor_flag(long long flag) {
    switch (flag) {
        case 0x02: return "tailable";
        case 0x04: return "slaveOk";
        case 0x10: return "noCursorTimeout";
        case 0x20: return "awaitData";
        case 0x40: return "exhaust";
        case 0x80: return "partial";
        default: return "unknown";
    }
}
