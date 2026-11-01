#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
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

#define CHECK(expression) do { if (!(expression)) { \
    fprintf(stderr, "SQLite WAL example line %d: %s\n", __LINE__, #expression); goto done; \
} } while (0)

static int scalar(void* database, const char* sql, int64_t* output)
{
    void* statement = xsqlite3_prepare_v2_native(database, sql);
    int ok = statement && xsqlite3_step_native(statement) == SQLITE_ROW;
    if (ok) { *output = xsqlite3_column_int64_native(statement, 0); ok = xrtGetError() == NULL; }
    int finalized = xsqlite3_finalize_native(statement);
    return ok && finalized == SQLITE_OK;
}

int main(void)
{
    char* directory = xrtDirTemp(NULL, "xlang-sqlite-wal-", "");
    char* filename = directory ? xrtPathJoin(directory, "快照.db") : NULL;
    void *writer = NULL, *reader = NULL, *insert = NULL, *mode = NULL;
    int result = 1, rc;
    int64_t before = 0, during = 0, after = 0, count = 0, sum = 0;
    CHECK(directory && filename);
    writer = xsqlite3_open_native(filename);
    CHECK(writer);
    mode = xsqlite3_prepare_v2_native(writer, "pragma journal_mode=wal");
    CHECK(mode && xsqlite3_step_native(mode) == SQLITE_ROW);
    CHECK(strcmp(xsqlite3_column_text_native(mode, 0), "wal") == 0 && !xrtGetError());
    rc = xsqlite3_finalize_native(mode);
    mode = NULL;
    CHECK(rc == SQLITE_OK);
    CHECK(xsqlite3_exec_native(writer, "create table items(value integer not null)") == SQLITE_OK);
    CHECK(xsqlite3_exec_native(writer, "insert into items values(1)") == SQLITE_OK);
    reader = xsqlite3_open_native(filename);
    CHECK(reader && xsqlite3_busy_timeout_native(reader, 0) == SQLITE_OK);
    insert = xsqlite3_prepare_v2_native(reader, "insert into items values(?)");
    CHECK(insert && xsqlite3_bind_int64_native(insert, 1, 2) == SQLITE_OK);

    /* 先持有真实写锁，再验证 BUSY/reset 后绑定与连接可恢复。 */
    CHECK(xsqlite3_exec_native(writer, "begin immediate") == SQLITE_OK);
    CHECK(xsqlite3_step_native(insert) == SQLITE_BUSY);
    CHECK(xsqlite3_reset_native(insert) == SQLITE_BUSY);
    CHECK(xsqlite3_exec_native(writer, "rollback") == SQLITE_OK);
    CHECK(xsqlite3_step_native(insert) == SQLITE_DONE);
    CHECK(xsqlite3_reset_native(insert) == SQLITE_OK);
    CHECK(scalar(reader, "select count(*) from items", &count) && count == 2);
    puts("journal: wal");
    puts("write retry: rows=2");

    CHECK(xsqlite3_exec_native(reader, "begin") == SQLITE_OK);
    CHECK(scalar(reader, "select count(*) from items", &before));
    CHECK(xsqlite3_exec_native(writer, "insert into items values(3)") == SQLITE_OK);
    CHECK(scalar(reader, "select count(*) from items", &during));
    CHECK(scalar(writer, "select count(*) from items", &count));
    CHECK(before == 2 && during == 2 && count == 3);

    /* C 另外确认拒绝源于旧快照，而不是另一个连接还持有写锁。 */
    CHECK(xsqlite3_step_native(insert) == SQLITE_BUSY);
    CHECK(xsqlite3_extended_errcode_native(reader) == SQLITE_BUSY_SNAPSHOT);
    CHECK(xsqlite3_reset_native(insert) == SQLITE_BUSY);
    CHECK(xsqlite3_exec_native(reader, "rollback") == SQLITE_OK);
    CHECK(scalar(reader, "select count(*) from items", &after) && after == 3);
    printf("snapshot: %lld/%lld/%lld\n", (long long)before, (long long)during, (long long)after);
    CHECK(xsqlite3_step_native(insert) == SQLITE_DONE);
    rc = xsqlite3_finalize_native(insert);
    insert = NULL;
    CHECK(rc == SQLITE_OK);
    CHECK(scalar(writer, "select count(*) from items", &count) && count == 4);
    CHECK(scalar(writer, "select sum(value) from items", &sum) && sum == 8);
    puts("stale write: recovered");
    puts("final: rows=4 sum=8");
    CHECK(xsqlite3_close_native(reader) == SQLITE_OK);
    reader = NULL;
    CHECK(xsqlite3_close_native(writer) == SQLITE_OK);
    writer = NULL;
    result = 0;
done:
    (void)xsqlite3_finalize_native(mode);
    (void)xsqlite3_finalize_native(insert);
    if (reader) (void)xsqlite3_close_v2_native(reader);
    if (writer) (void)xsqlite3_close_v2_native(writer);
    /* 只清理由本例排他创建的文件和空目录；绝不接收用户目录再递归删除。 */
    if (filename && !xrtFileDelete(filename)) result = 1;
    if (directory && !xrtDirRemove(directory)) result = 1;
    xrtFree(filename);
    xrtFree(directory);
    xrtTempUnit(xrtTempCurrent());
    if (sqlite3_shutdown() != SQLITE_OK || sqlite3_memory_used() != 0) result = 1;
    return result;
}
