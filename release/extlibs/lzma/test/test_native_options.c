/* 所有压缩入口统一检查级别；失败不得交付输出、泄漏或覆盖旧文件。 */
#define XRT_MODULE_MEMORY_DEBUG
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_PATH
#include "../include/lzma_native.h"
#include "../include/xrt_lzma.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static const int invalid[] = {-1, -7, INT_MIN, 10, INT_MAX};

static int check(bool condition, const char* message)
{
    if (!condition) fprintf(stderr, "lzma native options: %s\n", message);
    return condition ? 1 : 0;
}

static int check_memory_levels(void)
{
    int ok = 1;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        for (unsigned entry = 0; entry < 3; ++entry) {
            xmemdebugsnapshot before, after;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            void* result = entry == 0 ? xlzma_compress((void*)"abc", 3, invalid[i], 1024, 1024) :
                entry == 1 ? xlzma_compress_str("abc", 3, invalid[i], 1024, 1024) :
                xlzma_compressor_create(invalid[i], 1024, 1024);
            ok &= check(!result && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
                "a native entry accepted an invalid level or reported the wrong error");
            if (entry == 2) xlzma_compressor_destroy(result);
            else xrtBufferDestroy((xbuffer*)result);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            ok &= check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
                "invalid options leaked an allocation");
        }
    }
    for (int level = 0; level <= 9; ++level) {
        xbuffer* packed = (xbuffer*)xlzma_compress_str("abc", 3, level, 1024, 1024);
        xbuffer* plain = packed ? (xbuffer*)xlzma_decompress(packed->Data, packed->Size, 1024, 1024, 1000) : NULL;
        ok &= check(plain && !xrtGetError() && plain->Size == 3 && memcmp(plain->Data, "abc", 3) == 0,
            "a supported native level failed to round trip");
        xrtBufferDestroy(plain);
        xrtBufferDestroy(packed);
    }
    return ok;
}

static int write_text(const char* path, const char* text)
{
    xwriter* writer = xrtWriterOpen(path);
    int ok = writer && xrtWriterWriteFull(writer, text, strlen(text), NULL);
    if (!xrtWriterDestroy(writer)) ok = 0;
    return ok;
}

static int check_file_levels(void)
{
    str directory = xrtDirTemp(NULL, "xlang-lzma-native-", "");
    str source = directory ? xrtPathJoin(directory, "source.bin") : NULL;
    str target = directory ? xrtPathJoin(directory, "target.xz") : NULL;
    int ok = check(source && target && write_text(source, "abc"), "cannot prepare input file");
    if (!ok) goto done;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        for (unsigned exists = 0; exists < 2; ++exists) {
            if (exists && !write_text(target, "keep")) { ok = 0; goto done; }
            int result = xlzma_compress_file(source, target, invalid[i], true, 1024, 1024, NULL);
            ok &= check(result == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
                "the file entry accepted an invalid level or reported the wrong error");
            if (exists) {
                char text[5] = {0};
                xreader* reader = xrtReaderOpen(target);
                size_t read = 0;
                ok &= check(reader && xrtReaderRead(reader, text, sizeof(text), &read) &&
                    read == 4 && memcmp(text, "keep", 4) == 0, "failure changed the old destination");
                if (!xrtReaderDestroy(reader)) ok = 0;
            } else {
                ok &= check(!xrtFileExists(target), "failure published a destination file");
            }
            if (xrtFileExists(target) && !xrtFileDelete(target)) ok = 0;
            xrtClearError();
        }
    }
done:
    if (target && xrtFileExists(target) && !xrtFileDelete(target)) ok = 0;
    if (source && xrtFileExists(source) && !xrtFileDelete(source)) ok = 0;
    if (directory && !xrtDirRemove(directory)) ok = 0;
    xrtFree(target);
    xrtFree(source);
    xrtFree(directory);
    xrtClearError();
    return ok;
}

int main(void)
{
    int ok = check_memory_levels();
    ok &= check_file_levels();
    if (ok) puts("lzma native option validation: PASS");
    return ok ? 0 : 1;
}
