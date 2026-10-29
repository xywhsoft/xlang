#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
#include "../include/xrt_bz2.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../../_bzip2/third_party/bzip2/blocksort.c"
#include "../../_bzip2/third_party/bzip2/huffman.c"
#include "../../_bzip2/third_party/bzip2/crctable.c"
#include "../../_bzip2/third_party/bzip2/randtable.c"
#include "../../_bzip2/third_party/bzip2/compress.c"
#include "../../_bzip2/third_party/bzip2/decompress.c"
#include "../../_bzip2/third_party/bzip2/bzlib.c"
#include "../xrt_bz2.c"
#include "../xrt_bz2_file.c"
#endif

typedef struct { unsigned int Calls; bool Cancel; } progress_state;

/* 回调同步借用状态；返回 false 取消，不需要额外封装文件事务。 */
static bool progress(const xrtprogress* value, ptr user)
{
    progress_state* state = user;
    if (!(value->iFlags & XRT_PROGRESS_TOTAL_KNOWN)) return false;
    state->Calls++;
    return !state->Cancel;
}

int main(void)
{
    static const uint8 text[] = "hello \xe4\xb8\x96\xe7\x95\x8c\0\xff";
    str directory = xrtDirTemp(NULL, "xlang-bz2-", NULL);
    str source = NULL, packed = NULL, target = NULL;
    progress_state state = {0};
    size_t size = 0;
    bytes plain = NULL;
    int status = 1;
    if (directory == NULL) goto done;
    source = xrtPathJoin(directory, "输入.bin");
    packed = xrtPathJoin(directory, "data.bz2");
    target = xrtPathJoin(directory, "输出.bin");
    if (source == NULL || packed == NULL || target == NULL ||
        !xrtFileWriteAll(source, (xbytesview){text, sizeof(text) - 1}) ||
        !xrtBz2CompressFile(source, packed, NULL, true, progress, &state) ||
        !xrtBz2DecompressFile(packed, target, NULL, true, NULL, NULL)) goto done;
    plain = xrtFileReadAll(target, &size);
    if (plain == NULL || size != sizeof(text) - 1 || memcmp(plain, text, size) != 0 ||
        state.Calls == 0) goto done;
    xrtFree(plain);
    plain = NULL;

    /* 取消不会用半成品覆盖已有目标；原错误在清理之后仍可读取。 */
    if (!xrtFileWriteAll(target, (xbytesview){(const uint8*)"keep", 4})) goto done;
    state.Cancel = true;
    if (xrtBz2DecompressFile(packed, target, NULL, true, progress, &state) ||
        xrtErrorKind(xrtGetError()) != XERR_CANCELLED) goto done;
    xrtClearError();
    plain = xrtFileReadAll(target, &size);
    if (plain == NULL || size != 4 || memcmp(plain, "keep", 4) != 0) goto done;
    status = 0;
done:
    xrtFree(plain);
    if (source != NULL && xrtFileExists(source) && !xrtFileDelete(source)) status = 1;
    if (packed != NULL && xrtFileExists(packed) && !xrtFileDelete(packed)) status = 1;
    if (target != NULL && xrtFileExists(target) && !xrtFileDelete(target)) status = 1;
    if (directory != NULL && !xrtDirRemove(directory)) status = 1;
    xrtFree(source);
    xrtFree(packed);
    xrtFree(target);
    xrtFree(directory);
    if (status == 0) {
        puts("files: ok");
        puts("progress: ok");
        puts("cancel: preserved");
    }
    return status;
}
