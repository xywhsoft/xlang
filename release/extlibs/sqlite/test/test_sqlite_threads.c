#define XRT_MODULE_THREAD
#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include "../include/sqlite_native.h"
#include "../third_party/sqlite3.h"
/* 线程内的数据库调用位于断言中，发布配置也必须执行。 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int run_worker(void* data)
{
    const char* name = data;
    for (int i = 0; i < 100; ++i) {
        void* db = xsqlite3_open_native(":memory:");
        assert(db != NULL);
        void* stmt = xsqlite3_prepare_v2_native(db, "select ?");
        assert(stmt != NULL && xsqlite3_bind_text_native(stmt, 1, name) == SQLITE_OK);
        assert(xsqlite3_step_native(stmt) == SQLITE_ROW);
        const char* text = xsqlite3_column_text_native(stmt, 0);
        for (int n = 0; n < 20; ++n) {
            assert(strcmp(xsqlite3_column_name_native(stmt, 0), "?") == 0);
            xrtThreadYield();
        }
        assert(strcmp(text, name) == 0);
        assert(xsqlite3_finalize_native(stmt) == SQLITE_OK && xsqlite3_close_native(db) == SQLITE_OK);
        assert(strcmp(text, name) == 0);
        assert(xsqlite3_open_v2_native(":memory:", SQLITE_OPEN_READWRITE, name) == NULL);
        xrtThreadYield();
        assert(strstr(xsqlite_last_error_native(), name) != NULL);
        xrtClearError();
        assert(xrtTempClear());
    }
    xrtTempUnit(xrtTempCurrent());
    return 0;
}

int main(void)
{
    const char* names[] = {"sqlite-worker-0", "sqlite-worker-1", "sqlite-worker-2", "sqlite-worker-3"};
    xthread* threads[4];
    assert(sqlite3_initialize() == SQLITE_OK);
    for (int i = 0; i < 4; ++i) { threads[i] = xrtThreadCreate(run_worker, (void*)names[i], 0); assert(threads[i]); }
    for (int i = 0; i < 4; ++i) {
        assert(xrtThreadWait(threads[i]) == XWAIT_OK && xrtThreadExitCode(threads[i]) == 0);
        xrtThreadDestroy(threads[i]);
    }
    assert(sqlite3_shutdown() == SQLITE_OK && sqlite3_memory_used() == 0);
    puts("sqlite threads: 4 independent connections, text lifetime and error isolation passed");
    return 0;
}
