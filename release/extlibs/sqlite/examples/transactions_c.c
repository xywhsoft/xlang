#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include "../include/sqlite_native.h"
#ifdef __TINYC__
#include "../third_party/sqlite3.c"
#include "../sqlite_native.c"
#else
#include "../third_party/sqlite3.h"
#endif
#include <stdio.h>
#include <string.h>

int main(void)
{
    void* db = xsqlite3_open_native(":memory:");
    void *insert = NULL, *query = NULL;
    char* saved = NULL;
    int result = 1, rc;
    if (!db || xsqlite3_exec_native(db, "create table items(id integer primary key, stock integer not null)") != SQLITE_OK) goto done;
    insert = xsqlite3_prepare_v2_native(db, "insert into items values(?, ?)");
    if (!insert || xsqlite3_exec_native(db, "begin") != SQLITE_OK ||
        xsqlite3_bind_int64_native(insert, 1, 1) != SQLITE_OK ||
        xsqlite3_bind_int64_native(insert, 2, 10) != SQLITE_OK ||
        xsqlite3_step_native(insert) != SQLITE_DONE || xsqlite3_reset_native(insert) != SQLITE_OK ||
        xsqlite3_bind_int64_native(insert, 1, 1) != SQLITE_OK ||
        xsqlite3_bind_int64_native(insert, 2, 20) != SQLITE_OK ||
        xsqlite3_step_native(insert) != SQLITE_CONSTRAINT ||
        xsqlite3_reset_native(insert) != SQLITE_CONSTRAINT ||
        xsqlite3_clear_bindings_native(insert) != SQLITE_OK) goto done;
    puts("constraint: rejected");
    if (xsqlite3_exec_native(db, "rollback") != SQLITE_OK) goto done;
    query = xsqlite3_prepare_v2_native(db, "select count(*) from items");
    if (!query || xsqlite3_step_native(query) != SQLITE_ROW || xsqlite3_column_int64_native(query, 0) != 0) goto done;
    rc = xsqlite3_finalize_native(query);
    query = NULL;
    if (rc != SQLITE_OK) goto done;
    puts("rollback: rows=0");
    if (xsqlite3_bind_int64_native(insert, 1, 2) != SQLITE_OK ||
        xsqlite3_bind_int64_native(insert, 2, 7) != SQLITE_OK || xsqlite3_step_native(insert) != SQLITE_DONE ||
        xsqlite3_close_native(db) != SQLITE_BUSY) goto done;
    puts("busy-close: retained");
    rc = xsqlite3_finalize_native(insert);
    insert = NULL;
    if (rc != SQLITE_OK) goto done;
    query = xsqlite3_prepare_v2_native(db, "select count(*), sum(stock) from items");
    if (!query) goto done;
    // C借用出口仍返回空串，首错必须是无当前行，不伪造成功或OOM。
    const char* before = xsqlite3_column_text_native(query, 0);
    if (!before || *before || xrtErrorKind(xrtGetError()) != XERR_STATE ||
        xrtErrorCode(xrtGetError()) != SQLITE_MISUSE) goto done;
    xrtClearError();
    puts("before-row: rejected");
    if (xsqlite3_step_native(query) != SQLITE_ROW || xsqlite3_column_int64_native(query, 0) != 1 ||
        xsqlite3_column_int64_native(query, 1) != 7) goto done;
    saved = xsqlite3_column_text_owned_native(query, 1);
    if (!saved || strcmp(saved, "7")) goto done;
    rc = xsqlite3_finalize_native(query);
    query = NULL;
    if (rc != SQLITE_OK || xsqlite3_close_native(db) != SQLITE_OK) goto done;
    db = NULL;
    puts("commit: rows=1 total=7");
    printf("snapshot: %s\n", saved);
    result = 0;
done:
    (void)xsqlite3_finalize_native(query);
    (void)xsqlite3_finalize_native(insert);
    (void)xsqlite3_close_v2_native(db);
    xrtFree(saved);
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    (void)sqlite3_shutdown();
    if (result) fputs("SQLite transaction example failed\n", stderr);
    return result;
}
