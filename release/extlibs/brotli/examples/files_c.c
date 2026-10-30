#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
#include "../include/xrt_brotli.h"
#include <stdio.h>
#include <string.h>

typedef struct { unsigned Calls; bool Cancel; } progress_state;

/* 普通同步 C 回调；借用用户状态，不构造任何 xvalue/function 对象。 */
static bool progress(const xrtprogress* value, ptr user)
{
    progress_state* state = user;
    if (!(value->iFlags & XRT_PROGRESS_TOTAL_KNOWN)) return false;
    ++state->Calls;
    return !state->Cancel;
}

int main(void)
{
    static const uint8 input[] = {0, 255, 128, 65};
    str directory = xrtDirTemp(NULL, "xlang-brotli-files-example-", NULL);
    str source = NULL, packed = NULL, target = NULL;
    bytes plain = NULL;
    size_t size = 0;
    int status = 1;
    progress_state state = {0};
    xrtbrotlioptions options;
    xrtBrotliOptionsInit(&options);
    options.iQuality = 4;
    if (!directory) goto done;
    source = xrtPathJoin(directory, "输入.bin");
    packed = xrtPathJoin(directory, "packed.br");
    target = xrtPathJoin(directory, "输出.bin");
    if (!source || !packed || !target || !xrtFileWriteAll(source, (xbytesview){input, sizeof(input)}) ||
        !xrtBrotliCompressFile(source, packed, &options, true, progress, &state) ||
        !xrtBrotliDecompressFile(packed, target, NULL, true, NULL, NULL)) goto done;
    plain = xrtFileReadAll(target, &size);
    if (!plain || size != sizeof(input) || memcmp(plain, input, size) != 0 || state.Calls == 0) goto done;
    xrtFree(plain); plain = NULL;

    // 同 XLang 范例：取消解压后必须保留原目标，而非半份输出。
    if (!xrtFileWriteAll(target, (xbytesview){(cbytes)"keep", 4})) goto done;
    state.Cancel = true;
    if (xrtBrotliDecompressFile(packed, target, NULL, true, progress, &state) ||
        xrtErrorKind(xrtGetError()) != XERR_CANCELLED) goto done;
    xrtClearError();
    plain = xrtFileReadAll(target, &size);
    if (!plain || size != 4 || memcmp(plain, "keep", 4) != 0) goto done;
    status = 0;
done:
    xrtFree(plain);
    if (source && xrtFileExists(source) && !xrtFileDelete(source)) status = 1;
    if (packed && xrtFileExists(packed) && !xrtFileDelete(packed)) status = 1;
    if (target && xrtFileExists(target) && !xrtFileDelete(target)) status = 1;
    if (directory && !xrtDirRemove(directory)) status = 1;
    xrtFree(source); xrtFree(packed); xrtFree(target); xrtFree(directory);
    if (status == 0) {
        puts("files: ok");
        puts("progress: ok");
        puts("cancel: preserved");
    }
    return status;
}
