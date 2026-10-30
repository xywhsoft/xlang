#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_TEMP
#endif
#ifndef XRT_MODULE_CODEC_HEX
#define XRT_MODULE_CODEC_HEX
#endif
#include "../include/xrt_blake2.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* xl run单文件；正式GCC/包构建仍分别编译。 */
#include "../third_party/blake2.c"
#include "../xrt_blake2.c"
#endif

typedef struct {
    unsigned int Calls;
    bool Cancel;
} progress_state;

static bool progress(const xrtprogress* frame, ptr context)
{
    progress_state* state = context;
    ++state->Calls;
    return !state->Cancel && frame->iInputBytes == 9 &&
        frame->iTotalInputBytes == 9 && frame->iOutputBytes == 0;
}

int main(void)
{
    const uint8 data[] = {'a', 0, 'b', 0xe4, 0xbd, 0xa0, 0xe5, 0xa5, 0xbd};
    uint8 digest[16], unchanged[16], actual[9], extra;
    char saved[33];
    size_t count, written;
    xrtblake2error error;
    xrtresourcelimits limits;
    progress_state state = {0};
    xreader* reader = NULL;
    str input = NULL;
    xfile file = xrtFileTemp(NULL, "xlang-blake2-\xe4\xb8\xad\xe6\x96\x87-", ".bin", &input);
    int result = 1;
    if (!file || !xrtWriteFull(file, data, sizeof(data), &count) || count != sizeof(data)) goto done;
    bool closed = xrtClose(file);
    file = NULL;
    if (!closed) goto done;
    reader = xrtReaderOpen(input);
    if (!reader || !xrtBlake2bFile(reader, NULL, sizeof(digest), digest,
        progress, &state, &error) || state.Calls != 2 ||
        !xrtHexEncode(digest, sizeof(digest), saved, sizeof(saved), &written, 0) ||
        written != 32 || strcmp(saved, "7f9f144228b44255e32dafb65bbc72c4")) goto done;
    closed = xrtReaderDestroy(reader);
    reader = NULL;
    if (!closed) goto done;
    printf("b2b/16: %s\nprogress: 9/9 calls=2\n", saved);

    /* 取消与超限都不能发布部分摘要；每次重新打开，调用方负责检查关闭。 */
    memset(unchanged, 0x7c, sizeof(unchanged));
    memcpy(digest, unchanged, sizeof(digest));
    reader = xrtReaderOpen(input);
    state = (progress_state){0, true};
    if (!reader || xrtBlake2sFile(reader, NULL, sizeof(digest), digest, progress, &state, &error) ||
        error.iCode != XRT_BLAKE2_ERROR_CANCELLED || state.Calls != 1 ||
        memcmp(digest, unchanged, sizeof(digest))) goto done;
    closed = xrtReaderDestroy(reader);
    reader = NULL;
    if (!closed) goto done;
    puts("cancel: rejected");
    reader = xrtReaderOpen(input);
    xrtBlake2LimitsInit(&limits);
    limits.iMaxInputBytes = 1;
    if (!reader || xrtBlake2bFile(reader, &limits, sizeof(digest), digest, NULL, NULL, &error) ||
        error.iCode != XRT_BLAKE2_ERROR_LIMIT || memcmp(digest, unchanged, sizeof(digest))) goto done;
    closed = xrtReaderDestroy(reader);
    reader = NULL;
    if (!closed) goto done;
    puts("limit: rejected");
    reader = xrtReaderOpen(input);
    if (!reader || !xrtReaderRead(reader, actual, sizeof(actual), &count) || count != sizeof(data) ||
        memcmp(actual, data, sizeof(data)) || !xrtReaderRead(reader, &extra, 1, &count) || count != 0) goto done;
    closed = xrtReaderDestroy(reader);
    reader = NULL;
    if (!closed) goto done;
    puts("source: unchanged");
    result = 0;
done:
    if (file && !xrtClose(file)) result = 1;
    if (reader && !xrtReaderDestroy(reader)) result = 1;
    if (input && !xrtFileDelete(input)) result = 1;
    xrtFree(input);
    if (!result) printf("snapshot: %s\n", saved);
    else fputs("BLAKE2 file example failed\n", stderr);
    return result;
}
