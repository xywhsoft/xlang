#define XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/sqlite_native.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "sqlite priority line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static xallocator backing;
static sqlite3_mem_methods sql_memory;
static xerrkind target_kind;
static int target_code, armed, hits, sql_fail;

/* 只在首错已经完整发布后拒绝分配；不会击中首次错误对象的构造。 */
static int reject_after_error(void)
{
    const xerror* error = xrtGetError();
    if (armed && xrtErrorKind(error) == target_kind && xrtErrorCode(error) == target_code) {
        ++hits;
        return 1;
    }
    return 0;
}
static void* allocate(ptr context, size_t size)
{
    (void)context;
    return reject_after_error() ? NULL : backing.Alloc(backing.Context, size);
}
static void* resize(ptr context, ptr data, size_t size)
{
    (void)context;
    return reject_after_error() ? NULL : backing.Realloc(backing.Context, data, size);
}
static void release(ptr context, ptr data)
{
    (void)context;
    backing.Free(backing.Context, data);
}
static void* sql_allocate(int size)
{
    if (sql_fail) { sql_fail = 0; return NULL; }
    return sql_memory.xMalloc(size);
}
static void* sql_resize(void* data, int size)
{
    if (sql_fail) { sql_fail = 0; return NULL; }
    return sql_memory.xRealloc(data, size);
}

enum { TEXT_BORROW, TEXT_OWN, NAME_BORROW, NAME_OWN };
enum { NULL_STMT, NEGATIVE_INDEX, LARGE_INDEX, NO_ROW, NUL_TEXT, CONVERSION_OOM };

static const char* read_column(int api, sqlite3_stmt* stmt, int column)
{
    switch (api) {
    case TEXT_BORROW: return xsqlite3_column_text_native(stmt, column);
    case TEXT_OWN: return xsqlite3_column_text_owned_native(stmt, column);
    case NAME_BORROW: return xsqlite3_column_name_native(stmt, column);
    default: return xsqlite3_column_name_owned_native(stmt, column);
    }
}

static int error_case(int api, int scenario)
{
    static const xerrkind kinds[] = {XERR_ARGUMENT, XERR_RANGE, XERR_RANGE, XERR_STATE, XERR_VALUE, XERR_MEMORY};
    static const int codes[] = {SQLITE_MISUSE, SQLITE_RANGE, SQLITE_RANGE, SQLITE_MISUSE, SQLITE_MISMATCH, SQLITE_NOMEM};
    const char* sql = scenario == NUL_TEXT ? "select cast(x'410042' as text)" :
        scenario == CONVERSION_OOM ? "select 9223372036854775807" : "select 'value' as label";
    sqlite3* db;
    sqlite3_stmt* stmt;
    xmemdebugsnapshot before, after;
    const char* text;
    int bad = 0;
    xrtClearError();
    db = xsqlite3_open_native(":memory:");
    CHECK(db && sqlite3_db_config(db, SQLITE_DBCONFIG_LOOKASIDE, NULL, 0, 0) == SQLITE_OK);
    stmt = xsqlite3_prepare_v2_native(db, sql);
    CHECK(stmt);
    if (scenario != NO_ROW) CHECK(xsqlite3_step_native(stmt) == SQLITE_ROW);
    CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtMemDebugSnapshot(&before);
    target_kind = kinds[scenario];
    target_code = codes[scenario];
    hits = 0;
    armed = 1;
    sql_fail = scenario == CONVERSION_OOM;
    text = read_column(api, scenario == NULL_STMT ? NULL : stmt,
        scenario == NEGATIVE_INDEX ? -1 : scenario == LARGE_INDEX ? 0x7fffffff : 0);
    armed = 0;
    sql_fail = 0;
    if (hits || xrtErrorKind(xrtGetError()) != target_kind ||
        xrtErrorCode(xrtGetError()) != target_code ||
        strcmp(xrtErrorDomain(xrtGetError()), "xlang.extlib.sqlite")) {
        fprintf(stderr, "api=%d scenario=%d post-error-allocations=%d kind=%d code=%d; expected kind=%d code=%d\n",
            api, scenario, hits, xrtErrorKind(xrtGetError()), xrtErrorCode(xrtGetError()), target_kind, target_code);
        bad = 1;
    }
    if (api == TEXT_OWN || api == NAME_OWN) {
        if (text != NULL) bad = 1;
        xrtFree((void*)text);
    } else if (text == NULL || text[0] != '\0') bad = 1;
    xrtClearError();
    CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    CHECK(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    CHECK(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
    int rc = xsqlite3_finalize_native(stmt);
    CHECK(rc == SQLITE_OK || (rc & 0xff) == SQLITE_NOMEM);
    CHECK(xsqlite3_exec_native(db, "create table recovery(value)") == SQLITE_OK);
    CHECK(xsqlite3_close_native(db) == SQLITE_OK);
    CHECK(sqlite3_shutdown() == SQLITE_OK && sqlite3_memory_used() == 0);
    return bad;
}

/* 正常借用出口仍需分配临时结果；该分配失败必须继续报告MemoryError。 */
static int valid_output_oom(int api, const char* sql, const char* expected)
{
    sqlite3* db = xsqlite3_open_native(":memory:");
    sqlite3_stmt* stmt = db ? xsqlite3_prepare_v2_native(db, sql) : NULL;
    CHECK(stmt && xsqlite3_step_native(stmt) == SQLITE_ROW);
    CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    xrtClearError();
    target_kind = XERR_NONE;
    target_code = 0;
    hits = 0;
    armed = 1;
    const char* text = read_column(api, stmt, 0);
    armed = 0;
    CHECK(hits > 0 && text && !*text && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    xrtClearError();
    text = read_column(api, stmt, 0);
    CHECK(text && !strcmp(text, expected) && !xrtGetError());
    CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    CHECK(xsqlite3_finalize_native(stmt) == SQLITE_OK && xsqlite3_close_native(db) == SQLITE_OK);
    CHECK(sqlite3_shutdown() == SQLITE_OK && sqlite3_memory_used() == 0);
    return 0;
}

int main(void)
{
    xallocator selected = {NULL, allocate, resize, release};
    sqlite3_mem_methods selected_sql;
    int failures = 0, cases = 0;
    xrtGetAllocator(&backing);
    CHECK(xrtSetAllocator(&selected) && xrtMemDebugEnable(true));
    CHECK(xrtTempClear() && xrtTempTrim(xrtTempCurrent(), 0));
    CHECK(sqlite3_shutdown() == SQLITE_OK);
    CHECK(sqlite3_config(SQLITE_CONFIG_GETMALLOC, &sql_memory) == SQLITE_OK);
    selected_sql = sql_memory;
    selected_sql.xMalloc = sql_allocate;
    selected_sql.xRealloc = sql_resize;
    CHECK(sqlite3_config(SQLITE_CONFIG_MALLOC, &selected_sql) == SQLITE_OK);
    for (int api = TEXT_BORROW; api <= NAME_OWN; ++api) {
        for (int scenario = NULL_STMT; scenario <= CONVERSION_OOM; ++scenario) {
            if (api >= NAME_BORROW && scenario >= NO_ROW) continue;
            failures += error_case(api, scenario);
            ++cases;
        }
    }
    CHECK(valid_output_oom(TEXT_BORROW, "select 'value' as label", "value") == 0);
    CHECK(valid_output_oom(NAME_BORROW, "select 'value' as label", "label") == 0);
    CHECK(valid_output_oom(TEXT_BORROW, "select ''", "") == 0);
    CHECK(valid_output_oom(TEXT_BORROW, "select null", "") == 0);
    CHECK(valid_output_oom(NAME_BORROW, "select 1 as \"\"", "") == 0);
    CHECK(sqlite3_config(SQLITE_CONFIG_MALLOC, &sql_memory) == SQLITE_OK);
    xrtClearError();
    xrtTempUnit(xrtTempCurrent());
    printf("sqlite first error: %d errors + 5 valid-output OOM controls, failures=%d\n", cases, failures);
    return failures ? 1 : 0;
}
