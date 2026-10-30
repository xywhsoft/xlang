#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_DIR
#include "../include/xrt_tar.h"
#include <stdio.h>
#include <string.h>

static unsigned char data[131073];

static xbuffer* archive(void)
{
    xrttarentry entry;
    xrttarwriter* writer = xrtTarWriterCreateMemory(NULL);
    xbuffer* result = NULL;
    xrtTarEntryInit(&entry);
    entry.sName = "data.bin";
    if (writer != NULL && xrtTarWriterAdd(writer, &entry, data, sizeof(data)) &&
        xrtTarWriterClose(writer)) result = xrtTarWriterTakeBuffer(writer);
    xrtTarWriterDestroy(writer);
    return result;
}

static int valid_archive(const void* input, size_t size, cstr name, size_t body_size)
{
    xrttarreader* reader = xrtTarReaderOpenMemory(input, size, NULL);
    xrttarentry entry;
    xbuffer* body = NULL;
    int ok = 0;
    if (reader == NULL) return 0;
    if (name != NULL) {
        if (!xrtTarReaderNext(reader, &entry) || strcmp(entry.sName, name) != 0) goto done;
        body = xrtTarReaderRead(reader);
        if (body == NULL || body->Size != body_size || memcmp(body->Data, data, body_size) != 0) goto done;
    }
    ok = !xrtTarReaderNext(reader, &entry) && xrtTarReaderEOF(reader);
done:
    xrtTarReaderDestroy(reader);
    xrtBufferDestroy(body);
    return ok;
}

static int file_equals(cstr path, const void* expected, size_t size)
{
    size_t length;
    bytes contents = xrtFileReadAll(path, &length);
    int ok = contents != NULL && length == size && memcmp(contents, expected, size) == 0;
    xrtFree(contents);
    return ok;
}

static int no_temporary_files(cstr directory)
{
    xdir dir = xrtDirOpen(directory, 0);
    xdirentry entry;
    xdirnext next;
    int ok = 1;
    if (dir == NULL) return 0;
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM) {
        if (strncmp(entry.Name.Data, ".xrt-tar-", 9u) == 0) ok = 0;
    }
    ok &= next == XDIR_NEXT_END;
    ok &= xrtDirClose(dir);
    return ok;
}

/* 元数据阶段失败可完成空归档；一旦写入帧后失败，就不能发布半个条目。 */
static int writer_faults(const xbuffer* packed, cstr directory, cstr source,
    cstr target, int operation, cstr name)
{
    unsigned budget;
    for (budget = 0; budget < 96u; ++budget) {
        xmemdebugsnapshot before, after;
        xrttarwriter* writer = NULL;
        xrttarreader* reader = NULL;
        xreader* input = NULL;
        xbuffer* result = NULL;
        xbuffer* external = NULL;
        xrttarentry entry;
        bool completed = false, triggered = false;
        int code, ok = 1;
        xerrkind kind;
        size_t body_size = operation == 4 ? 65024u : sizeof(data);
        const void* original = operation == 7 ? (const void*)packed->Data : (const void*)"OLD";
        size_t original_size = operation == 7 ? packed->Size : 3u;
        xrtClearError();
        if ((operation == 5 || operation == 7 || operation == 8) &&
            !xrtFileWriteAll(target, (xbytesview){original, original_size})) return 0;
        xrtMemDebugSnapshot(&before);
        xrtTarEntryInit(&entry);
        entry.sName = name;
        if (operation == 4) {
            xwriter* output;
            external = xrtBufferCreate();
            output = external != NULL ? xrtWriterFromBuffer(external) : NULL;
            writer = output != NULL ? xrtTarWriterCreate(output, true, NULL) : NULL;
            if (writer == NULL) (void)xrtWriterDestroy(output);
        } else if (operation == 8) writer = xrtTarWriterOpenFile(target, NULL);
        else if (operation != 0 && operation != 5 && operation != 7) writer = xrtTarWriterCreateMemory(NULL);
        if (operation == 2) input = xrtReaderFromMemory((xbytesview){data, sizeof(data)});
        if (operation == 3) {
            reader = xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL);
            ok = reader != NULL && xrtTarReaderNext(reader, &entry);
        }
        if (operation == 4 || operation == 8) {
            ok = writer != NULL && xrtTarWriterAdd(writer, &entry, data, body_size);
            /* 用公开 Buffer 裁剪容量，确保写结尾时真实经过扩容失败路径。 */
            if (operation == 4) ok = ok && xrtBufferTrim(external);
        }
        if (!ok || (operation == 2 && input == NULL) ||
            (operation != 0 && operation != 5 && operation != 7 && writer == NULL)) goto cleanup;
        if (!xrtMemDebugFailAfter(budget)) { ok = 0; goto cleanup; }
        if (operation == 0) writer = xrtTarWriterCreateMemory(NULL);
        else if (operation == 1) completed = xrtTarWriterAdd(writer, &entry, data, body_size);
        else if (operation == 2) completed = xrtTarWriterAddFrom(writer, &entry, input, body_size);
        else if (operation == 3) completed = xrtTarWriterCopyCurrent(writer, reader);
        else if (operation == 4) completed = xrtTarWriterClose(writer);
        else if (operation == 5) writer = xrtTarWriterOpenFile(target, NULL);
        else if (operation == 6) completed = xrtTarWriterAddFile(writer, name, source, 0644u, 0);
        else if (operation == 7) writer = xrtTarWriterOpenAppend(target, NULL);
        else completed = xrtTarWriterClose(writer);
        triggered = xrtMemDebugFailTriggered();
        code = writer != NULL ? xrtTarWriterError(writer)->iCode : XRT_TAR_ERROR_MEMORY;
        kind = xrtGetError() != NULL ? xrtErrorKind(xrtGetError()) : XERR_NONE;
        xrtMemDebugFailClear();
        if (operation == 0 || operation == 5 || operation == 7) {
            completed = writer != NULL && code == XRT_TAR_OK;
        }
        ok = triggered ? !completed && code == XRT_TAR_ERROR_MEMORY && kind == XERR_MEMORY : completed;
        if (!ok) fprintf(stderr, "writer fault: op=%d budget=%u hit=%d completed=%d code=%d kind=%d\n",
            operation, budget, (int)triggered, (int)completed, code, (int)kind);
        xrtClearError();
        if (operation == 0 || (operation >= 1 && operation <= 4) || operation == 6) {
            if (xrtTarWriterClose(writer)) {
                if (operation == 4) {
                    ok &= !triggered && valid_archive(external->Data, external->Size, name, body_size);
                } else {
                    result = xrtTarWriterTakeBuffer(writer);
                    ok &= result != NULL && valid_archive(result->Data, result->Size,
                        !triggered && operation != 0 ? name : NULL, body_size);
                }
            } else ok &= triggered;
        } else if (!triggered) ok &= xrtTarWriterClose(writer);
cleanup:
        xrtMemDebugFailClear();
        xrtBufferDestroy(result);
        xrtTarWriterDestroy(writer);
        xrtBufferDestroy(external);
        xrtTarReaderDestroy(reader);
        (void)xrtReaderDestroy(input);
        xrtMemDebugFailClear();
        if ((operation == 5 || operation == 7 || operation == 8) && triggered) {
            ok &= file_equals(target, original, original_size);
        } else if ((operation == 5 || operation == 7 || operation == 8) && ok) {
            size_t length;
            bytes contents = xrtFileReadAll(target, &length);
            ok = contents != NULL && valid_archive(contents, length,
                operation == 5 ? NULL : "data.bin", sizeof(data));
            xrtFree(contents);
        }
        if (!no_temporary_files(directory)) {
            fprintf(stderr, "TAR temporary file survived: op=%d budget=%u\n", operation, budget);
            ok = 0;
        }
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes) {
            fprintf(stderr, "writer allocation imbalance: op=%d budget=%u count=%zu/%zu bytes=%zu/%zu\n",
                operation, budget, before.LiveCount, after.LiveCount, before.LiveBytes, after.LiveBytes);
            ok = 0;
        }
        if (!ok) return 0;
        if (!triggered) {
            printf("TAR writer operation %d: %u failures and success\n", operation, budget);
            return budget > 0 || operation == 8;
        }
    }
    return 0;
}

/* 单独把下一次故障放在析构入口，不能只证明业务操作失败后的清理成功。 */
static int destroy_without_allocation(cstr directory, cstr target)
{
    xmemdebugsnapshot before, after;
    xrttarwriter* writer;
    int ok;
    xrtClearError();
    if (!xrtFileWriteAll(target, XRT_BYTES_LITERAL("OLD"))) return 0;
    xrtMemDebugSnapshot(&before);
    writer = xrtTarWriterOpenFile(target, NULL);
    if (writer == NULL || xrtTarWriterError(writer)->iCode != XRT_TAR_OK) {
        xrtTarWriterDestroy(writer);
        return 0;
    }
    if (!xrtMemDebugFailAfter(0)) { xrtTarWriterDestroy(writer); return 0; }
    xrtTarWriterDestroy(writer);
    ok = !xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    ok &= no_temporary_files(directory) && file_equals(target, "OLD", 3u);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    ok &= before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes;
    if (!ok) fputs("TAR open transaction destruction allocated or left resources\n", stderr);
    return ok;
}

int main(int argc, char** argv)
{
    char long_name[601];
    unsigned i;
    int ok = 1;
    xbuffer* packed;
    str source, target;
    xrttarwriter* warm;
    if (argc == 3 && strcmp(argv[2], "--check-clean") == 0) {
        return no_temporary_files(argv[1]) ? 0 : 1;
    }
    if (argc != 2 || !xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 2;
    xrtMemDebugFailClear();
    for (i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)i;
    memset(long_name, 'n', sizeof(long_name) - 1u);
    long_name[sizeof(long_name) - 1u] = 0;
    packed = archive();
    source = xrtFormat("%s/source.bin", argv[1]);
    target = xrtFormat("%s/transaction.tar", argv[1]);
    if (packed == NULL || source == NULL || target == NULL ||
        !xrtFileWriteAll(source, (xbytesview){data, sizeof(data)})) { ok = 0; goto done; }
    warm = xrtTarWriterOpenFile(target, NULL);
    ok = warm != NULL && xrtTarWriterClose(warm);
    xrtTarWriterDestroy(warm);
    if (!ok) goto done;
    for (i = 0; i < 9u && ok; ++i) {
        ok = writer_faults(packed, argv[1], source, target, (int)i, "data.bin");
    }
    if (ok) ok = writer_faults(packed, argv[1], source, target, 1, long_name);
    if (ok) ok = destroy_without_allocation(argv[1], target);
done:
    xrtBufferDestroy(packed);
    xrtFree(source);
    xrtFree(target);
    if (ok) puts("TAR writer allocation boundaries: PASS");
    return ok ? 0 : 1;
}
