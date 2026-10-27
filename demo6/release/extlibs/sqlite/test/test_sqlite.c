#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_TEMP_MEMORY
#endif
#include "xrt.h"
#include "../include/sqlite_native.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "sqlite C line %d: %s\n", __LINE__, #c); return 1; } } while (0)

int main(void)
{
    CHECK(SQLITE_OK == 0);
    CHECK(SQLITE_ERROR == 1);
    CHECK(SQLITE_INTERNAL == 2);
    CHECK(SQLITE_PERM == 3);
    CHECK(SQLITE_ABORT == 4);
    CHECK(SQLITE_BUSY == 5);
    CHECK(SQLITE_LOCKED == 6);
    CHECK(SQLITE_NOMEM == 7);
    CHECK(SQLITE_READONLY == 8);
    CHECK(SQLITE_INTERRUPT == 9);
    CHECK(SQLITE_IOERR == 10);
    CHECK(SQLITE_CORRUPT == 11);
    CHECK(SQLITE_NOTFOUND == 12);
    CHECK(SQLITE_FULL == 13);
    CHECK(SQLITE_CANTOPEN == 14);
    CHECK(SQLITE_PROTOCOL == 15);
    CHECK(SQLITE_EMPTY == 16);
    CHECK(SQLITE_SCHEMA == 17);
    CHECK(SQLITE_TOOBIG == 18);
    CHECK(SQLITE_CONSTRAINT == 19);
    CHECK(SQLITE_MISMATCH == 20);
    CHECK(SQLITE_MISUSE == 21);
    CHECK(SQLITE_NOLFS == 22);
    CHECK(SQLITE_AUTH == 23);
    CHECK(SQLITE_FORMAT == 24);
    CHECK(SQLITE_RANGE == 25);
    CHECK(SQLITE_NOTADB == 26);
    CHECK(SQLITE_NOTICE == 27);
    CHECK(SQLITE_WARNING == 28);
    CHECK(SQLITE_ROW == 100);
    CHECK(SQLITE_DONE == 101);
    CHECK(SQLITE_INTEGER == 1);
    CHECK(SQLITE_FLOAT == 2);
    CHECK(SQLITE_TEXT == 3);
    CHECK(SQLITE_BLOB == 4);
    CHECK(SQLITE_NULL == 5);
    CHECK(SQLITE_OPEN_READONLY == 0x00000001);
    CHECK(SQLITE_OPEN_READWRITE == 0x00000002);
    CHECK(SQLITE_OPEN_CREATE == 0x00000004);
    CHECK(SQLITE_OPEN_URI == 0x00000040);
    CHECK(SQLITE_OPEN_MEMORY == 0x00000080);
    CHECK(SQLITE_OPEN_NOMUTEX == 0x00008000);
    CHECK(SQLITE_OPEN_FULLMUTEX == 0x00010000);
    CHECK(SQLITE_OPEN_SHAREDCACHE == 0x00020000);
    CHECK(SQLITE_OPEN_PRIVATECACHE == 0x00040000);
    CHECK(xsqlite_available_native() == 1);
    CHECK(strcmp(xsqlite_version_native(), sqlite3_libversion()) == 0);
    CHECK(strcmp(xsqlite3_libversion_native(), sqlite3_libversion()) == 0);
    CHECK(xsqlite3_libversion_number_native() == SQLITE_VERSION_NUMBER);
    CHECK(strstr(xsqlite3_errstr_native(SQLITE_BUSY), "locked") != NULL);

    /* 无句柄、参数范围和空操作必须是确定行为。 */
    CHECK(xsqlite3_exec_native(NULL, "select 1") == SQLITE_MISUSE);
    CHECK(xsqlite3_open_native(NULL) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xsqlite3_open_v2_native(NULL, SQLITE_OPEN_READWRITE, NULL) == NULL);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xsqlite3_step_native(NULL) == SQLITE_MISUSE);
    CHECK(xsqlite3_reset_native(NULL) == SQLITE_MISUSE);
    CHECK(xsqlite3_clear_bindings_native(NULL) == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_text_native(NULL, 1, "x") == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_int_native(NULL, 1, 0) == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_int64_native(NULL, 1, 0) == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_double_native(NULL, 1, 0) == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_null_native(NULL, 1) == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_blob_native(NULL, 1, NULL, 0) == SQLITE_MISUSE);
    CHECK(xsqlite3_bind_text_bytes_native(NULL, 1, NULL, 0) == SQLITE_MISUSE);
    CHECK(xsqlite3_busy_timeout_native(NULL, 0) == SQLITE_MISUSE);
    CHECK(xsqlite3_extended_errcode_native(NULL) == SQLITE_MISUSE);
    CHECK(xsqlite3_changes_native(NULL) == 0 && xsqlite3_total_changes_native(NULL) == 0);
    CHECK(xsqlite3_last_insert_rowid_native(NULL) == 0);
    CHECK(xsqlite3_bind_parameter_count_native(NULL) == 0);
    CHECK(xsqlite3_column_count_native(NULL) == 0);
    CHECK(strstr(xsqlite3_errmsg_native(NULL), "null") != NULL);
    CHECK(strstr(xsqlite3_stmt_errmsg_native(NULL), "null") != NULL);
    CHECK(xsqlite3_finalize_native(NULL) == SQLITE_OK);
    CHECK(xsqlite3_close_native(NULL) == SQLITE_OK && xsqlite3_close_v2_native(NULL) == SQLITE_OK);

    void* db = xsqlite3_open_native(":memory:");
    CHECK(db != NULL);
    CHECK(xsqlite3_exec_native(db, NULL) == SQLITE_MISUSE);
    CHECK(xsqlite3_busy_timeout_native(db, 0) == SQLITE_OK);
    CHECK(xsqlite3_busy_timeout_native(db, -1) == SQLITE_MISUSE);
    CHECK(xsqlite3_exec_native(db, "create table t(id integer primary key,i integer,r real,s text,b blob,n text)") == SQLITE_OK);
    void* insert = xsqlite3_prepare_v2_native(db, "insert into t values(?,?,?,?,?,?)");
    CHECK(insert != NULL && xsqlite3_bind_parameter_count_native(insert) == 6);
    CHECK(xsqlite3_bind_int_native(insert, 0, 1) == SQLITE_RANGE);
    CHECK(xsqlite3_bind_int_native(insert, 7, 1) == SQLITE_RANGE);
    CHECK(xsqlite3_bind_blob_native(insert, 5, "x", (size_t)INT_MAX + 1) == SQLITE_TOOBIG);
    CHECK(xsqlite3_bind_text_bytes_native(insert, 4, "x", (size_t)INT_MAX + 1) == SQLITE_TOOBIG);
    CHECK(xsqlite3_bind_blob_native(insert, 5, NULL, 1) == SQLITE_MISUSE);

    unsigned char blob[] = {0, 1, 255, 0, 42};
    CHECK(xsqlite3_bind_int_native(insert, 1, 7) == SQLITE_OK);
    CHECK(xsqlite3_bind_int64_native(insert, 2, INT64_MIN) == SQLITE_OK);
    CHECK(xsqlite3_bind_double_native(insert, 3, 12.5) == SQLITE_OK);
    CHECK(xsqlite3_bind_text_native(insert, 4, "hello UTF-8: \xe4\xbd\xa0\xe5\xa5\xbd") == SQLITE_OK);
    CHECK(xsqlite3_bind_blob_native(insert, 5, blob, sizeof(blob)) == SQLITE_OK);
    memset(blob, 0, sizeof(blob));
    CHECK(xsqlite3_bind_null_native(insert, 6) == SQLITE_OK);
    CHECK(xsqlite3_step_native(insert) == SQLITE_DONE);
    CHECK(xsqlite3_changes_native(db) == 1 && xsqlite3_total_changes_native(db) == 1);
    CHECK(xsqlite3_last_insert_rowid_native(db) == 7);
    CHECK(strcmp(xsqlite3_last_insert_rowid_text_native(db), "7") == 0);
    char* owned_rowid = xsqlite3_last_insert_rowid_text_owned_native(db);
    CHECK(owned_rowid != NULL && strcmp(owned_rowid, "7") == 0);
    xrtFree(owned_rowid);
    CHECK(xsqlite3_close_native(db) == SQLITE_BUSY);
    CHECK(xsqlite3_reset_native(insert) == SQLITE_OK);
    CHECK(xsqlite3_clear_bindings_native(insert) == SQLITE_OK);
    CHECK(xsqlite3_bind_int_native(insert, 1, 8) == SQLITE_OK);
    CHECK(xsqlite3_bind_int64_native(insert, 2, INT64_MAX) == SQLITE_OK);
    CHECK(xsqlite3_bind_text_bytes_native(insert, 4, NULL, 0) == SQLITE_OK);
    CHECK(xsqlite3_bind_blob_native(insert, 5, NULL, 0) == SQLITE_OK);
    CHECK(xsqlite3_step_native(insert) == SQLITE_DONE);
    CHECK(xsqlite3_finalize_native(insert) == SQLITE_OK);

    void* stmt = xsqlite3_prepare_v2_native(db, "select * from t order by id; -- trailing comment");
    CHECK(stmt != NULL && xsqlite3_column_count_native(stmt) == 6);
    CHECK(strcmp(xsqlite3_column_name_native(stmt, 3), "s") == 0);
    CHECK(xsqlite3_column_int_native(stmt, 0) == 0 && xrtErrorKind(xrtGetError()) == XERR_STATE);
    CHECK(xsqlite3_column_type_native(stmt, -1) == SQLITE_NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    CHECK(xsqlite3_column_type_native(stmt, 6) == SQLITE_NULL && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    CHECK(xsqlite3_step_native(stmt) == SQLITE_ROW);
    CHECK(xsqlite3_column_int_native(stmt, 0) == 7);
    CHECK(xsqlite3_column_int64_native(stmt, 1) == INT64_MIN);
    CHECK(xsqlite3_column_double_native(stmt, 2) == 12.5);
    const char* held = xsqlite3_column_text_native(stmt, 3);
    CHECK(strcmp(held, "hello UTF-8: \xe4\xbd\xa0\xe5\xa5\xbd") == 0);
    char* owned_text = xsqlite3_column_text_owned_native(stmt, 3);
    char* owned_name = xsqlite3_column_name_owned_native(stmt, 3);
    CHECK(owned_text != NULL && strcmp(owned_text, held) == 0);
    CHECK(owned_name != NULL && strcmp(owned_name, "s") == 0);
    for (int i = 0; i < 64; ++i) CHECK(strcmp(xsqlite3_column_name_native(stmt, i % 6), "") != 0);
    CHECK(strcmp(held, "hello UTF-8: \xe4\xbd\xa0\xe5\xa5\xbd") == 0);
    CHECK(xsqlite3_column_type_native(stmt, 4) == SQLITE_BLOB);
    CHECK(xsqlite3_column_type_native(stmt, 5) == SQLITE_NULL);
    CHECK(xsqlite3_column_bytes_native(stmt, 4) == 5);
    xbuffer* bytes = xsqlite3_column_blob_native(stmt, 4);
    unsigned char expected[] = {0, 1, 255, 0, 42};
    CHECK(bytes != NULL && bytes->Size == sizeof(expected));
    CHECK(memcmp(bytes->Data, expected, sizeof(expected)) == 0);
    CHECK(xsqlite3_step_native(stmt) == SQLITE_ROW);
    CHECK(xsqlite3_column_int64_native(stmt, 1) == INT64_MAX);
    CHECK(xsqlite3_column_type_native(stmt, 4) == SQLITE_BLOB);
    CHECK(xsqlite3_column_bytes_native(stmt, 4) == 0);
    CHECK(xsqlite3_column_type_native(stmt, 3) == SQLITE_TEXT);
    CHECK(xsqlite3_column_bytes_native(stmt, 3) == 0);
    CHECK(xsqlite3_step_native(stmt) == SQLITE_DONE);
    CHECK(xsqlite3_column_double_native(stmt, 2) == 0 && xrtErrorKind(xrtGetError()) == XERR_STATE);
    xrtClearError();
    CHECK(xsqlite3_finalize_native(stmt) == SQLITE_OK);
    CHECK(memcmp(bytes->Data, expected, sizeof(expected)) == 0);
    xrtBufferDestroy(bytes);
    CHECK(strcmp(owned_text, "hello UTF-8: \xe4\xbd\xa0\xe5\xa5\xbd") == 0);
    CHECK(strcmp(owned_name, "s") == 0);
    xrtFree(owned_text);
    xrtFree(owned_name);

    /* 含 NUL 的文本不得静默截断；显式字节接口完整往返。 */
    stmt = xsqlite3_prepare_v2_native(db, "select ?, ?");
    char nul_text[] = {'A', 0, 'B'};
    CHECK(xsqlite3_bind_text_bytes_native(stmt, 1, nul_text, sizeof(nul_text)) == SQLITE_OK);
    CHECK(xsqlite3_bind_blob_text_native(stmt, 2, "binary") == SQLITE_OK);
    CHECK(xsqlite3_step_native(stmt) == SQLITE_ROW);
    CHECK(strcmp(xsqlite3_column_text_native(stmt, 0), "") == 0);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(xsqlite3_column_text_owned_native(stmt, 0) == NULL);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_VALUE);
    CHECK(strstr(xsqlite_last_error_native(), "NUL") != NULL);
    CHECK(strstr(xsqlite3_last_error(), "NUL") != NULL);
    bytes = xsqlite3_column_text_bytes_native(stmt, 0);
    CHECK(bytes != NULL && bytes->Size == sizeof(nul_text) && memcmp(bytes->Data, nul_text, sizeof(nul_text)) == 0);
    xrtBufferDestroy(bytes);
    CHECK(strcmp(xsqlite3_column_text_native(stmt, 1), "binary") == 0);
    CHECK(xsqlite3_reset_native(stmt) == SQLITE_OK);
    CHECK(xsqlite3_finalize_native(stmt) == SQLITE_OK);

    /* 错误 SQL、空 SQL、多语句与半成品清理。 */
    CHECK(xsqlite3_prepare_v2_native(NULL, "select 1") == NULL);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xsqlite3_prepare_v2_native(db, NULL) == NULL);
    CHECK(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    CHECK(xsqlite3_prepare_v2_native(db, "") == NULL);
    CHECK(xsqlite3_prepare_v2_native(db, "select 1; select 2") == NULL);
    CHECK(xsqlite3_prepare_v2_native(db, "select 1; garbage") == NULL);
    CHECK(xsqlite3_prepare_v2_native(db, "select from") == NULL);
    CHECK(sqlite3_next_stmt(db, NULL) == NULL);
    xrtClearError();
    CHECK(xsqlite3_exec_native(db, "insert into t(id) values(7)") == SQLITE_CONSTRAINT);
    CHECK((xsqlite3_extended_errcode_native(db) & 0xff) == SQLITE_CONSTRAINT);
    CHECK(strstr(xsqlite3_errmsg_native(db), "UNIQUE") != NULL);
    char* owned_error = xsqlite3_errmsg_owned_native(db);
    CHECK(owned_error != NULL && strstr(owned_error, "UNIQUE") != NULL);
    stmt = xsqlite3_prepare_v2_native(db, "insert into t(id) values(7)");
    CHECK(stmt != NULL && xsqlite3_step_native(stmt) == SQLITE_CONSTRAINT);
    CHECK(strstr(xsqlite3_stmt_errmsg_native(stmt), "UNIQUE") != NULL);
    char* owned_stmt_error = xsqlite3_stmt_errmsg_owned_native(stmt);
    CHECK(owned_stmt_error != NULL && strstr(owned_stmt_error, "UNIQUE") != NULL);
    CHECK(xsqlite3_finalize_native(stmt) == SQLITE_CONSTRAINT);
    CHECK(xsqlite3_exec_native(db, "begin; insert into t(id) values(9); rollback") == SQLITE_OK);
    CHECK(xsqlite3_close_native(db) == SQLITE_OK);
    CHECK(strcmp(held, "hello UTF-8: \xe4\xbd\xa0\xe5\xa5\xbd") == 0);
    CHECK(strstr(owned_error, "UNIQUE") != NULL);
    CHECK(strstr(owned_stmt_error, "UNIQUE") != NULL);
    xrtFree(owned_error);
    xrtFree(owned_stmt_error);

    /* close_v2 后语句继续持有连接；最后 finalize 才释放。 */
    db = xsqlite3_open_v2_native(":memory:", SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, "");
    CHECK(db != NULL);
    stmt = xsqlite3_prepare_v2_native(db, "select 23");
    CHECK(stmt != NULL && xsqlite3_close_v2_native(db) == SQLITE_OK);
    CHECK(xsqlite3_step_native(stmt) == SQLITE_ROW && xsqlite3_column_int_native(stmt, 0) == 23);
    CHECK(xsqlite3_finalize_native(stmt) == SQLITE_OK);
    CHECK(xsqlite3_open_v2_native(":memory:", SQLITE_OPEN_READWRITE, "xlang-no-such-vfs") == NULL);
    CHECK(xrtGetError() != NULL);
    xrtClearError();
    CHECK(sqlite3_shutdown() == SQLITE_OK);
    CHECK(sqlite3_memory_used() == 0);
    xrtTempUnit(xrtTempCurrent());
    puts("sqlite C: ABI/constants, storage/binary, statement states, errors and lifetimes passed");
    return 0;
}
