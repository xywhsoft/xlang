#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_FILE_WHOLE
#include "../include/xrt_tar.h"
#include <stdio.h>
#include <string.h>

static xbuffer* archive(cstr name, uint64 uid)
{
    xrttarwriter* writer = xrtTarWriterCreateMemory(NULL);
    xbuffer* result = NULL;
    xrttarentry entry;
    xrtTarEntryInit(&entry);
    entry.sName = name;
    entry.sLinkName = "link-target";
    entry.sUserName = "reader-user";
    entry.sGroupName = "reader-group";
    entry.iMode = 0644u;
    entry.iUID = uid;
    if (writer != NULL && xrtTarWriterAdd(writer, &entry, "data", 4u) &&
        xrtTarWriterClose(writer)) result = xrtTarWriterTakeBuffer(writer);
    xrtTarWriterDestroy(writer);
    return result;
}

static int metadata(const xrttarentry* entry, cstr name, cstr link, uint64 uid)
{
    return entry->sName != NULL && strcmp(entry->sName, name) == 0 &&
        entry->sLinkName != NULL && strcmp(entry->sLinkName, link) == 0 &&
        entry->sUserName != NULL && strcmp(entry->sUserName, "reader-user") == 0 &&
        entry->sGroupName != NULL && strcmp(entry->sGroupName, "reader-group") == 0 &&
        entry->iUID == uid && entry->iSizeBytes == 4u;
}

/* 构造、Next、Read 分开注入；快照只比较清理后的活动分配。 */
static int reader_faults(const xbuffer* packed, cstr name, cstr link, uint64 uid,
    int operation, cstr file)
{
    unsigned budget;
    int failures = 0;
    for (budget = 0; budget < 64u; ++budget) {
        xmemdebugsnapshot before, after;
        xrttarreader* reader = NULL;
        xrttarentry entry, untouched;
        xbuffer* body = NULL;
        bool completed = false, triggered;
        xerrkind kind;
        int code;
        int ok;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        if (operation == 1 || operation == 2) {
            reader = xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL);
            if (reader == NULL || xrtTarReaderError(reader)->iCode != XRT_TAR_OK) return 0;
            if (operation == 2 && !xrtTarReaderNext(reader, &entry)) {
                xrtTarReaderDestroy(reader);
                return 0;
            }
        }
        memset(&entry, 0xa5, sizeof(entry));
        untouched = entry;
        if (!xrtMemDebugFailAfter(budget)) return 0;
        if (operation == 0) reader = xrtTarReaderOpenMemory(packed->Data, packed->Size, NULL);
        else if (operation == 1) completed = xrtTarReaderNext(reader, &entry);
        else if (operation == 2) {
            body = xrtTarReaderRead(reader);
            completed = body != NULL;
        } else reader = xrtTarReaderOpenFile(file, NULL);
        triggered = xrtMemDebugFailTriggered();
        kind = xrtGetError() != NULL ? xrtErrorKind(xrtGetError()) : XERR_NONE;
        code = reader != NULL ? xrtTarReaderError(reader)->iCode : XRT_TAR_ERROR_MEMORY;
        xrtMemDebugFailClear();
        if (triggered) {
            ok = !completed && kind == XERR_MEMORY && code == XRT_TAR_ERROR_MEMORY;
            if (operation == 1) {
                ok &= memcmp(&entry, &untouched, sizeof(entry)) == 0 &&
                    !xrtTarReaderNext(reader, &entry) && !xrtTarReaderEOF(reader);
            } else if (operation == 2) {
                /* 输出分配失败尚未消费正文；清除注入后原条目仍可读取。 */
                xrtClearError();
                body = xrtTarReaderRead(reader);
                ok &= body != NULL && body->Size == 4u && memcmp(body->Data, "data", 4u) == 0;
            }
            ++failures;
        } else if (operation == 0 || operation == 3) {
            ok = reader != NULL && code == XRT_TAR_OK &&
                xrtTarReaderNext(reader, &entry) && metadata(&entry, name, link, uid);
        } else if (operation == 1) {
            ok = completed && code == XRT_TAR_OK && metadata(&entry, name, link, uid);
        } else ok = completed && body->Size == 4u && memcmp(body->Data, "data", 4u) == 0;
        if (!ok) fprintf(stderr, "reader fault: op=%d budget=%u hit=%d completed=%d code=%d kind=%d\n",
            operation, budget, (int)triggered, (int)completed, code, (int)kind);
        xrtBufferDestroy(body);
        xrtTarReaderDestroy(reader);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes) {
            fprintf(stderr, "reader allocation imbalance: op=%d budget=%u count=%zu/%zu bytes=%zu/%zu\n",
                operation, budget, before.LiveCount, after.LiveCount, before.LiveBytes, after.LiveBytes);
            ok = 0;
        }
        if (!ok) return 0;
        if (!triggered) {
            printf("TAR reader operation %d: %u failures and success\n", operation, budget);
            return failures > 0;
        }
    }
    return 0;
}

static void checksum(unsigned char* header)
{
    unsigned sum = 0, i;
    memset(header + 148u, ' ', 8u);
    for (i = 0; i < 512u; ++i) sum += header[i];
    (void)snprintf((char*)header + 148u, 8u, "%06o", sum);
    header[155] = ' ';
}

/* GNU 扩展与普通文件分帧；复用真实 USTAR 头，但不复用被测解析器编码。 */
static xbuffer* gnu_extension(const xbuffer* plain, char type, cstr value)
{
    unsigned char frame[1024] = {0};
    size_t length = strlen(value) + 1u;
    xbuffer* result;
    if (length > 512u) return NULL;
    memcpy(frame, plain->Data, 512u);
    frame[156] = (unsigned char)type;
    (void)snprintf((char*)frame + 124u, 12u, "%011llo", (unsigned long long)length);
    checksum(frame);
    memcpy(frame + 512u, value, length);
    result = xrtBufferCreate();
    if (result != NULL && xrtBufferAppend(result, (xbytesview){frame, sizeof(frame)}) &&
        xrtBufferAppend(result, (xbytesview){plain->Data, plain->Size})) return result;
    xrtBufferDestroy(result);
    return NULL;
}

static int invalid_extension(const xbuffer* packed)
{
    xbuffer* invalid = xrtBufferCreate();
    xrttarreader* reader = NULL;
    xrttarentry entry, untouched;
    int ok = 0;
    if (invalid == NULL || !xrtBufferAppend(invalid, (xbytesview){packed->Data, packed->Size})) goto done;
    if (invalid->Data[156] == 'x' || invalid->Data[156] == 'g') {
        uint64 size;
        if (!xrtUIntParse((xstrview){(cstr)invalid->Data + 124, 11}, 8, 0, &size)) goto done;
        invalid->Data[512u + (size_t)size - 1u] = '!';
    } else invalid->Data[512] = 0xffu;
    reader = xrtTarReaderOpenMemory(invalid->Data, invalid->Size, NULL);
    if (reader == NULL) goto done;
    memset(&entry, 0xa5, sizeof(entry));
    untouched = entry;
    /* 上次操作的 OOM 不能把这次真正的格式错误误分为内存故障。 */
    xrtSetErrorInfo(XERR_MEMORY, "tar.test.stale", 1, "old allocation failure");
    ok = !xrtTarReaderNext(reader, &entry) && !xrtTarReaderEOF(reader) &&
        memcmp(&entry, &untouched, sizeof(entry)) == 0 &&
        xrtTarReaderError(reader)->iCode == XRT_TAR_ERROR_FORMAT &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL;
    if (!ok) fprintf(stderr, "invalid extension classification: type=%c\n", packed->Data[156]);
done:
    xrtTarReaderDestroy(reader);
    xrtBufferDestroy(invalid);
    xrtClearError();
    return ok;
}

static int create_ownership(const xbuffer* packed)
{
    xmemdebugsnapshot before, after;
    xreader* input;
    xrttarreader* reader;
    uint64 size = 0;
    int ok;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    input = xrtReaderFromMemory((xbytesview){packed->Data, packed->Size});
    if (input == NULL || !xrtMemDebugFailAfter(0)) {
        (void)xrtReaderDestroy(input);
        return 0;
    }
    reader = xrtTarReaderCreate(input, true, NULL);
    ok = reader == NULL && xrtMemDebugFailTriggered() &&
        xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
    xrtMemDebugFailClear();
    /* 只清理 Open* 自己创建的输入，不改变低层 Create 的所有权契约。 */
    if (reader == NULL) {
        ok &= xrtReaderSize(input, &size) && size == packed->Size;
        ok &= xrtReaderDestroy(input);
    } else xrtTarReaderDestroy(reader);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return ok && before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes;
}

/* 扩展记录不是文件条目；其后 EOF/截断不能沿用扩展解析成功标志。 */
static int extension_tail(const xbuffer* packed, bool truncate, cstr path)
{
    uint64 size;
    size_t span;
    const unsigned char zeros[1024] = {0};
    xbuffer* input = xrtBufferCreate();
    xrttarreader* reader = NULL;
    xrttarentry entry, untouched;
    bool next;
    int ok = 0;
    if (input == NULL ||
        !xrtUIntParse((xstrview){(cstr)packed->Data + 124, 11}, 8, 0, &size)) goto done;
    span = 512u + ((size_t)size + 511u) / 512u * 512u;
    if (span > packed->Size || !xrtBufferAppend(input, (xbytesview){packed->Data, span}) ||
        !xrtBufferAppend(input, (xbytesview){zeros, truncate ? 1u : sizeof(zeros)})) goto done;
    reader = xrtTarReaderOpenMemory(input->Data, input->Size, NULL);
    if (reader == NULL) goto done;
    memset(&entry, 0xa5, sizeof(entry));
    untouched = entry;
    next = xrtTarReaderNext(reader, &entry);
    ok = !next && memcmp(&entry, &untouched, sizeof(entry)) == 0 &&
        xrtTarReaderEOF(reader) == !truncate && xrtTarReaderError(reader)->iCode ==
        (truncate ? XRT_TAR_ERROR_FORMAT : XRT_TAR_OK) && !xrtTarReaderNext(reader, &entry);
    if (!truncate) {
        xrtSetErrorInfo(XERR_MEMORY, "tar.test.stale", 1, "old allocation failure");
        ok &= !xrtTarReaderNext(reader, &entry) && xrtGetError() == NULL;
    }
    if (path != NULL) ok &= xrtFileWriteAll(path, (xbytesview){input->Data, input->Size});
    if (!ok) fprintf(stderr, "extension tail: truncate=%d next=%d eof=%d code=%d\n",
        (int)truncate, (int)next, (int)xrtTarReaderEOF(reader), xrtTarReaderError(reader)->iCode);
done:
    xrtTarReaderDestroy(reader);
    xrtBufferDestroy(input);
    xrtClearError();
    return ok;
}

int main(int argc, char** argv)
{
    char prefix[146], long_name[201];
    xbuffer* fixtures[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    const char* names[6];
    const char* files[6] = {"plain", "prefix", "pax", "global", "gnu_name", "gnu_link"};
    unsigned i;
    int ok = 1;
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 1;
    xrtMemDebugFailClear();
    memset(prefix, 'p', 130u);
    memcpy(prefix + 130u, "/prefix.txt", 12u);
    memset(long_name, 'n', 200u);
    long_name[200] = 0;
    names[0] = "plain.txt";
    names[1] = prefix;
    names[2] = long_name;
    names[3] = long_name;
    names[4] = long_name;
    names[5] = names[0];
    for (i = 0; i < 6u; ++i) {
        uint64 uid = i == 2u || i == 3u ? UINT64_MAX : 0;
        cstr link = i == 5u ? long_name : "link-target";
        str path = NULL;
        if (i < 3u) fixtures[i] = archive(names[i], uid);
        else if (i == 3u) {
            fixtures[i] = archive(names[i], uid);
            if (fixtures[i] != NULL) {
                fixtures[i]->Data[156] = 'g';
                checksum(fixtures[i]->Data);
            }
        } else fixtures[i] = gnu_extension(fixtures[0], i == 4u ? 'L' : 'K', long_name);
        if (fixtures[i] == NULL) { ok = 0; goto done; }
        /* 先预热正确路径，分配快照不把运行库的首次初始化当作单次泄漏。 */
        {
            xrttarentry entry;
            xrttarreader* reader = xrtTarReaderOpenMemory(fixtures[i]->Data, fixtures[i]->Size, NULL);
            if (reader == NULL || !xrtTarReaderNext(reader, &entry) ||
                !metadata(&entry, names[i], link, uid)) {
                xrtTarReaderDestroy(reader);
                ok = 0;
                goto done;
            }
            xrtTarReaderDestroy(reader);
            xrtClearError();
        }
        ok &= reader_faults(fixtures[i], names[i], link, uid, 1, NULL);
        if (argc == 2) {
            path = xrtFormat("%s/%s.tar", argv[1], files[i]);
            if (path == NULL || !xrtFileWriteAll(path,
                (xbytesview){fixtures[i]->Data, fixtures[i]->Size})) {
                xrtFree(path);
                ok = 0;
                goto done;
            }
            if (i == 0u) ok &= reader_faults(fixtures[i], names[i], link, uid, 3, path);
            xrtFree(path);
        }
        if (i >= 2u) {
            int truncated;
            ok &= invalid_extension(fixtures[i]);
            for (truncated = 0; truncated <= 1; ++truncated) {
                path = argc == 2 ? xrtFormat("%s/%s_tail_%d.tar", argv[1], files[i], truncated) : NULL;
                if (argc == 2 && path == NULL) { ok = 0; goto done; }
                ok &= extension_tail(fixtures[i], truncated != 0, path);
                xrtFree(path);
            }
        }
    }
    ok &= reader_faults(fixtures[0], names[0], "link-target", 0, 0, NULL);
    ok &= reader_faults(fixtures[0], names[0], "link-target", 0, 2, NULL);
    ok &= create_ownership(fixtures[0]);
done:
    for (i = 0; i < 6u; ++i) xrtBufferDestroy(fixtures[i]);
    if (ok) puts("TAR reader allocation boundaries: PASS");
    return ok ? 0 : 1;
}
