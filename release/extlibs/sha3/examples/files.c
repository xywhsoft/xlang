#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_PATH
#include "../include/xrt_sha3.h"
#include <stdio.h>
#include <string.h>

typedef struct progress_state { int calls, mode; } progress_state;
/* 进度回调同步借用状态；只有 mode 0 继续读取。 */
static bool progress(const xrtprogress* p, ptr user)
{
    progress_state* state = user;
    if (p->iInputBytes != 3 || p->iTotalInputBytes != 3 || p->iOutputBytes != 0) return false;
    ++state->calls;
    if (state->mode == 2) xrtSetErrorInfo(XERR_VALUE, "example", 0, "callback marker");
    return state->mode == 0;
}
int main(void)
{
    str directory = xrtDirTemp(NULL, "xlang-sha3-", NULL);
    str path = NULL;
    xreader* reader = NULL;
    xrtresourcelimits limits;
    progress_state state = {0, 0};
    uint8 digest[32];
    uint64 position;
    unsigned i;
    int status = 1;

    if (directory == NULL) goto done;
    path = xrtPathJoin(directory, "中文.txt");
    if (path == NULL || !xrtFileWriteAll(path, XRT_BYTES_LITERAL("abc"))) goto done;
    reader = xrtReaderOpen(path);
    if (reader == NULL || !xrtSha3File(reader, NULL, 256, digest, 32, progress, &state, NULL)) goto done;
    printf("file=");
    for (i = 0; i < 32; ++i) printf("%02x", digest[i]);
    printf("\ncallbacks=%d\nbytes=%u\n", state.calls, (unsigned)sizeof(digest));

    /* Reader 由调用方持有；每次测试从文件开头重新读取。 */
    state.mode = 1;
    if (!xrtReaderSeek(reader, 0, XSEEK_START, &position) ||
        xrtSha3File(reader, NULL, 256, digest, 32, progress, &state, NULL) ||
        xrtErrorKind(xrtGetError()) != XERR_CANCELLED) goto done;
    puts("cancelled=true");
    state.mode = 2;
    if (!xrtReaderSeek(reader, 0, XSEEK_START, &position) ||
        xrtSha3File(reader, NULL, 256, digest, 32, progress, &state, NULL) ||
        xrtErrorKind(xrtGetError()) != XERR_VALUE ||
        strcmp(xrtErrorMessage(xrtGetError()), "callback marker") != 0) goto done;
    puts("callback-error=true");
    xrtResourceLimitsInit(&limits);
    limits.iMaxInputBytes = 2;
    if (!xrtReaderSeek(reader, 0, XSEEK_START, &position) ||
        xrtSha3File(reader, &limits, 256, digest, 32, NULL, NULL, NULL) ||
        xrtErrorKind(xrtGetError()) != XERR_RANGE) goto done;
    puts("budget=true");
    status = 0;

done:
    xrtClearError();
    if (reader != NULL && !xrtReaderDestroy(reader)) status = 1;
    if (path != NULL && xrtFileExists(path) && !xrtFileDelete(path)) status = 1;
    if (directory != NULL && !xrtDirRemove(directory)) status = 1;
    xrtFree(path);
    xrtFree(directory);
    return status;
}
