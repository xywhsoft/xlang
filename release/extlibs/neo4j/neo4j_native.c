/* neo4j_native.c - Neo4j Bolt protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Bolt message type name */
const char* xneo4j_msg_name(long long sig) {
    switch (sig) {
        case 0x01: return "INIT";
        case 0x0E: return "ACK_FAILURE";
        case 0x0F: return "RESET";
        case 0x10: return "RUN";
        case 0x3F: return "DISCARD_ALL";
        case 0x2F: return "PULL_ALL";
        case 0x70: return "SUCCESS";
        case 0x7E: return "IGNORED";
        case 0x7F: return "FAILURE";
        case 0x71: return "RECORD";
        case 0x0A: return "HELLO";
        case 0x02: return "GOODBYE";
        case 0x39: return "BEGIN";
        case 0x12: return "COMMIT";
        case 0x13: return "ROLLBACK";
        default: return "UNKNOWN";
    }
}

/* Neo4j type system name */
const char* xneo4j_type_name(long long code) {
    switch (code) {
        case 0: return "Node";
        case 1: return "Relationship";
        case 2: return "Path";
        case 3: return "Integer";
        case 4: return "Float";
        case 5: return "String";
        case 6: return "Boolean";
        case 7: return "Null";
        case 8: return "List";
        case 9: return "Map";
        case 10: return "DateTime";
        case 11: return "Point";
        case 12: return "Duration";
        case 13: return "Bytes";
        default: return "Unknown";
    }
}

/* Escape Cypher string */
const char* xneo4j_escape(const char *input) {
    int pos = 0;
    for (const char *p = input; *p && pos < 4080; p++) {
        if (*p == '\'' || *p == '\\') { str_buf[pos++] = '\\'; }
        str_buf[pos++] = *p;
    }
    str_buf[pos] = '\0';
    return str_buf;
}

/* Parse bolt:// URI component */
const char* xneo4j_uri_part(const char *uri, const char *part) {
    static char buf[512];
    const char *p = uri;
    if (strncmp(p, "bolt://", 7) == 0) p += 7;
    else if (strncmp(p, "neo4j://", 8) == 0) p += 8;
    else if (strncmp(p, "bolt+s://", 9) == 0) p += 9;
    else if (strncmp(p, "neo4j+s://", 10) == 0) p += 10;
    else return "";
    /* skip user:pass@ if present */
    const char *at = strchr(p, '@');
    if (at) {
        if (strcmp(part, "user") == 0) {
            int i = 0;
            while (p < at && *p != ':' && i < 510) buf[i++] = *p++;
            buf[i] = '\0'; return buf;
        }
        p = at + 1;
    } else if (strcmp(part, "user") == 0) return "";
    if (strcmp(part, "host") == 0) {
        int i = 0;
        while (*p && *p != ':' && *p != '/' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    if (strcmp(part, "port") == 0) {
        while (*p && *p != ':') p++;
        if (*p != ':') return "7687";
        p++;
        int i = 0;
        while (*p && *p != '/' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    return "";
}

/* Transaction state name */
const char* xneo4j_tx_state(long long state) {
    switch (state) {
        case 0: return "disconnected";
        case 1: return "ready";
        case 2: return "streaming";
        case 3: return "tx_ready";
        case 4: return "tx_streaming";
        case 5: return "failed";
        default: return "unknown";
    }
}

/* Access mode name */
const char* xneo4j_access_mode(long long mode) {
    switch (mode) {
        case 0: return "write";
        case 1: return "read";
        default: return "unknown";
    }
}

/* Validate label name */
long long xneo4j_valid_label(const char *label) {
    int len = (int)strlen(label);
    if (len == 0) return 0;
    char c = label[0];
    if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') && c != '_') return 0;
    for (int i = 1; i < len; i++) {
        c = label[i];
        if (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z') &&
            !(c >= '0' && c <= '9') && c != '_') return 0;
    }
    return 1;
}
