#define XRT_MODULE_DIR
#define XRT_MODULE_FILE_WHOLE
#include "../include/xrt_murmur3.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct progress_state { int calls, mode; } progress_state;
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* state = user;
    assert(p->iInputBytes == 3 && p->iTotalInputBytes == 3 && p->iOutputBytes == 0);
    ++state->calls;
    if (state->mode == 2) xrtSetErrorInfo(XERR_VALUE, "example", 0, "callback marker");
    return state->mode == 0;
}

int main(void)
{
    const char* path = "build/test/murmur3-example/中文.txt";
    xreader* reader;
    xrtresourcelimits limits;
    progress_state state = {0, 0};
    uint8 digest[16];
    uint64 position;
    unsigned i;
    assert(xrtDirCreateAll("build/test/murmur3-example"));
    assert(xrtFileWriteAll(path, XRT_BYTES_LITERAL("abc")));
    reader = xrtReaderOpen(path);
    assert(reader && xrtMurmur3File128(reader, NULL, 42, digest, progress, &state, NULL));
    printf("file128=");
    for (i = 0; i < 16; ++i) printf("%02x", digest[i]);
    printf("\ncallbacks=%d\n", state.calls);

    // Reader 是借用对象，每次调用从当前游标继续；这里显式回到开头。
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    printf("file32=%u\n", xrtMurmur3File32(reader, NULL, 42, NULL, NULL, NULL));
    assert(xrtGetError() == NULL);
    state.mode = 1;
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    assert(!xrtMurmur3File128(reader, NULL, 42, digest, progress, &state, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED);
    puts("cancelled=true");
    state.mode = 2;
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    assert(!xrtMurmur3File128(reader, NULL, 42, digest, progress, &state, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && strcmp(xrtErrorMessage(xrtGetError()), "callback marker") == 0);
    puts("callback-error=true");
    xrtResourceLimitsInit(&limits);
    limits.iMaxInputBytes = 2;
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    assert(!xrtMurmur3File128(reader, &limits, 42, digest, NULL, NULL, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    puts("budget=true");
    xrtClearError();
    assert(xrtReaderDestroy(reader) && xrtFileDelete(path));
    return 0;
}
