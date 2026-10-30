#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/sqlite_native.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "sqlite alloc line %d: %s\n", __LINE__, #c); return 1; } } while (0)

/* SQLite 的分配器由其官方配置接口注入；不修改 vendor，也不改变产品分配器。 */
static sqlite3_mem_methods original;
static int fail_at, calls, rejected;
static int reject(void) { if (++calls == fail_at) { ++rejected; return 1; } return 0; }
static void* allocate(int size) { return reject() ? NULL : original.xMalloc(size); }
static void* resize(void* ptr, int size) { return reject() ? NULL : original.xRealloc(ptr, size); }

/* 临时区直接经过 XRT 底层分配器，另行注入，避免只覆盖普通堆。 */
static xallocator backing;
static int backing_after = -1;
static int backing_failed;
static int reject_backing(void)
{
    if (backing_after < 0) return 0;
    if (backing_after-- == 0) { backing_failed = 1; return 1; }
    return 0;
}
static void* backing_alloc(void* context, size_t size)
{
    (void)context;
    return reject_backing() ? NULL : backing.Alloc(backing.Context, size);
}
static void* backing_realloc(void* context, void* memory, size_t size)
{
    (void)context;
    return reject_backing() ? NULL : backing.Realloc(backing.Context, memory, size);
}
static void backing_free(void* context, void* memory)
{
    (void)context;
    backing.Free(backing.Context, memory);
}

static int workload(void)
{
    void* db = xsqlite3_open_native(":memory:");
    void* stmt = NULL;
    int rc = SQLITE_NOMEM;
    if (db == NULL) return rc;
    rc = xsqlite3_exec_native(db, "create table t(value text)");
    if (rc != SQLITE_OK) goto done;
    stmt = xsqlite3_prepare_v2_native(db, "insert into t values(?)");
    if (stmt == NULL) { rc = SQLITE_NOMEM; goto done; }
    rc = xsqlite3_bind_text_native(stmt, 1, "allocation failure leaves ownership intact");
    if (rc != SQLITE_OK) goto done;
    rc = xsqlite3_step_native(stmt);
    if (rc != SQLITE_DONE) goto done;
    xsqlite3_finalize_native(stmt);
    stmt = xsqlite3_prepare_v2_native(db, "select value from t; -- trailing comment");
    if (stmt == NULL) { rc = SQLITE_NOMEM; goto done; }
    rc = xsqlite3_step_native(stmt);
    if (rc == SQLITE_ROW) rc = SQLITE_OK;
done:
    xsqlite3_finalize_native(stmt);
    xsqlite3_close_v2_native(db);
    return rc;
}

/* SQLite 可能在列访问时才分配：数值转文本、UTF-16 转 UTF-8。
 * zeroblob 另核对正文；当前 vendor 会在 step 阶段展开，列访问可以不分配。
 * 此处只注入 SQLite 分配器，区别于下面的 XRT 输出副本故障矩阵。 */
static int column_conversions(void)
{
    static const char* sql[] = {"select 9223372036854775807", "select 0.125",
        "select value from text_values", "select zeroblob(4096)", "select value from text_values"};
    static const char* expected[] = {"9223372036854775807", "0.125", "hi", NULL, "hi"};
    unsigned operations = 0, failures = 0;
    fail_at = 0;
    for (unsigned operation = 0; operation < 5; ++operation) {
        int complete = 0;
        for (int budget = 1; budget <= 32; ++budget) {
            sqlite3* db = xsqlite3_open_native(":memory:");
            CHECK(db != NULL);
            CHECK(sqlite3_db_config(db, SQLITE_DBCONFIG_LOOKASIDE, NULL, 0, 0) == SQLITE_OK);
            if (operation == 2 || operation == 4) {
                CHECK(xsqlite3_exec_native(db, "pragma encoding='UTF-16'; create table text_values(value);"
                    "insert into text_values values('hi')") == SQLITE_OK);
            }
            sqlite3_stmt* stmt = xsqlite3_prepare_v2_native(db, sql[operation]);
            CHECK(stmt != NULL);
            CHECK(xsqlite3_step_native(stmt) == SQLITE_ROW);
            xrtClearError();
            xmemdebugsnapshot before, after;
            xrtMemDebugSnapshot(&before);
            char* text = NULL;
            xbuffer* bytes = NULL;
            calls = rejected = 0;
            fail_at = budget;
            if (operation < 3) text = xsqlite3_column_text_owned_native(stmt, 0);
            else if (operation == 3) bytes = xsqlite3_column_blob_native(stmt, 0);
            else bytes = xsqlite3_column_text_bytes_native(stmt, 0);
            int hit = rejected;
            fail_at = 0;
            int memory = (sqlite3_errcode(db) & 0xff) == SQLITE_NOMEM;
            if (memory) {
                CHECK(hit > 0 && text == NULL && bytes == NULL);
                CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                CHECK(xrtErrorCode(xrtGetError()) == SQLITE_NOMEM);
                ++failures;
            } else {
                CHECK(xrtGetError() == NULL);
                if (operation < 3) CHECK(text != NULL && strcmp(text, expected[operation]) == 0);
                else if (operation == 3) {
                    CHECK(bytes != NULL && bytes->Size == 4096);
                    for (size_t i = 0; i < bytes->Size; ++i) CHECK(bytes->Data[i] == 0);
                } else CHECK(bytes != NULL && bytes->Size == 2 && memcmp(bytes->Data, "hi", 2) == 0);
            }
            xrtFree(text);
            xrtBufferDestroy(bytes);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            CHECK(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
            int finalized = xsqlite3_finalize_native(stmt);
            CHECK(finalized == SQLITE_OK || (finalized & 0xff) == SQLITE_NOMEM);
            CHECK(xsqlite3_exec_native(db, "create table recovery(value)") == SQLITE_OK);
            CHECK(xsqlite3_close_native(db) == SQLITE_OK);
            CHECK(sqlite3_shutdown() == SQLITE_OK && sqlite3_memory_used() == 0);
            if (hit == 0) {
                CHECK(operation == 3 || budget > 1);
                printf("sqlite column conversion %u: %d allocation points\n", operation, budget - 1);
                complete = 1;
                ++operations;
                break;
            }
        }
        CHECK(complete);
    }
    CHECK(operations == 5 && failures >= 4);
    printf("sqlite deferred column conversions: %u operations, %u OOM paths passed\n", operations, failures);
    return 0;
}

int main(void)
{
    xallocator selected = {NULL, backing_alloc, backing_realloc, backing_free};
    xrtGetAllocator(&backing);
    CHECK(xrtSetAllocator(&selected));
    CHECK(xrtMemDebugEnable(true));
    CHECK(sqlite3_shutdown() == SQLITE_OK);
    CHECK(sqlite3_config(SQLITE_CONFIG_GETMALLOC, &original) == SQLITE_OK);
    sqlite3_mem_methods allocator = original;
    allocator.xMalloc = allocate;
    allocator.xRealloc = resize;
    CHECK(sqlite3_config(SQLITE_CONFIG_MALLOC, &allocator) == SQLITE_OK);
    int completed = 0;
    for (fail_at = 1; fail_at <= 2048; ++fail_at) {
        calls = rejected = 0;
        int rc = workload();
        CHECK(rc == SQLITE_OK || (rc & 0xff) == SQLITE_NOMEM);
        if (rejected == 0) { CHECK(rc == SQLITE_OK); completed = 1; }
        CHECK(sqlite3_shutdown() == SQLITE_OK);
        CHECK(sqlite3_memory_used() == 0);
        xrtClearError();
        CHECK(xrtTempClear());
        if (completed) break;
    }
    CHECK(completed && fail_at > 50);
    printf("sqlite allocator: %d injected allocation points, clean recovery passed\n", fail_at - 1);
    CHECK(column_conversions() == 0);
    CHECK(sqlite3_config(SQLITE_CONFIG_MALLOC, &original) == SQLITE_OK);

    /* 返回 Buffer 的 XRT 分配失败不能破坏 SQLite 当前行，也不能转移半成品。 */
    void* db = xsqlite3_open_native(":memory:");
    void* stmt = xsqlite3_prepare_v2_native(db, "select x'0001ff', 'retained text'");
    CHECK(stmt != NULL && xsqlite3_step_native(stmt) == SQLITE_ROW);
    unsigned failed = 0, passed = 0;
    for (uint64 i = 0; i < 8; ++i) {
        xrtClearError();
        xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        CHECK(xrtMemDebugFailAfter(i));
        xbuffer* bytes = xsqlite3_column_blob_native(stmt, 0);
        xrtMemDebugFailClear();
        if (bytes == NULL) { ++failed; CHECK(xrtErrorKind(xrtGetError()) == XERR_MEMORY); }
        else { ++passed; CHECK(bytes->Size == 3 && bytes->Data[2] == 255); xrtBufferDestroy(bytes); }
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        CHECK(before.LiveBytes == after.LiveBytes && before.LiveCount == after.LiveCount);
        CHECK(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    }
    CHECK(failed > 0 && passed > 0);
    unsigned temp_failed = 0, temp_passed = 0;
    CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    for (int i = 0; i < 8; ++i) {
        xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        backing_failed = 0;
        backing_after = i;
        const char* text = xsqlite3_column_text_native(stmt, 1);
        backing_after = -1;
        if (backing_failed) {
            ++temp_failed;
            CHECK(text[0] == 0 && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else { ++temp_passed; CHECK(strcmp(text, "retained text") == 0); }
        xrtClearError();
        CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
        xrtMemDebugSnapshot(&after);
        CHECK(before.LiveBytes == after.LiveBytes && before.LiveCount == after.LiveCount);
        CHECK(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
    }
    CHECK(temp_failed > 0 && temp_passed > 0);
    CHECK(xsqlite3_finalize_native(stmt) == SQLITE_OK && xsqlite3_close_native(db) == SQLITE_OK);
    CHECK(sqlite3_shutdown() == SQLITE_OK && sqlite3_memory_used() == 0);
    xrtTempUnit(xrtTempCurrent());
    printf("sqlite XRT buffer allocator: failed=%u passed=%u\n", failed, passed);
    printf("sqlite XRT temporary allocator: failed=%u passed=%u\n", temp_failed, temp_passed);
    return 0;
}
