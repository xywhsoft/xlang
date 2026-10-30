#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_TEMP_MEMORY
#include "xrt.h"
#include "../include/sqlite_native.h"
#ifdef __TINYC__
/* 单文件直跑复用同一份 SQLite/XRT；正式包仍分别编译清单中的源文件。 */
#include "../third_party/sqlite3.c"
#include "../sqlite_native.c"
#else
#include "../third_party/sqlite3.h"
#endif
#include <stdio.h>
#include <string.h>

int main(void)
{
    unsigned char payload[] = {65, 0, 66};
    static const unsigned char expected[] = {65, 0, 66};
    void* db = xsqlite3_open_native(":memory:");
    void* stmt = NULL;
    xbuffer *text = NULL, *blob = NULL;
    const char* failed = "open";
    int rc;
    if (!db) goto cleanup;

    failed = "bind exact-length TEXT and BLOB";
    stmt = xsqlite3_prepare_v2_native(db, "select ?, ?");
    if (!stmt || xsqlite3_bind_text_bytes_native(stmt, 1, payload, sizeof(payload)) != SQLITE_OK ||
        xsqlite3_bind_blob_native(stmt, 2, payload, sizeof(payload)) != SQLITE_OK) goto cleanup;
    memset(payload, 0, sizeof(payload));  /* SQLITE_TRANSIENT 已复制绑定值。 */
    failed = "read current row";
    if (xsqlite3_step_native(stmt) != SQLITE_ROW ||
        xsqlite3_column_type_native(stmt, 0) != SQLITE_TEXT ||
        xsqlite3_column_type_native(stmt, 1) != SQLITE_BLOB) goto cleanup;
    text = xsqlite3_column_text_bytes_native(stmt, 0);
    blob = xsqlite3_column_blob_native(stmt, 1);
    if (!text || !blob) goto cleanup;
    failed = "finalize and close";
    rc = xsqlite3_finalize_native(stmt);
    stmt = NULL;
    if (rc != SQLITE_OK || xsqlite3_close_native(db) != SQLITE_OK) goto cleanup;
    db = NULL;

    /* 两个 Buffer 均为调用方拥有的副本，关闭语句和连接不会让它们失效。 */
    failed = "retained result bytes";
    if (text->Size != sizeof(expected) || blob->Size != sizeof(expected) ||
        memcmp(text->Data, expected, sizeof(expected)) != 0 ||
        memcmp(blob->Data, expected, sizeof(expected)) != 0) goto cleanup;
    printf("TEXT: %zu bytes\nBLOB: %zu bytes\nmiddle byte: %u\n",
        text->Size, blob->Size, (unsigned)text->Data[1]);
    failed = NULL;

cleanup:
    if (stmt) xsqlite3_finalize_native(stmt);
    if (db) xsqlite3_close_v2_native(db);
    xrtBufferDestroy(text);
    xrtBufferDestroy(blob);
    sqlite3_shutdown();
    if (failed) fprintf(stderr, "sqlite text-byte example failed: %s\n", failed);
    return failed ? 1 : 0;
}
