/* mysql_native.c - MySQL wire protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* MySQL command name */
const char* xmysql_cmd_name(long long cmd) {
    switch (cmd) {
        case 0x01: return "COM_QUIT";
        case 0x02: return "COM_INIT_DB";
        case 0x03: return "COM_QUERY";
        case 0x05: return "COM_CREATE_DB";
        case 0x06: return "COM_DROP_DB";
        case 0x07: return "COM_REFRESH";
        case 0x08: return "COM_STATISTICS";
        case 0x09: return "COM_PROCESS_INFO";
        case 0x0B: return "COM_PROCESS_KILL";
        case 0x0E: return "COM_PING";
        case 0x10: return "COM_CHANGE_USER";
        case 0x11: return "COM_RESET_CONNECTION";
        case 0x16: return "COM_STMT_PREPARE";
        case 0x17: return "COM_STMT_EXECUTE";
        case 0x19: return "COM_STMT_CLOSE";
        case 0x1F: return "COM_SET_OPTION";
        default: return "UNKNOWN";
    }
}

/* MySQL column type name */
const char* xmysql_col_type(long long type) {
    switch (type) {
        case 0x00: return "DECIMAL";
        case 0x01: return "TINY";
        case 0x02: return "SHORT";
        case 0x03: return "LONG";
        case 0x04: return "FLOAT";
        case 0x05: return "DOUBLE";
        case 0x06: return "NULL";
        case 0x07: return "TIMESTAMP";
        case 0x08: return "LONGLONG";
        case 0x09: return "INT24";
        case 0x0A: return "DATE";
        case 0x0B: return "TIME";
        case 0x0C: return "DATETIME";
        case 0x0D: return "YEAR";
        case 0x0F: return "VARCHAR";
        case 0x10: return "BIT";
        case 0xF5: return "JSON";
        case 0xF6: return "NEWDECIMAL";
        case 0xF7: return "ENUM";
        case 0xF8: return "SET";
        case 0xF9: return "TINY_BLOB";
        case 0xFA: return "MEDIUM_BLOB";
        case 0xFB: return "LONG_BLOB";
        case 0xFC: return "BLOB";
        case 0xFD: return "VAR_STRING";
        case 0xFE: return "STRING";
        case 0xFF: return "GEOMETRY";
        default: return "UNKNOWN";
    }
}

/* MySQL server status flag decode */
const char* xmysql_status_str(long long status) {
    static char sbuf[64];
    int pos = 0;
    if (status & 0x0001) pos += sprintf(sbuf + pos, "IN_TRANS ");
    if (status & 0x0002) pos += sprintf(sbuf + pos, "AUTOCOMMIT ");
    if (status & 0x0008) pos += sprintf(sbuf + pos, "MORE_RESULTS ");
    if (status & 0x0010) pos += sprintf(sbuf + pos, "NO_GOOD_INDEX ");
    if (status & 0x0020) pos += sprintf(sbuf + pos, "NO_INDEX ");
    if (status & 0x0040) pos += sprintf(sbuf + pos, "CURSOR_EXISTS ");
    if (status & 0x0100) pos += sprintf(sbuf + pos, "SESSION_STATE_CHANGED ");
    if (pos > 0) sbuf[pos - 1] = '\0'; else sbuf[0] = '\0';
    return sbuf;
}

/* MySQL capability flag name */
const char* xmysql_cap_name(long long cap) {
    switch (cap) {
        case 0x0001: return "LONG_PASSWORD";
        case 0x0002: return "FOUND_ROWS";
        case 0x0004: return "LONG_FLAG";
        case 0x0008: return "CONNECT_WITH_DB";
        case 0x0200: return "PROTOCOL_41";
        case 0x8000: return "SECURE_CONNECTION";
        case 0x00080000: return "PLUGIN_AUTH";
        case 0x00100000: return "CONNECT_ATTRS";
        case 0x00200000: return "PLUGIN_AUTH_LENENC";
        default: return "UNKNOWN";
    }
}

/* Escape MySQL string */
const char* xmysql_escape(const char *input) {
    int pos = 0;
    for (const char *p = input; *p && pos < 4080; p++) {
        switch (*p) {
            case '\'': str_buf[pos++] = '\\'; str_buf[pos++] = '\''; break;
            case '"': str_buf[pos++] = '\\'; str_buf[pos++] = '"'; break;
            case '\\': str_buf[pos++] = '\\'; str_buf[pos++] = '\\'; break;
            case '\0': str_buf[pos++] = '\\'; str_buf[pos++] = '0'; break;
            case '\n': str_buf[pos++] = '\\'; str_buf[pos++] = 'n'; break;
            case '\r': str_buf[pos++] = '\\'; str_buf[pos++] = 'r'; break;
            case '\x1a': str_buf[pos++] = '\\'; str_buf[pos++] = 'Z'; break;
            default: str_buf[pos++] = *p; break;
        }
    }
    str_buf[pos] = '\0';
    return str_buf;
}

/* Format MySQL DSN */
const char* xmysql_format_dsn(const char *host, int port, const char *dbname, const char *user) {
    snprintf(str_buf, sizeof(str_buf), "mysql://%s@%s:%d/%s", user, host, port, dbname);
    return str_buf;
}

/* Parse MySQL DSN: mysql://user@host:port/db - extract component */
const char* xmysql_dsn_part(const char *dsn, const char *part) {
    /* Simple parser for mysql://user@host:port/db */
    const char *p = dsn;
    if (strncmp(p, "mysql://", 8) == 0) p += 8;
    static char buf[512];
    if (strcmp(part, "user") == 0) {
        int i = 0;
        while (*p && *p != '@' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    /* skip user@ */
    while (*p && *p != '@') p++;
    if (*p == '@') p++;
    if (strcmp(part, "host") == 0) {
        int i = 0;
        while (*p && *p != ':' && *p != '/' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    if (strcmp(part, "port") == 0) {
        while (*p && *p != ':') p++;
        if (*p == ':') p++;
        int i = 0;
        while (*p && *p != '/' && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    if (strcmp(part, "db") == 0) {
        while (*p && *p != '/') p++;
        if (*p == '/') p++;
        int i = 0;
        while (*p && i < 510) buf[i++] = *p++;
        buf[i] = '\0'; return buf;
    }
    return "";
}

/* MySQL error code category */
const char* xmysql_error_class(long long code) {
    if (code >= 1000 && code < 2000) return "client";
    if (code >= 2000 && code < 3000) return "server";
    if (code >= 3000 && code < 4000) return "extended";
    if (code >= 4000 && code < 5000) return "plugin";
    return "unknown";
}
