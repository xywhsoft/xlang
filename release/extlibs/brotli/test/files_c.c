#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_brotli.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8 data[131073];
static const uint8 marker[] = "keep existing destination";
static xrtbrotlioptions options;

static void same_file(cstr path, xbytesview expected)
{
    size_t size;
    bytes value = xrtFileReadAll(path, &size);
    assert(value && size == expected.Size && (size == 0 || memcmp(value, expected.Data, size) == 0));
    xrtFree(value);
}

static void no_temporary(cstr directory)
{
    xdir dir = xrtDirOpen(directory, 0);
    xdirentry entry;
    xdirnext next;
    assert(dir);
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM)
        assert(entry.Name.Size < 12 || memcmp(entry.Name.Data, ".xrt-brotli-", 12) != 0);
    assert(next == XDIR_NEXT_END && xrtDirClose(dir));
}

typedef struct {
    uint64 Total, Input, Output;
    unsigned Calls, Finals, Mode;
    cstr Destination;
} progress_state;

static bool progress(const xrtprogress* value, ptr user)
{
    progress_state* state = user;
    assert(value->iFlags & XRT_PROGRESS_TOTAL_KNOWN);
    assert(value->iTotalInputBytes == state->Total);
    assert(value->iInputBytes >= state->Input && value->iOutputBytes >= state->Output);
    state->Input = value->iInputBytes;
    state->Output = value->iOutputBytes;
    ++state->Calls;
    if (value->iFlags & XRT_PROGRESS_FINAL) ++state->Finals;
    if (state->Mode == 1 || (state->Mode == 2 && state->Finals)) return false;
    if (state->Mode == 3) {
        xrtSetErrorInfo(XERR_ARGUMENT, "test.brotli", 902, "file callback marker");
        return false;
    }
    // 提交前另一写入者创建目标；overwrite=false 不能覆盖它。
    if (state->Mode == 4 && state->Finals)
        assert(xrtFileWriteAll(state->Destination, (xbytesview){marker, sizeof(marker) - 1}));
    return true;
}

static bool transform(cstr source, cstr target, bool decode, bool overwrite,
    xrtprogressproc callback, ptr user)
{
    return decode ? xrtBrotliDecompressFile(source, target, &options, overwrite, callback, user) :
        xrtBrotliCompressFile(source, target, &options, overwrite, callback, user);
}

static void allocation_failures(cstr source, cstr target, cstr directory, bool decode, bool overwrite)
{
    unsigned rejected = 0, successes = 0;
    for (unsigned budget = 0; budget < 256 && successes < 2; ++budget) {
        xmemdebugsnapshot before, after;
        if (overwrite) assert(xrtFileWriteAll(target, (xbytesview){marker, sizeof(marker) - 1}));
        else assert(!xrtPathExists(target));
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(budget));
        bool ok = transform(source, target, decode, overwrite, NULL, NULL);
        bool injected = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (injected) {
            assert(!ok && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++rejected;
        } else { assert(ok && !xrtGetError()); ++successes; }
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
        assert(before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount);
        if (!ok && overwrite) same_file(target, (xbytesview){marker, sizeof(marker) - 1});
        else if (!ok) assert(!xrtPathExists(target));
        else if (decode) same_file(target, (xbytesview){data, sizeof(data)});
        no_temporary(directory);
        if (!overwrite && ok) assert(xrtFileDelete(target));
    }
    assert(rejected && successes == 2);
    printf("brotli C file allocation: decode=%d overwrite=%d rejected=%u\n", (int)decode, (int)overwrite, rejected);
}

int main(void)
{
    str directory = xrtDirTemp(NULL, "xlang-brotli-c-files-", NULL);
    assert(directory);
    str source = xrtPathJoin(directory, "输入.bin");
    str packed = xrtPathJoin(directory, "packed.br");
    str target = xrtPathJoin(directory, "输出.bin");
    str absent = xrtPathJoin(directory, "new.br");
    assert(source && packed && target && absent);
    xrtBrotliOptionsInit(&options);
    options.iQuality = 1;
    options.iWindow = 16;
    for (size_t i = 0; i < sizeof(data); ++i) data[i] = (uint8)(i * 31u + i / 17u);
    assert(xrtFileWriteAll(source, (xbytesview){data, sizeof(data)}));
    progress_state state = {0};
    state.Total = sizeof(data);
    assert(transform(source, packed, false, false, progress, &state));
    assert(state.Calls > 1 && state.Finals == 1 && state.Input == sizeof(data));
    uint64 packed_size = state.Output;
    memset(&state, 0, sizeof(state)); state.Total = packed_size;
    assert(transform(packed, target, true, false, progress, &state));
    assert(state.Finals == 1 && state.Output == sizeof(data));
    same_file(target, (xbytesview){data, sizeof(data)});
    assert(transform(source, source, false, true, NULL, NULL));
    assert(transform(source, source, true, true, NULL, NULL));
    same_file(source, (xbytesview){data, sizeof(data)});

    for (unsigned mode = 1; mode <= 4; ++mode) {
        for (unsigned decode = 0; decode < 2; ++decode) {
            cstr output = mode == 4 ? absent : target;
            if (mode < 4) assert(xrtFileWriteAll(target, (xbytesview){marker, sizeof(marker) - 1}));
            memset(&state, 0, sizeof(state));
            state.Total = decode ? packed_size : sizeof(data);
            state.Mode = mode; state.Destination = output;
            assert(!transform(decode ? packed : source, output, decode != 0, mode < 4, progress, &state));
            if (mode < 3) assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
            else if (mode == 3) assert(xrtErrorCode(xrtGetError()) == 902 &&
                strcmp(xrtErrorDomain(xrtGetError()), "test.brotli") == 0 &&
                strcmp(xrtErrorMessage(xrtGetError()), "file callback marker") == 0);
            if (mode == 2 || mode == 4) assert(state.Finals == 1);
            same_file(output, (xbytesview){marker, sizeof(marker) - 1});
            no_temporary(directory);
            if (mode == 4) assert(xrtFileDelete(output));
        }
    }
    assert(!transform(source, target, false, false, NULL, NULL) && xrtErrorKind(xrtGetError()) == XERR_EXISTS);
    assert(!transform(NULL, target, false, true, NULL, NULL) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(!transform(absent, target, false, true, NULL, NULL) && xrtGetError());
    assert(!transform(source, target, true, true, NULL, NULL) && xrtErrorKind(xrtGetError()) == XERR_PROTOCOL);
    options.tLimits.iMaxOutputBytes = 1;
    assert(!transform(packed, target, true, true, NULL, NULL) && xrtErrorKind(xrtGetError()) == XERR_RANGE);
    options.tLimits.iMaxOutputBytes = 536870912;
    same_file(target, (xbytesview){marker, sizeof(marker) - 1});
    allocation_failures(source, target, directory, false, true);
    allocation_failures(packed, target, directory, true, true);
    allocation_failures(source, absent, directory, false, false);
    allocation_failures(packed, absent, directory, true, false);

    // 空文件仅有最终进度；NULL options 和 NULL 回调都属于有效调用。
    assert(xrtFileWriteAll(source, (xbytesview){NULL, 0}));
    memset(&state, 0, sizeof(state)); state.Mode = 2;
    assert(!xrtBrotliCompressFile(source, target, NULL, true, progress, &state));
    assert(state.Calls == 1 && state.Finals == 1 && xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
    assert(xrtBrotliCompressFile(source, packed, NULL, true, NULL, NULL));
    assert(xrtBrotliDecompressFile(packed, target, NULL, true, NULL, NULL));
    same_file(target, (xbytesview){NULL, 0});
    no_temporary(directory);
    assert(xrtFileDelete(source) && xrtFileDelete(packed) && xrtFileDelete(target));
    assert(xrtDirRemove(directory));
    xrtFree(source); xrtFree(packed); xrtFree(target); xrtFree(absent); xrtFree(directory);
    puts("brotli C files: callbacks, transactions, UTF-8, same path, empty and OOM PASS");
    return 0;
}
