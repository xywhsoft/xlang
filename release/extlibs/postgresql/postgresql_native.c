/* postgresql_native.c - PostgreSQL wire protocol helpers */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static char str_buf[4096];

/* Parse connection string key=value pairs, return value for key */
const char* xpg_conn_param(const char *conninfo, const char *key) {
    const char *p = conninfo;
    int klen = (int)strlen(key);
    while (*p) {
        while (*p == ' ') p++;
        if (strncmp(p, key, klen) == 0 && p[klen] == '=') {
            p += klen + 1;
            int pos = 0;
            if (*p == '\'') { p++; while (*p && *p != '\'' && pos < 4090) str_buf[pos++] = *p++; }
            else { while (*p && *p != ' ' && pos < 4090) str_buf[pos++] = *p++; }
            str_buf[pos] = '\0';
            return str_buf;
        }
        while (*p && *p != ' ') p++;
    }
    return "";
}

/* Build startup message (protocol 3.0) as hex */
const char* xpg_startup_msg(const char *user, const char *database) {
    int body_len = 4 + 4 + 5 + (int)strlen(user) + 1 + 9 + (int)strlen(database) + 1 + 1;
    int total = 4 + body_len;
    int pos = 0;
    pos += sprintf(str_buf + pos, "%02x%02x%02x%02x", (total>>24)&0xFF, (total>>16)&0xFF, (total>>8)&0xFF, total&0xFF);
    pos += sprintf(str_buf + pos, "00030000"); /* protocol 3.0 */
    pos += sprintf(str_buf + pos, "75736572"); /* "user" */
    pos += sprintf(str_buf + pos, "00");
    for (const char *p = user; *p; p++) pos += sprintf(str_buf + pos, "%02x", (unsigned char)*p);
    pos += sprintf(str_buf + pos, "00");
    pos += sprintf(str_buf + pos, "6461746162617365"); /* "database" */
    pos += sprintf(str_buf + pos, "00");
    for (const char *p = database; *p; p++) pos += sprintf(str_buf + pos, "%02x", (unsigned char)*p);
    pos += sprintf(str_buf + pos, "0000"); /* terminator */
    return str_buf;
}

/* Message type name from first byte */
const char* xpg_msg_type(long long type_byte) {
    switch (type_byte) {
        case 'R': return "Authentication";
        case 'S': return "ParameterStatus";
        case 'K': return "BackendKeyData";
        case 'Z': return "ReadyForQuery";
        case 'T': return "RowDescription";
        case 'D': return "DataRow";
        case 'C': return "CommandComplete";
        case 'E': return "ErrorResponse";
        case 'N': return "NoticeResponse";
        case 'Q': return "Query";
        case 'P': return "Parse";
        case 'B': return "Bind";
        case '1': return "ParseComplete";
        case '2': return "BindComplete";
        case '3': return "CloseComplete";
        case 'c': return "CopyDone";
        case 'd': return "CopyData";
        case 'H': return "Flush";
        case 'X': return "Terminate";
        default: return "Unknown";
    }
}

/* Transaction status from ReadyForQuery byte */
const char* xpg_tx_status(long long status) {
    switch (status) {
        case 'I': return "idle";
        case 'T': return "in_transaction";
        case 'E': return "failed";
        default: return "unknown";
    }
}

/* SQL type name from OID */
const char* xpg_type_name(long long oid) {
    switch (oid) {
        case 16: return "bool";
        case 20: return "int8";
        case 21: return "int2";
        case 23: return "int4";
        case 25: return "text";
        case 700: return "float4";
        case 701: return "float8";
        case 1043: return "varchar";
        case 1082: return "date";
        case 1114: return "timestamp";
        case 1184: return "timestamptz";
        case 2950: return "uuid";
        case 3802: return "jsonb";
        default: return "unknown";
    }
}

/* Escape SQL string (doubling single quotes) */
const char* xpg_escape(const char *input) {
    int pos = 0;
    for (const char *p = input; *p && pos < 4090; p++) {
        if (*p == '\'') { str_buf[pos++] = '\''; str_buf[pos++] = '\''; }
        else str_buf[pos++] = *p;
    }
    str_buf[pos] = '\0';
    return str_buf;
}

/* Build simple query message as hex */
const char* xpg_query_msg(const char *sql) {
    int len = 4 + (int)strlen(sql) + 1;
    int pos = 0;
    pos += sprintf(str_buf + pos, "51"); /* 'Q' */
    pos += sprintf(str_buf + pos, "%02x%02x%02x%02x", (len>>24)&0xFF, (len>>16)&0xFF, (len>>8)&0xFF, len&0xFF);
    for (const char *p = sql; *p; p++) pos += sprintf(str_buf + pos, "%02x", (unsigned char)*p);
    pos += sprintf(str_buf + pos, "00");
    return str_buf;
}

/* Parse error severity from hex (simplified) */
const char* xpg_error_field_name(long long code) {
    switch (code) {
        case 'S': return "severity";
        case 'V': return "severity_nonlocalized";
        case 'C': return "code";
        case 'M': return "message";
        case 'D': return "detail";
        case 'H': return "hint";
        case 'P': return "position";
        case 'F': return "file";
        case 'L': return "line";
        case 'R': return "routine";
        default: return "unknown";
    }
}

/* Format DSN from components */
const char* xpg_format_dsn(const char *host, int port, const char *dbname, const char *user) {
    snprintf(str_buf, sizeof(str_buf), "postgresql://%s@%s:%d/%s", user, host, port, dbname);
    return str_buf;
}
