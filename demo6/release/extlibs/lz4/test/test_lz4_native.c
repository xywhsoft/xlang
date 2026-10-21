#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "xruntime.h"
#include "../include/xrt_lz4.h"
#include "../include/lz4_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { unsigned int Mode, Calls; } callback_state;

static bool callback(ptr environment, const xrtcallframe* frame, xrtcallresult* result)
{
    callback_state* state = environment;
    xvalue* returned;
    bool ok;
    uint64 value;
    size_t i;
    assert(frame->ArgumentCount == 3);
    for (i = 0; i < 3; ++i) assert(xrtValueGetUInt(frame->Arguments[i], &value));
    ++state->Calls;
    if (state->Mode == 2) {
        xrtSetErrorInfo(XERR_ARGUMENT, "test.lz4.callback", 991, "callback identity");
        return false;
    }
    if (state->Mode == 3) return true; /* 合法 callable，错误的进度返回合同。 */
    returned = state->Mode == 4 ? xrtValueInt(7) : xrtValueBool(state->Mode != 1);
    if (!returned) return false;
    ok = xrtCallResultPushTake(result, &returned);
    xrtValueRelease(returned);
    return ok;
}

static void clean_snapshot(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount);
}

int main(int argc, char** argv)
{
    static const uint8 content[] = "native progress";
    callback_state state = {0};
    xrtcallable* callable;
    xvalue* value;
    xbuffer* output;
    xmemdebugsnapshot before;
    char source[1024], target[1024];
    unsigned int mode, i, bad = 0, good = 0;
    assert(argc == 2 && strlen(argv[1]) < 900);
    assert(xrtMemDebugEnable(true));
    assert(xlz4_available() && strcmp(xlz4_version(), "1.10.0") == 0);
    assert(!xlz4_compressor_create(0, 0, 0) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    assert(strstr(xlz4_last_error(), "acceleration") != NULL);
    assert(!xlz4_compressor_update(NULL, NULL, 0) && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT);
    output = xlz4_compress_str("你好", 6, 1, 0, 0);
    assert(output);
    {
        xbuffer* decoded = xlz4_decompress(output->Data, output->Size, 0, 0, 0);
        assert(decoded && decoded->Size == 6 && memcmp(decoded->Data, "你好", 6) == 0);
        xrtBufferDestroy(decoded);
    }
    xrtBufferDestroy(output);
    assert(xrtDirCreateAll(argv[1]));
    snprintf(source, sizeof(source), "%s/source.txt", argv[1]);
    snprintf(target, sizeof(target), "%s/target.lz4", argv[1]);
    assert(xrtFileWriteAll(source, (xbytesview){content, sizeof(content) - 1}));
    callable = xrtCallableCreate(NULL, callback, &state, NULL);
    assert(callable);
    value = xrtValueCallableTake(&callable);
    assert(value && !callable);
    for (mode = 0; mode < 5; ++mode) {
        int result;
        state.Mode = mode; state.Calls = 0;
        result = xlz4_compress_file(source, target, 1, false, 0, 0, value);
        assert(state.Calls > 0);
        if (mode == 0) assert(result == 1 && xrtFileDelete(target));
        else {
            assert(result == (mode == 1 ? -1 : -2));
            if (mode == 1) assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
            if (mode == 2) {
                // xcall 补充调用上下文，原始错误必须仍作为 cause 完整保留。
                const xerror* error = xrtGetError();
                assert(xrtErrorKind(error) == XERR_ARGUMENT);
                while (xrtErrorCause(error)) error = xrtErrorCause(error);
                assert(xrtErrorCode(error) == 991);
                assert(strcmp(xrtErrorDomain(error), "test.lz4.callback") == 0);
                assert(strcmp(xrtErrorMessage(error), "callback identity") == 0);
            }
            if (mode == 3) assert(xrtErrorKind(xrtGetError()) == XERR_STATE);
            if (mode == 4) assert(xrtErrorKind(xrtGetError()) == XERR_TYPE);
            assert(!xrtPathExists(target));
        }
    }
    state.Mode = 0;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    for (i = 0; i < 128 && good < 2; ++i) {
        int result;
        bool failed;
        assert(xrtMemDebugFailAfter(i));
        result = xlz4_compress_file(source, target, 1, false, 0, 0, value);
        failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (failed) {
            assert(result != 1 && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++bad;
        } else { assert(result == 1); ++good; }
        clean_snapshot(&before);
        if (result == 1) assert(xrtFileDelete(target));
        else assert(!xrtPathExists(target));
    }
    assert(good == 2 && bad > 0);
    xrtValueRelease(value);
    assert(xrtFileDelete(source) && xrtDirRemove(argv[1]));
    printf("lz4 native callback: contract, identity, unsigned ABI and %u allocation failures passed\n", bad);
    return 0;
}
