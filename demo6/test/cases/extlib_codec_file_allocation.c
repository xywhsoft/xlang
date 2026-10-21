/* 两个文件绑定使用同一组故障断言，避免只修复单个压缩库。 */
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_MEMORY_DEBUG
#if defined(TEST_CODEC_GZIP)
    #include "../../release/extlibs/gzip/include/gzip_native.h"
    #define CODEC_NAME "gzip"
#elif defined(TEST_CODEC_BROTLI)
    #include "../../release/extlibs/brotli/include/brotli_native.h"
    #define CODEC_NAME "brotli"
#else
    #error Select TEST_CODEC_GZIP or TEST_CODEC_BROTLI
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8 marker[] = "keep destination";
static uint8 data[8193];

static bool transform(const char* source, const char* target, bool decode,
    bool overwrite, unsigned long long maxOutput)
{
#if defined(TEST_CODEC_GZIP)
    return (decode ? xgzip_decompress_file(source, target, overwrite, 1048576, maxOutput, 1000, NULL) :
        xgzip_compress_file(source, target, 1, 0, overwrite, 1048576, maxOutput, NULL)) == 1;
#else
    return (decode ? xbrotli_decompress_file(source, target, overwrite, 1048576, maxOutput, 1000, NULL) :
        xbrotli_compress_file(source, target, 1, 16, 0, overwrite, 1048576, maxOutput, NULL)) == 1;
#endif
}

static void same_file(const char* path, xbytesview expected)
{
    size_t size = 0;
    bytes value = xrtFileReadAll(path, &size);
    assert(value && size == expected.Size && memcmp(value, expected.Data, size) == 0);
    xrtFree(value);
}

static void no_temporary(const char* directory)
{
    xdirentry entry;
    xdirnext next;
    xdir dir = xrtDirOpen(directory, 0);
    assert(dir);
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM) {
        bool temporary = entry.Name.Size >= 5 && memcmp(entry.Name.Data, ".xrt-", 5) == 0;
        if (temporary)
            fprintf(stderr, "%s orphan temporary: %s/%.*s\n", CODEC_NAME, directory, (int)entry.Name.Size, entry.Name.Data);
        assert(!temporary);
    }
    assert(next == XDIR_NEXT_END && xrtDirClose(dir));
}

static void sweep(const char* source, const char* target, const char* directory, bool decode, bool overwrite)
{
    unsigned int i, rejected = 0, successes = 0;
    for (i = 0; i < 256 && successes < 2; ++i) {
        xmemdebugsnapshot before, after;
        bool ok, injected;
        if (overwrite) assert(xrtFileWriteAll(target, (xbytesview){marker, sizeof(marker) - 1}));
        else assert(!xrtPathExists(target));
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(i));
        ok = transform(source, target, decode, overwrite, 1048576);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            if (ok || !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_MEMORY)
                fprintf(stderr, "%s %s failAfter=%u ok=%d kind=%d message=%s\n", CODEC_NAME,
                    decode ? "decode" : "encode", i, (int)ok, (int)xrtErrorKind(xrtGetError()),
                    xrtErrorMessage(xrtGetError()));
            assert(!ok && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            rejected++;
        } else {
            assert(ok && xrtGetError() == NULL);
            successes++;
        }
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.UnderflowCount == after.UnderflowCount && before.OverflowCount == after.OverflowCount);
        if (!ok && overwrite) same_file(target, (xbytesview){marker, sizeof(marker) - 1});
        else if (!ok) assert(!xrtPathExists(target));
        else if (decode) same_file(target, (xbytesview){data, sizeof(data)});
        no_temporary(directory);
        if (!overwrite && ok) assert(xrtFileDelete(target));
    }
    assert(rejected && successes == 2);
    printf("%s file allocation: %s overwrite=%d rejected=%u\n", CODEC_NAME, decode ? "decode" : "encode", (int)overwrite, rejected);
}

int main(int argc, char** argv)
{
    const char* directory;
    char source[1024], packed[1024], target[1024];
    size_t i;
    assert(argc == 2 && strlen(argv[1]) < 900);
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    directory = argv[1];
    assert(xrtDirCreateAll(directory));
    snprintf(source, sizeof(source), "%s/输入.bin", directory);
    snprintf(packed, sizeof(packed), "%s/packed.bin", directory);
    snprintf(target, sizeof(target), "%s/输出.bin", directory);
    for (i = 0; i < sizeof(data); ++i) data[i] = (uint8)(i * 31u + i / 17u);
    assert(xrtFileWriteAll(source, (xbytesview){data, sizeof(data)}));
    assert(transform(source, packed, false, true, 1048576));
    assert(transform(packed, target, true, true, 1048576));
    same_file(target, (xbytesview){data, sizeof(data)});
    assert(transform(source, source, false, true, 1048576));
    assert(transform(source, source, true, true, 1048576));
    same_file(source, (xbytesview){data, sizeof(data)});
    assert(xrtFileWriteAll(target, (xbytesview){marker, sizeof(marker) - 1}));
    assert(!transform(source, target, false, false, 1048576) && xrtErrorKind(xrtGetError()) == XERR_EXISTS);
    same_file(target, (xbytesview){marker, sizeof(marker) - 1});
    assert(!transform(packed, target, true, true, 1) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    same_file(target, (xbytesview){marker, sizeof(marker) - 1});
    assert(!transform(source, target, true, true, 1048576) && xrtGetError());
    same_file(target, (xbytesview){marker, sizeof(marker) - 1});
    no_temporary(directory);
    xrtClearError();
    sweep(source, target, directory, false, true);
    sweep(packed, target, directory, true, true);
    assert(xrtFileDelete(target));
    sweep(source, target, directory, false, false);
    sweep(packed, target, directory, true, false);
    assert(xrtFileDelete(source) && xrtFileDelete(packed) && xrtDirRemove(directory));
    printf("%s file transactions: UTF-8, same path, errors, ownership and OOM passed\n", CODEC_NAME);
    return 0;
}
