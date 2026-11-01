#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include "../include/sqlite_native.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <string.h>

static const char* fail_sql;
static uintptr_t consumed;
static unsigned injected, repeated;
static char* owned_directory;

static char* remember_directory(const char* parent, const char* prefix, const char* suffix)
{
    char* directory = xrtDirTemp(parent, prefix, suffix);
    if (directory) owned_directory = xrtMemDup(directory, strlen(directory) + 1u);
    return directory;
}

/* 先真实 finalize，再注入错误返回。重入只记录，不把已释放句柄再交给
 * SQLite，故 Windows 测试也能安全捕获旧范例的二次释放意图。 */
static int consuming_finalize(void* statement)
{
    uintptr_t identity = (uintptr_t)statement;
    if (injected && statement && identity == consumed) { ++repeated; return SQLITE_MISUSE; }
    const char* sql = statement ? sqlite3_sql(statement) : NULL;
    int target = sql && strcmp(sql, fail_sql) == 0;
    int rc = xsqlite3_finalize_native(statement);
    if (target && !injected) { consumed = identity; ++injected; return SQLITE_ERROR; }
    return rc;
}

#define main wal_example_main
#define xsqlite3_finalize_native consuming_finalize
#define xrtDirTemp remember_directory
#include "../examples/wal_snapshots_c.c"
#undef xrtDirTemp
#undef xsqlite3_finalize_native
#undef main

int main(void)
{
    static const char* targets[] = {"pragma journal_mode=wal", "insert into items values(?)"};
    unsigned failures = 0;
    for (size_t at = 0; at < sizeof(targets) / sizeof(targets[0]); ++at) {
        fail_sql = targets[at]; consumed = 0; injected = repeated = 0; owned_directory = NULL;
        int result = wal_example_main();
        // 预期的失败返回不能掩盖文件清理失败，必须确认目录已确实不存在。
        xfileinfo info;
        xrtClearError();
        bool removed = owned_directory && !xrtPathStat(owned_directory, false, &info) &&
            xrtErrorKind(xrtGetError()) == XERR_NOT_FOUND;
        xrtClearError();
        if (result != 1 || injected != 1 || repeated || !removed || sqlite3_memory_used() != 0) {
            ++failures;
            fprintf(stderr, "WAL cleanup case=%zu result=%d injected=%u repeated=%u removed=%d\n", at, result, injected, repeated, removed);
        }
        xrtFree(owned_directory);
    }
    printf("sqlite WAL finalizer failure: 2 cases, %u failures\n", failures);
    return failures ? 1 : 0;
}
