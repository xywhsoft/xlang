/* 同一原错误跨 Reader/回调保持身份；C 错误快照也必须保留原后端码。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_blake3.h"
#include "../blake3_portable.c"
#include "../xrt_blake3.c"
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int Mode, Reads, Progress, Closed, Code;
    xerrkind Kind;
    bool Late;
    const xerror* Cause;
} probe;

static unsigned failures, cases;

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "blake3 reader cause: %s\n", message); }
}

static bool report_error(probe* state)
{
    xrtSetErrorInfo(state->Kind, "test.blake3.cause", state->Code, "first reader/callback cause");
    state->Cause = xrtGetError();
    check(state->Cause != NULL, "cannot publish fixture error");
    // 首因已经建立：库只应复制到栈上快照，不能再申请诊断对象。
    check(xrtMemDebugFailAfter(0), "cannot guard error preservation");
    return false;
}

static bool read_source(ptr context, ptr data, size_t requested, size_t* count)
{
    probe* state = context;
    ++state->Reads;
    *count = 0;
    if (state->Mode == 0 && (!state->Late || state->Reads > 1)) return report_error(state);
    if (state->Reads == 1 && requested) { *(uint8*)data = 'x'; *count = 1; }
    return true;
}

static bool close_source(ptr context)
{
    ++((probe*)context)->Closed;
    return true;
}

static bool report_progress(const xrtprogress* progress, ptr context)
{
    probe* state = context;
    ++state->Progress;
    if (state->Mode == 1 && (!state->Late || (progress->iFlags & XRT_PROGRESS_FINAL)))
        return report_error(state);
    return true;
}

static xreader* create_source(probe* state)
{
    xreaderops operations = {read_source, NULL, NULL, NULL, close_source};
    return xrtReaderCreate(&operations, state);
}

static void run_case(int mode, bool late, xerrkind kind, int code, bool snapshot)
{
    uint8 output[65], expected[65], sentinel[65];
    probe state = {0};
    xrtblake3error error;
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    state.Mode = mode; state.Late = late; state.Kind = kind; state.Code = code;
    xreader* reader = create_source(&state);
    check(reader != NULL, "cannot create fixture reader");
    memset(output, 0xa5, sizeof(output));
    memcpy(sentinel, output, sizeof(output));
    bool ok = xrtBlake3File(reader, NULL, output, sizeof(output), report_progress, &state,
        snapshot ? &error : NULL);
    bool allocated = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    check(!ok && !allocated && state.Closed == 0 && memcmp(output, sentinel, sizeof(output)) == 0,
        "failure allocated a new error, modified output or closed a borrowed reader");
    check(xrtGetError() == state.Cause && xrtErrorKind(state.Cause) == kind &&
        xrtErrorCode(state.Cause) == code && strcmp(xrtErrorDomain(state.Cause), "test.blake3.cause") == 0 &&
        strcmp(xrtErrorMessage(state.Cause), "first reader/callback cause") == 0,
        "original error identity or diagnostic differs");
    if (snapshot) {
        int category = mode == 1 ? XRT_BLAKE3_ERROR_CANCELLED :
            (kind == XERR_MEMORY ? XRT_BLAKE3_ERROR_MEMORY : XRT_BLAKE3_ERROR_IO);
        uint64 consumed = mode == 0 && !late ? 0 : 1;
        if (error.iCode != category || error.iBackendCode != code || error.iInputOffset != consumed ||
            error.iOutputOffset != 0 || strcmp(error.sMessage, "first reader/callback cause")) {
            ++failures;
            fprintf(stderr, "blake3 reader snapshot: mode=%d late=%d kind=%d expected=%d actual=%d\n",
                mode, late, (int)kind, code, error.iBackendCode);
        }
    }
    xrtClearError();
    check(xrtReaderDestroy(reader) && state.Closed == 1, "caller did not close reader exactly once");
    // 清除故障，重建 Reader 后立即重复成功操作；不是重用已消费的游标。
    state = (probe){0};
    state.Mode = 2;
    reader = create_source(&state);
    check(reader && xrtBlake3File(reader, NULL, output, sizeof(output), report_progress, &state, &error) &&
        error.iCode == XRT_BLAKE3_OK && !xrtGetError() &&
        xrtBlake3Hash("x", 1, expected, sizeof(expected), NULL) &&
        memcmp(output, expected, sizeof(output)) == 0 && state.Closed == 0,
        "same hash operation did not recover");
    check(xrtReaderDestroy(reader) && state.Closed == 1, "recovery reader ownership differs");
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount, "failure or recovery leaked memory");
    ++cases;
}

int main(void)
{
    const xerrkind kinds[] = {XERR_IO, XERR_MEMORY, XERR_VALUE};
    const int codes[] = {0, 42, INT_MIN, INT_MAX};
    for (int mode = 0; mode < 2; ++mode)
        for (int late = 0; late < 2; ++late)
            for (size_t kind = 0; kind < sizeof(kinds) / sizeof(*kinds); ++kind)
                for (size_t code = 0; code < sizeof(codes) / sizeof(*codes); ++code)
                    for (int snapshot = 0; snapshot < 2; ++snapshot)
                        run_case(mode, late != 0, kinds[kind], codes[code], snapshot != 0);
    if (!failures) printf("blake3 reader cause: %u cases, snapshots, no-allocation and recovery PASS\n", cases);
    return failures ? 1 : 0;
}
