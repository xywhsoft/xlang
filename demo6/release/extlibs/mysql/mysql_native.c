/* mysql_native.c - MySQL wire protocol helpers */
#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "include/mysql_native.h"

/* 经典协议命令编号；名称查询不代表服务器仍接受已废弃命令。 */
const char* xmysql_cmd_name(long long cmd) {
    static const char* const names[] = {
        "COM_SLEEP", "COM_QUIT", "COM_INIT_DB", "COM_QUERY", "COM_FIELD_LIST",
        "COM_CREATE_DB", "COM_DROP_DB", "COM_REFRESH", "COM_SHUTDOWN",
        "COM_STATISTICS", "COM_PROCESS_INFO", "COM_CONNECT", "COM_PROCESS_KILL",
        "COM_DEBUG", "COM_PING", "COM_TIME", "COM_DELAYED_INSERT", "COM_CHANGE_USER",
        "COM_BINLOG_DUMP", "COM_TABLE_DUMP", "COM_CONNECT_OUT", "COM_REGISTER_SLAVE",
        "COM_STMT_PREPARE", "COM_STMT_EXECUTE", "COM_STMT_SEND_LONG_DATA",
        "COM_STMT_CLOSE", "COM_STMT_RESET", "COM_SET_OPTION", "COM_STMT_FETCH",
        "COM_DAEMON", "COM_BINLOG_DUMP_GTID", "COM_RESET_CONNECTION"
    };
    xrtClearError();
    return cmd >= 0 && cmd < (long long)(sizeof(names) / sizeof(names[0])) ? names[cmd] : "UNKNOWN";
}

/* MySQL column type name */
const char* xmysql_col_type(long long type) {
    xrtClearError();
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
    static const struct { unsigned mask; const char* name; } flags[] = {
        {0x0001, "IN_TRANS"}, {0x0002, "AUTOCOMMIT"}, {0x0008, "MORE_RESULTS"},
        {0x0010, "NO_GOOD_INDEX"}, {0x0020, "NO_INDEX"}, {0x0040, "CURSOR_EXISTS"},
        {0x0080, "LAST_ROW_SENT"}, {0x0100, "DB_DROPPED"},
        {0x0200, "NO_BACKSLASH_ESCAPES"}, {0x0400, "METADATA_CHANGED"},
        {0x0800, "QUERY_WAS_SLOW"}, {0x1000, "PS_OUT_PARAMS"},
        {0x2000, "IN_TRANS_READONLY"}, {0x4000, "SESSION_STATE_CHANGED"}
    };
    size_t i, size = 1, pos = 0;
    char* text;
    xrtClearError();
    /* 从同一张表计算上界，后续增加标志也不会越界；返回区交给 XRT 管理。 */
    for (i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
        if (status & flags[i].mask) size += strlen(flags[i].name) + 1;
    text = xrtTemp(size);
    if (text == NULL) return NULL;
    for (i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i) {
        if (status & flags[i].mask) {
            size_t length = strlen(flags[i].name);
            if (pos != 0) text[pos++] = ' ';
            memcpy(text + pos, flags[i].name, length);
            pos += length;
        }
    }
    text[pos] = '\0';
    return text;
}

/* MySQL capability flag name */
const char* xmysql_cap_name(long long cap) {
    xrtClearError();
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

// ----------------------------- 临时文本与参数边界 -----------------------------

static const char* mysql_argument(const char* message) {
    xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.mysql", 1, message);
    return NULL;
}

/* 仅实现反斜杠字符串字面量转义，不代替数据库驱动的参数绑定。 */
const char* xmysql_escape(const char* input) {
    const char* p;
    char* text;
    size_t length, pos = 0;
    xrtClearError();
    if (input == NULL) return mysql_argument("mysql escape input is required");
    length = strlen(input);
    if (length > (SIZE_MAX - 1) / 2) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.mysql", 2, "mysql escaped text is too large");
        return NULL;
    }
    text = xrtTemp(length * 2 + 1);
    if (text == NULL) return NULL;
    for (p = input; *p; ++p) {
        char escaped = 0;
        switch (*p) {
        case '\'': case '"': case '\\': escaped = *p; break;
        case '\n': escaped = 'n'; break;
        case '\r': escaped = 'r'; break;
        case '\x1a': escaped = 'Z'; break;
        default: break;
        }
        if (escaped) { text[pos++] = '\\'; text[pos++] = escaped; }
        else text[pos++] = *p;
    }
    text[pos] = 0;
    return text;
}

/* DSN 辅助格式不承载密码/查询参数，拒绝会改变结构的分隔符。 */
static bool mysql_component(const char* text, const char* forbidden) {
    const unsigned char* p = (const unsigned char*)text;
    if (p == NULL) return false;
    for (; *p; ++p) if (*p <= 32 || *p == 127 || strchr(forbidden, *p)) return false;
    return true;
}

const char* xmysql_format_dsn(const char* host, int port, const char* dbname, const char* user) {
    int size;
    char* text;
    bool ipv6;
    xrtClearError();
    if (!mysql_component(host, "/@?#[]") || !host[0] ||
        !mysql_component(dbname, "/@?#") || !mysql_component(user, "/@:#?") ||
        port < 1 || port > 65535)
        return mysql_argument("invalid mysql DSN component or port");
    ipv6 = strchr(host, ':') != NULL;
    size = snprintf(NULL, 0, "mysql://%s@%s%s%s:%d/%s",
        user, ipv6 ? "[" : "", host, ipv6 ? "]" : "", port, dbname);
    if (size < 0) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.mysql", 2, "mysql DSN is too large");
        return NULL;
    }
    text = xrtTemp((size_t)size + 1);
    if (text != NULL) snprintf(text, (size_t)size + 1, "mysql://%s@%s%s%s:%d/%s",
        user, ipv6 ? "[" : "", host, ipv6 ? "]" : "", port, dbname);
    return text;
}

const char* xmysql_dsn_part(const char* dsn, const char* part) {
    const char *user, *at, *host, *endHost, *port, *slash, *begin, *end, *p;
    unsigned number = 0;
    xrtClearError();
    if (dsn == NULL || part == NULL || strncmp(dsn, "mysql://", 8) != 0)
        return mysql_argument("mysql DSN must start with mysql://");
    user = dsn + 8;
    at = strchr(user, '@');
    if (at == NULL) return mysql_argument("mysql DSN user separator is missing");
    for (p = user; p < at; ++p)
        if ((unsigned char)*p <= 32 || *p == 127 || strchr("/:#?", *p)) return mysql_argument("invalid mysql DSN user");
    host = at + 1;
    if (*host == '[') {
        ++host;
        endHost = strchr(host, ']');
        if (endHost == NULL || endHost[1] != ':') return mysql_argument("invalid mysql DSN host");
        port = endHost + 2;
    } else {
        endHost = strchr(host, ':');
        if (endHost == NULL) return mysql_argument("mysql DSN port is missing");
        port = endHost + 1;
    }
    if (host == endHost) return mysql_argument("mysql DSN host is empty");
    for (p = host; p < endHost; ++p)
        if ((unsigned char)*p <= 32 || *p == 127 || strchr("/@?#[]", *p)) return mysql_argument("invalid mysql DSN host");
    slash = strchr(port, '/');
    if (slash == NULL || slash == port || !mysql_component(slash + 1, "/@?#"))
        return mysql_argument("invalid mysql DSN database or port");
    for (p = port; p < slash; ++p) {
        if (*p < '0' || *p > '9' || number > 6553)
            return mysql_argument("invalid mysql DSN port");
        number = number * 10 + (unsigned)(*p - '0');
    }
    if (number < 1 || number > 65535) return mysql_argument("invalid mysql DSN port");
    if (strcmp(part, "user") == 0) { begin = user; end = at; }
    else if (strcmp(part, "host") == 0) { begin = host; end = endHost; }
    else if (strcmp(part, "port") == 0) { begin = port; end = slash; }
    else if (strcmp(part, "db") == 0) { begin = slash + 1; end = begin + strlen(begin); }
    else return mysql_argument("unknown mysql DSN component");
    return xrtTempStr(xrtTempCurrent(), (xstrview){begin, (size_t)(end - begin)});
}

/* 按经典公开范围分类，不声称范围内每一个编号都已定义。 */
const char* xmysql_error_class(long long code) {
    xrtClearError();
    if (code > 0 && code < 1000) return "global";
    if (code >= 2000 && code < 3000) return "client";
    if ((code >= 1000 && code < 2000) || (code >= 3000 && code < 5000)) return "server";
    return "unknown";
}
