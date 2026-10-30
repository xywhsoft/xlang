#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include "../include/sqlite_native.h"
#ifdef __TINYC__
/* SQLite amalgamation 必须先于独立 sqlite3.h 进入本翻译单元。 */
#include "../third_party/sqlite3.c"
#include "../sqlite_native.c"
#else
#include "../third_party/sqlite3.h"
#endif
#include <stdio.h>

int main(void)
{
    void* db = xsqlite3_open_native(":memory:");
    void* stmt = NULL;
    xbuffer* bytes = NULL;
    const char* failed = "open";
    int rc = SQLITE_OK;
    int count = 0;
    const char* names[] = {"Ada", "O'Reilly"};
    if (!db) goto cleanup;

    /* prepare 只接受一条语句；拒绝的尾部 PRAGMA 不得让后续建表变成只读。 */
    failed = "reject extra statement";
    if (xsqlite3_exec_native(db, "pragma query_only = off") != SQLITE_OK) goto cleanup;
    stmt = xsqlite3_prepare_v2_native(db, "select 1; pragma query_only = on");
    if (stmt != NULL || xrtErrorKind(xrtGetError()) != XERR_ARGUMENT) goto cleanup;
    xrtClearError();

    failed = "create table";
    if (xsqlite3_exec_native(db, "create table people(id integer primary key, name text)") != SQLITE_OK) goto cleanup;
    failed = "prepare insert";
    stmt = xsqlite3_prepare_v2_native(db, "insert into people(name) values(?)");
    if (!stmt) goto cleanup;
    for (int i = 0; i < 2; ++i) {
        failed = "insert";
        if (xsqlite3_bind_text_native(stmt, 1, names[i]) != SQLITE_OK ||
            xsqlite3_step_native(stmt) != SQLITE_DONE ||
            xsqlite3_reset_native(stmt) != SQLITE_OK) goto cleanup;
    }
    failed = "finalize insert";
    rc = xsqlite3_finalize_native(stmt);
    stmt = NULL;
    if (rc != SQLITE_OK) goto cleanup;

    puts("sqlite demo");
    failed = "select people";
    stmt = xsqlite3_prepare_v2_native(db, "select id, name from people order by id");
    if (!stmt) goto cleanup;
    while ((rc = xsqlite3_step_native(stmt)) == SQLITE_ROW) {
        const char* name = xsqlite3_column_text_native(stmt, 1);
        if (!name) goto cleanup;
        printf("%lld: %s\n", (long long)xsqlite3_column_int64_native(stmt, 0), name);
    }
    if (rc != SQLITE_DONE) goto cleanup;
    rc = xsqlite3_finalize_native(stmt);
    stmt = NULL;
    if (rc != SQLITE_OK) goto cleanup;
    failed = "rollback";
    if (xsqlite3_exec_native(db, "begin; insert into people(name) values('discarded'); rollback") != SQLITE_OK) goto cleanup;
    failed = "count people";
    stmt = xsqlite3_prepare_v2_native(db, "select id from people");
    if (!stmt) goto cleanup;
    while ((rc = xsqlite3_step_native(stmt)) == SQLITE_ROW) ++count;
    if (rc != SQLITE_DONE) goto cleanup;
    rc = xsqlite3_finalize_native(stmt);
    stmt = NULL;
    if (rc != SQLITE_OK) goto cleanup;
    printf("rollback: %d\n", count);

    /* 列数据复制到由 XRT 管理的 Buffer；语句和数据库关闭后仍然有效。 */
    failed = "select blob";
    stmt = xsqlite3_prepare_v2_native(db, "select x'00ff2a'");
    if (!stmt || xsqlite3_step_native(stmt) != SQLITE_ROW) goto cleanup;
    bytes = xsqlite3_column_blob_native(stmt, 0);
    if (!bytes) goto cleanup;
    rc = xsqlite3_finalize_native(stmt);
    stmt = NULL;
    if (rc != SQLITE_OK) goto cleanup;
    failed = "close";
    rc = xsqlite3_close_native(db);
    if (rc != SQLITE_OK) goto cleanup;
    db = NULL;
    printf("blob: %zu bytes\n", bytes->Size);
    failed = NULL;

cleanup:
    if (stmt) xsqlite3_finalize_native(stmt);
    if (db) xsqlite3_close_native(db);
    xrtBufferDestroy(bytes);
    xrtTempUnit(xrtTempCurrent());
    sqlite3_shutdown();
    if (failed) fprintf(stderr, "sqlite example failed: %s\n", failed);
    return failed ? 1 : 0;
}
