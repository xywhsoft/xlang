#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#include "xrt.h"
#include "third_party/sqlite3.h"
#include "include/sqlite_native.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define XSQLITE_EXPORT __declspec(dllexport)
#else
#define XSQLITE_EXPORT
#endif

/* 状态码入口保留 SQLite 合同；无状态码的失败通过当前 XRT 错误上下文报告。 */
static void xsqlite_error(sqlite3* db, int rc)
{
    xerrkind kind = XERR_STATE;
    switch (rc & 0xff) {
    case SQLITE_NOMEM: kind = XERR_MEMORY; break;
    case SQLITE_MISUSE: case SQLITE_RANGE: kind = XERR_ARGUMENT; break;
    case SQLITE_TOOBIG: kind = XERR_RANGE; break;
    case SQLITE_CANTOPEN: case SQLITE_READONLY: case SQLITE_IOERR: kind = XERR_IO; break;
    default: break;
    }
    xrtSetErrorInfo(kind, "xlang.extlib.sqlite", rc,
        db != NULL ? sqlite3_errmsg(db) : sqlite3_errstr(rc));
}

/* 不使用环形返回槽；调用方借用到当前 XRT 临时区 clear/rewind 为止。 */
static const char* xsqlite_text(const char* text)
{
    if (text == NULL) text = "";
    char* copy = xrtTempStr(xrtTempCurrent(), (xstrview){text, strlen(text)});
    return copy != NULL ? copy : "";
}

/* 语言侧拥有型出口只复制一次，且不要求额外的 XRT 字符串模块。 */
static char* xsqlite_owned_text(const char* text)
{
    return (char*)xrtMemDup(text, strlen(text) + 1u);
}

static int xsqlite_column(sqlite3_stmt* stmt, int column, int require_row)
{
    xrtClearError();
    if (stmt == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sqlite", SQLITE_MISUSE,
            "null sqlite3_stmt handle");
        return 0;
    }
    if (column < 0 || column >= sqlite3_column_count(stmt)) {
        xrtSetErrorInfo(XERR_RANGE, "xlang.extlib.sqlite", SQLITE_RANGE,
            "sqlite column index out of range");
        return 0;
    }
    if (require_row && sqlite3_data_count(stmt) == 0) {
        xrtSetErrorInfo(XERR_STATE, "xlang.extlib.sqlite", SQLITE_MISUSE,
            "sqlite statement has no current row");
        return 0;
    }
    return 1;
}


/* 打开失败释放半成品句柄；诊断在释放前复制到 XRT，不创建私有 TLS 错误系统。 */
XSQLITE_EXPORT void* xsqlite3_open_v2_native(const char* filename, int flags, const char* vfs)
{
    sqlite3* db = NULL;
    xrtClearError();
    if (filename == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sqlite", SQLITE_MISUSE,
            "null sqlite filename");
        return NULL;
    }
    int rc = sqlite3_open_v2(filename, &db, flags,
        vfs != NULL && vfs[0] != '\0' ? vfs : NULL);
    if (rc != SQLITE_OK) {
        xsqlite_error(db, rc);
        sqlite3_close(db);
        return NULL;
    }
    return db;
}

XSQLITE_EXPORT void* xsqlite3_open_native(const char* filename)
{
    return xsqlite3_open_v2_native(filename, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
}

XSQLITE_EXPORT int xsqlite3_exec_native(void* db, const char* sql)
{
    if (db == NULL || sql == NULL) return SQLITE_MISUSE;
    return sqlite3_exec((sqlite3*)db, sql, NULL, NULL, NULL);
}

/* prepare 只接收一条语句；多条执行必须显式使用 exec，不能静默丢弃尾部 SQL。 */
XSQLITE_EXPORT void* xsqlite3_prepare_v2_native(void* handle, const char* sql)
{
    sqlite3* db = (sqlite3*)handle;
    sqlite3_stmt* stmt = NULL;
    sqlite3_stmt* extra = NULL;
    const char* tail = NULL;
    xrtClearError();
    if (db == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sqlite", SQLITE_MISUSE, "null sqlite3 handle");
        return NULL;
    }
    if (sql == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sqlite", SQLITE_MISUSE,
            "null sqlite SQL statement");
        return NULL;
    }
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, &tail);
    if (rc != SQLITE_OK) {
        xsqlite_error(db, rc);
        return NULL;
    }
    if (stmt == NULL) {
        xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sqlite", SQLITE_MISUSE, "sqlite prepare requires one statement");
        return NULL;
    }
    if (tail != NULL && *tail != '\0') {
        rc = sqlite3_prepare_v2(db, tail, -1, &extra, NULL);
        if (rc != SQLITE_OK || extra != NULL) {
            sqlite3_finalize(extra);
            sqlite3_finalize(stmt);
            if ((rc & 0xff) == SQLITE_NOMEM) xsqlite_error(NULL, rc);
            else xrtSetErrorInfo(XERR_ARGUMENT, "xlang.extlib.sqlite", SQLITE_MISUSE,
                    "sqlite prepare accepts exactly one statement");
            return NULL;
        }
    }
    return stmt;
}

/* 绑定使用 SQLITE_TRANSIENT；空 blob 与 SQL NULL 明确区分。 */
XSQLITE_EXPORT int xsqlite3_bind_text_native(void* stmt, int index, const char* value)
{
    return stmt != NULL ? sqlite3_bind_text((sqlite3_stmt*)stmt, index, value, -1, SQLITE_TRANSIENT) : SQLITE_MISUSE;
}

XSQLITE_EXPORT int xsqlite3_bind_blob_native(void* stmt, int index, void* data, size_t size)
{
    if (stmt == NULL || (data == NULL && size != 0)) return SQLITE_MISUSE;
    if (size > INT_MAX) return SQLITE_TOOBIG;
    if (size == 0) return sqlite3_bind_zeroblob((sqlite3_stmt*)stmt, index, 0);
    return sqlite3_bind_blob((sqlite3_stmt*)stmt, index, data, (int)size, SQLITE_TRANSIENT);
}

XSQLITE_EXPORT int xsqlite3_bind_text_bytes_native(void* stmt, int index, void* data, size_t size)
{
    if (stmt == NULL || (data == NULL && size != 0)) return SQLITE_MISUSE;
    if (size > INT_MAX) return SQLITE_TOOBIG;
    return sqlite3_bind_text((sqlite3_stmt*)stmt, index, data != NULL ? (const char*)data : "",
        (int)size, SQLITE_TRANSIENT);
}

XSQLITE_EXPORT int xsqlite3_bind_blob_text_native(void* stmt, int index, const char* value)
{
    return xsqlite3_bind_blob_native(stmt, index, (void*)value, value != NULL ? strlen(value) : 0);
}

XSQLITE_EXPORT int xsqlite3_bind_int_native(void* stmt, int index, int value)
{
    return stmt != NULL ? sqlite3_bind_int((sqlite3_stmt*)stmt, index, value) : SQLITE_MISUSE;
}

XSQLITE_EXPORT int xsqlite3_bind_int64_native(void* stmt, int index, int64_t value)
{
    return stmt != NULL ? sqlite3_bind_int64((sqlite3_stmt*)stmt, index, (sqlite3_int64)value) : SQLITE_MISUSE;
}

XSQLITE_EXPORT int xsqlite3_bind_double_native(void* stmt, int index, double value)
{
    return stmt != NULL ? sqlite3_bind_double((sqlite3_stmt*)stmt, index, value) : SQLITE_MISUSE;
}

XSQLITE_EXPORT int xsqlite3_bind_null_native(void* stmt, int index)
{
    return stmt != NULL ? sqlite3_bind_null((sqlite3_stmt*)stmt, index) : SQLITE_MISUSE;
}

/* 当前行与列范围在调用 SQLite 前检查；SQLite 对这些误用不保证定义良好的行为。 */
XSQLITE_EXPORT int xsqlite3_column_type_native(void* stmt, int column)
{
    return xsqlite_column(stmt, column, 1) ? sqlite3_column_type(stmt, column) : SQLITE_NULL;
}

XSQLITE_EXPORT int xsqlite3_column_int_native(void* stmt, int column)
{
    return xsqlite_column(stmt, column, 1) ? sqlite3_column_int(stmt, column) : 0;
}

XSQLITE_EXPORT int64_t xsqlite3_column_int64_native(void* stmt, int column)
{
    return xsqlite_column(stmt, column, 1) ? (int64_t)sqlite3_column_int64(stmt, column) : 0;
}

XSQLITE_EXPORT double xsqlite3_column_double_native(void* stmt, int column)
{
    return xsqlite_column(stmt, column, 1) ? sqlite3_column_double(stmt, column) : 0.0;
}

XSQLITE_EXPORT int xsqlite3_column_bytes_native(void* stmt, int column)
{
    if (!xsqlite_column(stmt, column, 1)) return 0;
    int size = sqlite3_column_bytes(stmt, column);
    if (sqlite3_errcode(sqlite3_db_handle(stmt)) == SQLITE_NOMEM) xsqlite_error(NULL, SQLITE_NOMEM);
    return size;
}

static const char* xsqlite_column_text_view(void* stmt, int column)
{
    if (!xsqlite_column(stmt, column, 1)) return "";
    const char* text = (const char*)sqlite3_column_text(stmt, column);
    int size = sqlite3_column_bytes(stmt, column);
    if (sqlite3_errcode(sqlite3_db_handle(stmt)) == SQLITE_NOMEM) {
        xsqlite_error(NULL, SQLITE_NOMEM);
        return "";
    }
    if (text != NULL && memchr(text, 0, (size_t)size) != NULL) {
        xrtSetErrorInfo(XERR_VALUE, "xlang.extlib.sqlite", SQLITE_MISMATCH,
            "sqlite text contains NUL; read column_text_bytes instead");
        return "";
    }
    return text != NULL ? text : "";
}

XSQLITE_EXPORT const char* xsqlite3_column_text_native(void* stmt, int column)
{
    return xsqlite_text(xsqlite_column_text_view(stmt, column));
}

/* 语言侧直接接管这一次复制，原 C 借用入口保持不变。 */
XSQLITE_EXPORT char* xsqlite3_column_text_owned_native(void* stmt, int column)
{
    const char* text = xsqlite_column_text_view(stmt, column);
    return xrtGetError() == NULL ? xsqlite_owned_text(text) : NULL;
}

static void* xsqlite_column_buffer(void* stmt, int column, int text)
{
    if (!xsqlite_column(stmt, column, 1)) return NULL;
    const void* data = text ? (const void*)sqlite3_column_text(stmt, column) : sqlite3_column_blob(stmt, column);
    int size = sqlite3_column_bytes(stmt, column);
    if (sqlite3_errcode(sqlite3_db_handle(stmt)) == SQLITE_NOMEM) {
        xsqlite_error(NULL, SQLITE_NOMEM);
        return NULL;
    }
    return xrtBufferFrom((xbytesview){data, (size_t)size});
}

XSQLITE_EXPORT void* xsqlite3_column_blob_native(void* stmt, int column)
{
    return xsqlite_column_buffer(stmt, column, 0);
}

XSQLITE_EXPORT void* xsqlite3_column_text_bytes_native(void* stmt, int column)
{
    return xsqlite_column_buffer(stmt, column, 1);
}

static const char* xsqlite_column_name_view(void* stmt, int column)
{
    if (!xsqlite_column(stmt, column, 0)) return "";
    const char* name = sqlite3_column_name(stmt, column);
    if (name == NULL) { xsqlite_error(NULL, SQLITE_NOMEM); return ""; }
    return name;
}

XSQLITE_EXPORT const char* xsqlite3_column_name_native(void* stmt, int column)
{
    return xsqlite_text(xsqlite_column_name_view(stmt, column));
}

XSQLITE_EXPORT char* xsqlite3_column_name_owned_native(void* stmt, int column)
{
    const char* name = xsqlite_column_name_view(stmt, column);
    return xrtGetError() == NULL ? xsqlite_owned_text(name) : NULL;
}

/* 元数据和资源管理：返回码入口不隐式抛出 SQL 执行错误；调用方保留完整状态码。 */
XSQLITE_EXPORT int xsqlite_available_native(void) { return 1; }
XSQLITE_EXPORT const char* xsqlite3_libversion_native(void) { return sqlite3_libversion(); }
XSQLITE_EXPORT const char* xsqlite_version_native(void) { return sqlite3_libversion(); }
XSQLITE_EXPORT int xsqlite3_libversion_number_native(void) { return sqlite3_libversion_number(); }
XSQLITE_EXPORT const char* xsqlite3_last_error(void) { return xrtErrorMessage(xrtGetError()); }
XSQLITE_EXPORT const char* xsqlite_last_error_native(void) { return xsqlite3_last_error(); }
XSQLITE_EXPORT const char* xsqlite3_errstr_native(int rc) { return sqlite3_errstr(rc); }

XSQLITE_EXPORT const char* xsqlite3_errmsg_native(void* db)
{
    return db != NULL ? xsqlite_text(sqlite3_errmsg(db)) : "null sqlite3 handle";
}

XSQLITE_EXPORT char* xsqlite3_errmsg_owned_native(void* db)
{
    return xsqlite_owned_text(db != NULL ? sqlite3_errmsg(db) : "null sqlite3 handle");
}

XSQLITE_EXPORT const char* xsqlite3_stmt_errmsg_native(void* stmt)
{
    return stmt != NULL ? xsqlite3_errmsg_native(sqlite3_db_handle(stmt)) : "null sqlite3_stmt handle";
}

XSQLITE_EXPORT char* xsqlite3_stmt_errmsg_owned_native(void* stmt)
{
    return xsqlite_owned_text(stmt != NULL ? sqlite3_errmsg(sqlite3_db_handle(stmt)) :
        "null sqlite3_stmt handle");
}

XSQLITE_EXPORT int xsqlite3_close_native(void* db) { return sqlite3_close(db); }
XSQLITE_EXPORT int xsqlite3_close_v2_native(void* db) { return sqlite3_close_v2(db); }
XSQLITE_EXPORT int xsqlite3_extended_errcode_native(void* db)
{
    return db != NULL ? sqlite3_extended_errcode(db) : SQLITE_MISUSE;
}
XSQLITE_EXPORT int64_t xsqlite3_changes_native(void* db) { return db != NULL ? (int64_t)sqlite3_changes64(db) : 0; }
XSQLITE_EXPORT int64_t xsqlite3_total_changes_native(void* db) { return db != NULL ? (int64_t)sqlite3_total_changes64(db) : 0; }
XSQLITE_EXPORT int64_t xsqlite3_last_insert_rowid_native(void* db)
{
    return db != NULL ? (int64_t)sqlite3_last_insert_rowid(db) : 0;
}
XSQLITE_EXPORT const char* xsqlite3_last_insert_rowid_text_native(void* db)
{
    if (db == NULL) { xsqlite_error(NULL, SQLITE_MISUSE); return ""; }
    char text[32];
    snprintf(text, sizeof(text), "%lld", (long long)sqlite3_last_insert_rowid(db));
    return xsqlite_text(text);
}

XSQLITE_EXPORT char* xsqlite3_last_insert_rowid_text_owned_native(void* db)
{
    if (db == NULL) { xsqlite_error(NULL, SQLITE_MISUSE); return NULL; }
    char text[32];
    snprintf(text, sizeof(text), "%lld", (long long)sqlite3_last_insert_rowid(db));
    return xsqlite_owned_text(text);
}

XSQLITE_EXPORT int xsqlite3_busy_timeout_native(void* db, int milliseconds)
{
    return db != NULL && milliseconds >= 0 ? sqlite3_busy_timeout(db, milliseconds) : SQLITE_MISUSE;
}
XSQLITE_EXPORT int xsqlite3_step_native(void* stmt) { return stmt != NULL ? sqlite3_step(stmt) : SQLITE_MISUSE; }
XSQLITE_EXPORT int xsqlite3_finalize_native(void* stmt) { return sqlite3_finalize(stmt); }
XSQLITE_EXPORT int xsqlite3_reset_native(void* stmt) { return stmt != NULL ? sqlite3_reset(stmt) : SQLITE_MISUSE; }
XSQLITE_EXPORT int xsqlite3_clear_bindings_native(void* stmt)
{
    return stmt != NULL ? sqlite3_clear_bindings(stmt) : SQLITE_MISUSE;
}
XSQLITE_EXPORT int xsqlite3_bind_parameter_count_native(void* stmt)
{
    return stmt != NULL ? sqlite3_bind_parameter_count(stmt) : 0;
}
XSQLITE_EXPORT int xsqlite3_column_count_native(void* stmt) { return sqlite3_column_count(stmt); }
