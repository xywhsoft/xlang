#define XRT_MODULE_IO_BUFFER
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
    void* stmt = NULL;
    char* text = NULL;
    char* integer = NULL;
    xbuffer* utf8 = NULL;
    xbuffer* zeros = NULL;
    int result = 1;
    if (db == NULL) goto done;
    /* SQLite 存储 UTF-16，公开列接口转换为 UTF-8，并复制到独立的 XRT 存储。 */
    if (xsqlite3_exec_native(db, "pragma encoding='UTF-16'; create table entry(name text);"
        "insert into entry values('你好')") != SQLITE_OK) goto done;
    stmt = xsqlite3_prepare_v2_native(db,
        "select name, 9223372036854775807, zeroblob(4096) from entry");
    if (stmt == NULL || xsqlite3_step_native(stmt) != SQLITE_ROW) goto done;
    text = xsqlite3_column_text_owned_native(stmt, 0);
    integer = xsqlite3_column_text_owned_native(stmt, 1);
    utf8 = xsqlite3_column_text_bytes_native(stmt, 0);
    zeros = xsqlite3_column_blob_native(stmt, 2);
    if (text == NULL || integer == NULL || utf8 == NULL || zeros == NULL) goto done;
    int rc = xsqlite3_finalize_native(stmt);
    stmt = NULL;
    if (rc != SQLITE_OK) goto done;
    rc = xsqlite3_close_native(db);
    if (rc != SQLITE_OK) goto done;
    db = NULL;

    /* 只释放拥有型结果；不保留 SQLite 的借用列指针。 */
    if (strcmp(text, "你好") != 0 || strcmp(integer, "9223372036854775807") != 0 ||
        utf8->Size != 6 || memcmp(utf8->Data, text, 6) != 0 || zeros->Size != 4096) goto done;
    for (size_t i = 0; i < zeros->Size; ++i) if (zeros->Data[i] != 0) goto done;
    printf("text: %s\ninteger: %s\nutf8 bytes: %zu\nzero blob: %zu\n",
        text, integer, utf8->Size, zeros->Size);
    result = 0;
done:
    (void)xsqlite3_finalize_native(stmt);
    (void)xsqlite3_close_v2_native(db);
    xrtFree(text);
    xrtFree(integer);
    xrtBufferDestroy(utf8);
    xrtBufferDestroy(zeros);
    xrtTempUnit(xrtTempCurrent());
    (void)sqlite3_shutdown();
    if (result != 0) fputs("SQLite column storage example failed\n", stderr);
    return result;
}
