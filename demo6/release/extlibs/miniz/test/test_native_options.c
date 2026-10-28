/* C 桥接入口与导出类构造器必须和工厂遵守同一套 0..10 级别约束。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_PATH
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#include "../include/xrt_miniz.h"
#include "../include/miniz_native.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int check(bool condition, const char* message)
{
    if (!condition) fprintf(stderr, "miniz native options: %s\n", message);
    return condition ? 1 : 0;
}

static int check_levels(void)
{
    static const int invalid[] = {-1, -7, INT_MIN, 11, INT_MAX};
    int ok = 1;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        for (unsigned entry = 0; entry < 3; ++entry) {
            xmemdebugsnapshot before, after;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            void* result = entry == 0 ? xminiz_compress((void*)"abc", 3, invalid[i], 1024, 1024) :
                entry == 1 ? xminiz_compress_str("abc", 3, invalid[i], 1024, 1024) :
                xminiz_compressor_create(invalid[i], 1024, 1024);
            ok &= check(!result && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
                "a native entry accepted an invalid level or reported the wrong error");
            if (entry == 2) xminiz_compressor_destroy(result);
            else xrtBufferDestroy((xbuffer*)result);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            ok &= check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
                "rejecting a native option leaked an allocation");
        }
    }
    for (int level = 0; level <= 10; ++level) {
        xbuffer* packed = (xbuffer*)xminiz_compress_str("abc", 3, level, 1024, 1024);
        xbuffer* plain = packed ? (xbuffer*)xminiz_decompress(packed->Data, packed->Size, 1024, 1024, 1000) : NULL;
        ok &= check(plain && !xrtGetError() && plain->Size == 3 && memcmp(plain->Data, "abc", 3) == 0,
            "an explicitly supported level did not round trip through the native bridge");
        xrtBufferDestroy(plain);
        xrtBufferDestroy(packed);
    }
    return ok;
}

static int check_file_levels(void)
{
    static const int invalid[] = {-1, -7, INT_MIN, 11, INT_MAX};
    str directory = xrtDirTemp(NULL, "xlang-miniz-native-", "");
    str source = directory ? xrtPathJoin(directory, "source.bin") : NULL;
    str target = directory ? xrtPathJoin(directory, "target.zz") : NULL;
    xwriter* writer = source ? xrtWriterOpen(source) : NULL;
    int ok = check(writer && target && xrtWriterWriteFull(writer, "abc", 3, NULL), "cannot prepare file input");
    if (!xrtWriterDestroy(writer)) ok = 0;
    if (!ok) goto done;
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        int result = xminiz_compress_file(source, target, invalid[i], true, 1024, 1024, NULL);
        ok &= check(result == 0 && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
            "the native file entry accepted an invalid level or reported the wrong error");
        ok &= check(!xrtFileExists(target), "an invalid level published a destination file");
        if (xrtFileExists(target) && !xrtFileDelete(target)) ok = 0;
        xrtClearError();
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
    int ok = check_levels();
    ok &= check_file_levels();
    if (ok) puts("miniz native option validation: PASS");
    return ok ? 0 : 1;
}
