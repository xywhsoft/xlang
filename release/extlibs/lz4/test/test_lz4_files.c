#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_lz4.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8 marker[] = "keep existing destination";
static uint8 data[131073];

static void same_file(const char* path, xbytesview expected)
{
    size_t size = 0;
    bytes value = xrtFileReadAll(path, &size);
    assert(value && size == expected.Size);
    assert(size == 0 || memcmp(value, expected.Data, size) == 0);
    xrtFree(value);
}

static void no_temporary(const char* directory)
{
    xdirentry entry;
    xdirnext next;
    xdir dir = xrtDirOpen(directory, 0);
    assert(dir);
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM) {
        bool temporary = entry.Name.Size >= 9 && memcmp(entry.Name.Data, ".xrt-lz4-", 9) == 0;
        if (temporary)
            fprintf(stderr, "orphan temporary: %s/%.*s\n", directory, (int)entry.Name.Size, entry.Name.Data);
        assert(!temporary);
    }
    assert(next == XDIR_NEXT_END && xrtDirClose(dir));
}

typedef struct {
    uint64 Input, Output, Total;
    unsigned int Calls, Finals, Mode;
    const char* Destination;
} progress_state;

static bool progress(const xrtprogress* value, ptr user)
{
    progress_state* state = user;
    assert(value->iFlags & XRT_PROGRESS_TOTAL_KNOWN);
    assert(value->iTotalInputBytes == state->Total);
    assert(value->iInputBytes >= state->Input && value->iOutputBytes >= state->Output);
    state->Input = value->iInputBytes;
    state->Output = value->iOutputBytes;
    state->Calls++;
    if (value->iFlags & XRT_PROGRESS_FINAL) state->Finals++;
    if (state->Mode == 1 || (state->Mode == 2 && state->Finals)) return false;
    if (state->Mode == 3) {
        xrtSetErrorInfo(XERR_ARGUMENT, "test.lz4", 902, "file callback marker");
        return false;
    }
    // 模拟另一个写入者在最终提交前创建目标，不应被 overwrite=false 覆盖。
    if (state->Mode == 4 && state->Finals)
        assert(xrtFileWriteAll(state->Destination, (xbytesview){marker, sizeof(marker) - 1}));
    return true;
}

static void allocation_failures(const char* source, const char* target,
    const char* directory, bool decode, bool overwrite)
{
    unsigned int i, rejected = 0, successes = 0;
    for (i = 0; i < 128 && successes < 2; ++i) {
        xmemdebugsnapshot before, after;
        bool ok, injected;
        if (overwrite) assert(xrtFileWriteAll(target, (xbytesview){marker, sizeof(marker) - 1}));
        else assert(!xrtPathExists(target));
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(i));
        ok = decode ? xrtLz4DecompressFile(source, target, NULL, overwrite, NULL, NULL) :
            xrtLz4CompressFile(source, target, NULL, overwrite, NULL, NULL);
        injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
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
    printf("lz4 file allocation: %s overwrite=%d rejected=%u\n", decode ? "decode" : "encode", (int)overwrite, rejected);
}

int main(int argc, char** argv)
{
    const char* directory = argc > 1 ? argv[1] : NULL;
    char source[1024], packed[1024], target[1024], absent[1024];
    progress_state state = {0};
    xrtlz4options options;
    xmemdebugsnapshot before, after;
    size_t packedSize, i;
    bytes packedBytes;
    if (!directory || !directory[0]) {
        fputs("usage: test_lz4_files <exclusive-test-directory>\n", stderr);
        return 2;
    }
    assert(strlen(directory) < 900);
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));
    assert(xrtDirCreateAll(directory));
    snprintf(source, sizeof(source), "%s/输入.bin", directory);
    snprintf(packed, sizeof(packed), "%s/packed.lz4", directory);
    snprintf(target, sizeof(target), "%s/输出.bin", directory);
    snprintf(absent, sizeof(absent), "%s/new.lz4", directory);
    assert(!xrtPathExists(absent));
    for (i = 0; i < sizeof(data); ++i) data[i] = (uint8)(i * 31u + i / 17u);
    assert(xrtFileWriteAll(source, (xbytesview){data, sizeof(data)}));
    state.Total = sizeof(data);
    assert(xrtLz4CompressFile(source, packed, NULL, true, progress, &state));
    assert(state.Calls > 1 && state.Finals == 1 && state.Input == sizeof(data));
    packedBytes = xrtFileReadAll(packed, &packedSize);
    assert(packedBytes && packedSize == state.Output);
    xrtFree(packedBytes);
    memset(&state, 0, sizeof(state)); state.Total = packedSize;
    assert(xrtLz4DecompressFile(packed, target, NULL, true, progress, &state));
    assert(state.Finals == 1 && state.Output == sizeof(data));
    same_file(target, (xbytesview){data, sizeof(data)});

    // 源和目标同路径仅在所有 reader/writer 关闭后替换。
    assert(xrtLz4CompressFile(source, source, NULL, true, NULL, NULL));
    assert(xrtLz4DecompressFile(source, source, NULL, true, NULL, NULL));
    same_file(source, (xbytesview){data, sizeof(data)});
    for (i = 1; i <= 3; ++i) {
        bool decode;
        for (decode = false; ; decode = true) {
            assert(xrtFileWriteAll(target, (xbytesview){marker, sizeof(marker) - 1}));
            memset(&state, 0, sizeof(state));
            state.Total = decode ? packedSize : sizeof(data); state.Mode = (unsigned int)i;
            assert(!(decode ? xrtLz4DecompressFile(packed, target, NULL, true, progress, &state) :
                xrtLz4CompressFile(source, target, NULL, true, progress, &state)));
            if (i < 3) assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
            else assert(xrtErrorCode(xrtGetError()) == 902 &&
                strcmp(xrtErrorMessage(xrtGetError()), "file callback marker") == 0);
            if (i == 2) assert(state.Finals == 1);
            same_file(target, (xbytesview){marker, sizeof(marker) - 1});
            no_temporary(directory);
            if (decode) break;
        }
    }
    memset(&state, 0, sizeof(state)); state.Total = sizeof(data); state.Mode = 1;
    assert(!xrtLz4CompressFile(source, absent, NULL, false, progress, &state));
    assert(!xrtPathExists(absent));
    memset(&state, 0, sizeof(state)); state.Total = sizeof(data); state.Mode = 4; state.Destination = absent;
    assert(!xrtLz4CompressFile(source, absent, NULL, false, progress, &state));
    same_file(absent, (xbytesview){marker, sizeof(marker) - 1});
    assert(xrtFileDelete(absent));
    assert(!xrtLz4CompressFile(source, target, NULL, false, NULL, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_EXISTS);
    assert(!xrtLz4CompressFile(absent, target, NULL, true, NULL, NULL));
    assert(xrtGetError());
    assert(!xrtLz4CompressFile(NULL, target, NULL, true, NULL, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!xrtLz4DecompressFile(source, target, NULL, true, NULL, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_PROTOCOL);
    same_file(target, (xbytesview){marker, sizeof(marker) - 1});
    xrtLz4OptionsInit(&options); options.tLimits.iMaxOutputBytes = 1;
    assert(!xrtLz4DecompressFile(packed, target, &options, true, NULL, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    same_file(target, (xbytesview){marker, sizeof(marker) - 1});

    xrtClearError();
    xrtMemDebugSnapshot(&before);
    allocation_failures(source, target, directory, false, true);
    allocation_failures(packed, target, directory, true, true);
    allocation_failures(source, absent, directory, false, false);
    allocation_failures(packed, absent, directory, true, false);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    no_temporary(directory);
    assert(xrtFileDelete(source) && xrtFileDelete(packed) && xrtFileDelete(target));
    assert(xrtDirRemove(directory));
    puts("lz4 C files: transactions, callbacks, UTF-8, same path and allocation passed");
    return 0;
}
