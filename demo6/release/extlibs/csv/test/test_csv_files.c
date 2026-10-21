#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/csv_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const unsigned char marker[] = "keep existing destination";
static const unsigned char encoded[] = "left,right\n";

static void same_file(cstr path, const unsigned char* expected, size_t size)
{
    size_t actualSize = 0;
    bytes actual = xrtFileReadAll(path, &actualSize);
    assert(actual && actualSize == size && !memcmp(actual, expected, size));
    xrtFree(actual);
}

static void seed(cstr path)
{
    assert(xrtFileWriteAll(path, (xbytesview){marker, sizeof(marker) - 1}));
}

static void no_temporary(cstr directory)
{
    xdirentry entry;
    xdirnext next;
    xdir dir = xrtDirOpen(directory, 0);
    assert(dir);
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM) {
        bool temporary = entry.Name.Size >= 9 && !memcmp(entry.Name.Data, ".xrt-csv-", 9);
        if (temporary) fprintf(stderr, "CSV temporary remains: %.*s\n", (int)entry.Name.Size, entry.Name.Data);
        assert(!temporary);
    }
    assert(next == XDIR_NEXT_END && xrtDirClose(dir));
}

static void unchanged(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static void transactions(cstr directory, cstr path)
{
    const str fields[] = {"left", "right"};
    xrtcsvdialect dialect;
    xrtcsvwriter* writer;
    str cwd = xrtPathCwd();
    str absolute = xrtPathAbs(directory);
    assert(cwd && absolute);
    seed(path);
    writer = xrtCsvWriterOpenFile(path, NULL);
    assert(writer && xrtCsvWriterWrite(writer, 2, fields));
    same_file(path, marker, sizeof(marker) - 1);
    // 提交目标在打开时固定，不随宿主随后切换工作目录而改变。
    assert(xrtPathSetCwd(absolute));
    assert(xrtCsvWriterClose(writer) && xrtCsvWriterClose(writer));
    assert(xrtPathSetCwd(cwd));
    xrtCsvWriterDestroy(writer);
    same_file(path, encoded, sizeof(encoded) - 1);
    no_temporary(directory);
    xrtFree(cwd); xrtFree(absolute);

    seed(path);
    writer = xrtCsvWriterOpenFile(path, NULL);
    assert(writer && xrtCsvWriterWrite(writer, 2, fields));
    xrtSetErrorInfo(XERR_ARGUMENT, "test.csv", 913, "preserve caller diagnostic");
    xrtCsvWriterDestroy(writer);
    assert(xrtErrorCode(xrtGetError()) == 913);
    xrtClearError();
    same_file(path, marker, sizeof(marker) - 1);
    no_temporary(directory);

    xrtCsvDialectInit(&dialect);
    dialect.iQuote = dialect.iDelimiter;
    assert(xrtCsvWriterOpenFile(path, &dialect) == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    xrtClearError();
    same_file(path, marker, sizeof(marker) - 1);
    no_temporary(directory);

    xrtCsvDialectInit(&dialect);
    dialect.tLimits.iMaxOutputBytes = 3;
    writer = xrtCsvWriterOpenFile(path, &dialect);
    assert(writer && !xrtCsvWriterWrite(writer, 2, fields) && !xrtCsvWriterClose(writer));
    xrtCsvWriterDestroy(writer);
    xrtClearError();
    same_file(path, marker, sizeof(marker) - 1);
    no_temporary(directory);

    // 已存在的目录不是可替换的文件；提交失败须移除旁路临时文件。
    {
        str target = xrtPathJoin(directory, "directory-target");
        assert(target && xrtDirCreateAll(target));
        writer = xrtCsvWriterOpenFile(target, NULL);
        assert(writer && xrtCsvWriterWrite(writer, 2, fields));
        assert(!xrtCsvWriterClose(writer) && xrtErrorKind(xrtGetError()) == XERR_IO);
        xrtCsvWriterDestroy(writer);
        xrtClearError();
        assert(xrtDirRemove(target));
        xrtFree(target);
        no_temporary(directory);
    }
}

static unsigned int allocation_failures(cstr directory, cstr target, bool exists)
{
    unsigned int rejected = 0, complete = 0;
    for (unsigned int i = 0; i < 256 && complete < 2; ++i) {
        xmemdebugsnapshot before;
        void* writer;
        bool ok, injected;
        if (exists) seed(target);
        else assert(!xrtPathExists(target));
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(i));
        writer = xcsv_writer_open_file(target, ",", "\"", 0, false, false, 0, 0, 0, 0, 0);
        ok = writer && xcsv_writer_begin(writer) && xcsv_writer_field(writer, "left") &&
            xcsv_writer_field(writer, "right") && xcsv_writer_end(writer) && xcsv_writer_close(writer);
        xcsv_writer_destroy(writer);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            if (ok || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                fprintf(stderr, "file allocation %u exists=%d kind=%d\n", i, exists, xrtErrorKind(xrtGetError()));
            assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else { assert(ok && !xrtGetError()); ++complete; }
        unchanged(&before);
        if (ok) same_file(target, encoded, sizeof(encoded) - 1);
        else if (exists) same_file(target, marker, sizeof(marker) - 1);
        else assert(!xrtPathExists(target));
        no_temporary(directory);
        if (!exists && ok) assert(xrtFileDelete(target));
    }
    assert(complete == 2 && rejected > 0);
    return rejected;
}

static unsigned int reader_failures(cstr target)
{
    unsigned int rejected = 0, complete = 0;
    for (unsigned int i = 0; i < 128 && complete < 2; ++i) {
        xmemdebugsnapshot before;
        void* reader;
        str field = NULL;
        bool ok, injected;
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(i));
        reader = xcsv_reader_open_file(target, ",", "\"", true, 0, 0, 0, 0, 0);
        ok = reader && xcsv_reader_next(reader);
        if (ok) { field = xcsv_reader_field(reader, 0); ok = field != NULL; }
        xcsv_reader_destroy(reader);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) { assert(!ok && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++rejected; }
        else { assert(ok && !strcmp(field, "left") && !xrtGetError()); ++complete; }
        xrtFree(field);
        unchanged(&before);
    }
    assert(complete == 2 && rejected > 0);
    return rejected;
}

int main(int argc, char** argv)
{
    cstr parent = argc > 1 ? argv[1] : "build/test";
    str directory, target, absent;
    xmemdebugsnapshot before;
    unsigned int rejected;
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    assert(xrtDirCreateAll(parent));
    directory = xrtDirTemp(parent, "csv-files-", "");
    assert(directory);
    target = xrtPathJoin(directory, "\xe8\xbe\x93\xe5\x87\xba.csv");
    absent = xrtPathJoin(directory, "new.csv");
    assert(target && absent);
    transactions(directory, target);
    rejected = allocation_failures(directory, target, true);
    rejected += allocation_failures(directory, absent, false);
    rejected += reader_failures(target);
    assert(xrtFileDelete(target) && xrtDirRemove(directory));
    xrtFree(target); xrtFree(absent); xrtFree(directory);
    unchanged(&before);
    printf("csv file transactions: %u failure points, UTF-8 and cleanup passed\n", rejected);
    return 0;
}
