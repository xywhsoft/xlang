#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#include "xrt.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "../../release/extlibs/mysql/include/mysql_native.h"
double xfaiss_l2_distance(const char* a, const char* b);
double xfaiss_inner_product(const char* a, const char* b);
double xfaiss_cosine(const char* a, const char* b);

static void mysql_metadata(void)
{
    const char* full = "IN_TRANS AUTOCOMMIT MORE_RESULTS NO_GOOD_INDEX NO_INDEX CURSOR_EXISTS LAST_ROW_SENT DB_DROPPED NO_BACKSLASH_ESCAPES METADATA_CHANGED QUERY_WAS_SLOW PS_OUT_PARAMS IN_TRANS_READONLY SESSION_STATE_CHANGED";
    /* 独立按官方协议编号列出期望，不从实现表读取答案。 */
    const unsigned masks[] = {1, 2, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    const char* names[] = {"IN_TRANS", "AUTOCOMMIT", "MORE_RESULTS", "NO_GOOD_INDEX", "NO_INDEX", "CURSOR_EXISTS", "LAST_ROW_SENT", "DB_DROPPED", "NO_BACKSLASH_ESCAPES", "METADATA_CHANGED", "QUERY_WAS_SLOW", "PS_OUT_PARAMS", "IN_TRANS_READONLY", "SESSION_STATE_CHANGED"};
    const char* saved = xmysql_status_str(0x7ffb);
    unsigned bits, i;
    assert(saved && strcmp(saved, full) == 0);
    for (bits = 0; bits < 16384; ++bits) {
        unsigned mask = 0;
        char expected[256] = {0};
        const char* text;
        for (i = 0; i < 14; ++i) if (bits & (1u << i)) {
            mask |= masks[i];
            size_t used = strlen(expected);
            int written = snprintf(expected + used, sizeof(expected) - used, "%s%s", used ? " " : "", names[i]);
            assert(written >= 0 && (size_t)written < sizeof(expected) - used);
        }
        text = xmysql_status_str(mask);
        assert(text && strcmp(text, expected) == 0);
        assert(strcmp(saved, full) == 0);
    }
    assert(strcmp(xmysql_status_str(0), "") == 0);
    assert(strcmp(xmysql_status_str(-1), full) == 0);
    assert(strcmp(xmysql_status_str(0x8004), "") == 0);
    assert(strcmp(xmysql_cmd_name(9), "COM_STATISTICS") == 0);
    assert(strcmp(xmysql_cmd_name(10), "COM_PROCESS_INFO") == 0);
    assert(strcmp(xmysql_cmd_name(12), "COM_PROCESS_KILL") == 0);
    assert(strcmp(xmysql_cmd_name(17), "COM_CHANGE_USER") == 0);
    assert(strcmp(xmysql_cmd_name(27), "COM_SET_OPTION") == 0);
    assert(strcmp(xmysql_cmd_name(31), "COM_RESET_CONNECTION") == 0);
    assert(strcmp(xmysql_cmd_name(-1), "UNKNOWN") == 0);
    assert(strcmp(xmysql_cmd_name(0x100000000LL), "UNKNOWN") == 0);
    assert(strcmp(xmysql_cap_name(0x100000), "CONNECT_ATTRS") == 0);
    assert(strcmp(xmysql_cap_name(0x200000), "PLUGIN_AUTH_LENENC") == 0);
    assert(strcmp(xmysql_error_class(1045), "server") == 0);
    assert(strcmp(xmysql_error_class(2003), "client") == 0);
    assert(strcmp(xmysql_error_class(3000), "server") == 0);
    assert(strcmp(xmysql_error_class(1), "global") == 0);
    assert(strcmp(xmysql_error_class(0), "unknown") == 0);
}

static void mysql_text(void)
{
    char large[10001];
    const char *escaped, *dsn, *user, *host, *port, *db;
    char *ownedEscape, *ownedDsn, *ownedDb;
    size_t i;
    memset(large, '\'', 10000); large[10000] = 0;
    escaped = xmysql_escape(large);
    assert(escaped && strlen(escaped) == 20000);
    for (i = 0; i < 10000; ++i) assert(escaped[i * 2] == '\\' && escaped[i * 2 + 1] == '\'');
    memset(large, 'x', 10000);
    dsn = xmysql_format_dsn("::1", 65535, large, "user");
    assert(dsn && strlen(dsn) == 10025);
    user = xmysql_dsn_part(dsn, "user");
    host = xmysql_dsn_part(dsn, "host");
    port = xmysql_dsn_part(dsn, "port");
    db = xmysql_dsn_part(dsn, "db");
    assert(user && host && port && db);
    assert(strcmp(user, "user") == 0 && strcmp(host, "::1") == 0 && strcmp(port, "65535") == 0);
    assert(strcmp(db, large) == 0 && strlen(escaped) == 20000);
    ownedEscape = xmysql_escape_owned(large);
    ownedDsn = xmysql_format_dsn_owned("::1", 65535, large, "user");
    ownedDb = xmysql_dsn_part_owned(dsn, "db");
    assert(ownedEscape && ownedDsn && ownedDb);
    assert(strlen(ownedEscape) == 10000 && strcmp(ownedDsn, dsn) == 0);
    assert(strcmp(ownedDb, large) == 0);
    xrtFree(ownedDb);
    xrtFree(ownedDsn);
    xrtFree(ownedEscape);
    ownedEscape = xmysql_escape_owned("it's");
    assert(ownedEscape && strcmp(ownedEscape, "it\\'s") == 0);
    xrtFree(ownedEscape);
    assert(strcmp(xmysql_escape("'\"\\\n\r\x1a"), "\\'\\\"\\\\\\n\\r\\Z") == 0);
    assert(xmysql_escape(NULL) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(xmysql_escape_owned(NULL) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(strcmp(xmysql_escape(""), "") == 0 && xrtGetError() == NULL);
    assert(xmysql_format_dsn(NULL, 3306, "db", "u") == NULL);
    assert(xmysql_format_dsn("h", 0, "db", "u") == NULL);
    assert(xmysql_format_dsn("h", 65536, "db", "u") == NULL);
    assert(xmysql_format_dsn("h", 3306, "db", "u@v") == NULL);
    assert(xmysql_format_dsn("h", 3306, "db/part", "u") == NULL);
    assert(xmysql_dsn_part(NULL, "db") == NULL);
    assert(xmysql_dsn_part("mysql://u@h:3306/db", NULL) == NULL);
    {
        const char* invalid[] = {"u@h:3306/db", "mysql://h/db", "mysql://u@:3306/db", "mysql://u@h:x/db", "mysql://u@h:0/db", "mysql://u@h:99999999/db", "mysql://u@[::1:3306/db", "mysql://u@h:3306/db?x=1"};
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
            assert(xmysql_dsn_part(invalid[i], "host") == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    }
    assert(xmysql_dsn_part("mysql://u@h:3306/db", "password") == NULL);
    assert(strcmp(xmysql_dsn_part("mysql://@h:1/", "db"), "") == 0);
    assert(xrtGetError() == NULL);
}

int main(void)
{
    unsigned i;
    mysql_metadata();
    mysql_text();
    assert(xfaiss_l2_distance("1.5,2.5", "4.5,6.5") == 5);
    assert(xfaiss_l2_distance("-1e0, -2", "2,2") == 5);
    assert(xfaiss_inner_product(" -1.5,2e1 ", "2,3") == 57);
    assert(fabs(xfaiss_cosine("1.5,-2.5", "-3,5") + 1) < 1e-14);
    assert(xfaiss_cosine("0,0", "2,3") == 0);
    for (i = 0; i < 3; ++i) {
        double (*metric)(const char*, const char*) = i == 0 ? xfaiss_l2_distance :
            i == 1 ? xfaiss_inner_product : xfaiss_cosine;
        const char* bad[] = {"x", "1,", "1,,2", "1 2", "nan", "inf", "1e999", " "};
        size_t j;
        for (j = 0; j < sizeof(bad) / sizeof(bad[0]); ++j) {
            assert(metric(bad[j], "1") == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
            assert(metric("1", bad[j]) == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        }
        assert(metric(NULL, "1") == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(metric("1,2", "1") == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
        assert(metric("", "") == 0 && xrtGetError() == NULL);
    }
    assert(xfaiss_inner_product("1e308", "1e308") == 0 && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    puts("partial helpers: 16384 protocol status combinations, text lifetimes and numeric boundaries passed");
    return 0;
}
