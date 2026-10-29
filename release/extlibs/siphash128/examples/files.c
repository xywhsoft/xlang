#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_FILE_WHOLE
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#include "../include/xrt_siphash.h"
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
    const char* path = "build/test/siphash-example/中文.txt";
    const uint8 key[] = "0123456789abcdef";
    xreader* reader;
    xrtresourcelimits limits;
    progress_state state = {0, 0};
    uint8 digest[16];
    uint64 position;
    unsigned i;
    assert(xrtDirCreateAll("build/test/siphash-example"));
    assert(xrtFileWriteAll(path, XRT_BYTES_LITERAL("abc")));
    reader = xrtReaderOpen(path);
    assert(reader && xrtSipHashFile128(reader, NULL, key, digest, progress, &state, NULL));
    printf("file128=");
    for (i = 0; i < 16; ++i) printf("%02x", digest[i]);
    printf("\ncallbacks=%d\nbytes=16\n", state.calls);

    // Reader 由调用方持有，库不关闭它；每次计算从当前游标开始。
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    printf("file64=%llu\n", (unsigned long long)xrtSipHashFile64(reader, NULL, key, NULL, NULL, NULL));
    assert(!xrtGetError());
    state.mode = 1;
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    assert(!xrtSipHashFile128(reader, NULL, key, digest, progress, &state, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_CANCELLED); puts("cancelled=true");
    state.mode = 2;
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    assert(!xrtSipHashFile128(reader, NULL, key, digest, progress, &state, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_VALUE && !strcmp(xrtErrorMessage(xrtGetError()), "callback marker"));
    puts("callback-error=true");
    xrtResourceLimitsInit(&limits); limits.iMaxInputBytes = 2;
    assert(xrtReaderSeek(reader, 0, XSEEK_START, &position));
    assert(!xrtSipHashFile128(reader, &limits, key, digest, NULL, NULL, NULL));
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE); puts("budget=true");
    xrtClearError(); assert(xrtReaderDestroy(reader) && xrtFileDelete(path));
    return 0;
}
