/* 与 file_transaction.xl 同功能；使用现有 native 文件 ABI 和真实 XRT callable。 */
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#define XRT_MODULE_IO_FILE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#include "xruntime.h"
#include "../include/lzma_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 仅独立 xl run/build 单文件时包含后端；GCC 按 manifest 分离编译。 */
#include "../third_party/xz/7zCrc.c"
#include "../third_party/xz/7zCrcOpt.c"
#include "../third_party/xz/Alloc.c"
#include "../third_party/xz/Bra.c"
#include "../third_party/xz/Bra86.c"
#include "../third_party/xz/CpuArch.c"
#include "../third_party/xz/Delta.c"
#include "../third_party/xz/LzFind.c"
#include "../third_party/xz/LzmaDec.c"
#include "../third_party/xz/LzmaEnc.c"
#include "../third_party/xz/Lzma2Dec.c"
#include "../third_party/xz/Lzma2Enc.c"
#include "../third_party/xz/Sha256.c"
#include "../third_party/xz/Sha256Opt.c"
#include "../third_party/xz/Xz.c"
#include "../third_party/xz/XzCrc64.c"
#include "../third_party/xz/XzCrc64Opt.c"
#include "../third_party/xz/XzDec.c"
#include "../third_party/xz/XzEnc.c"
#include "../xrt_lzma_common.c"
#include "../xrt_lzma_encode.c"
#include "../xrt_lzma_decode.c"
#include "../xrt_lzma.c"
#include "../lzma_native.c"
#endif

typedef struct { xvalue *Reply; bool Raise; } progress_state;
static bool progress(ptr context, const xrtcallframe *frame, xrtcallresult *result) {
    progress_state *state = context;
    if (frame->ArgumentCount != 3) return false;
    if (state->Raise) {
        xrtSetErrorInfo(XERR_ARGUMENT, "example.lzma", 493, "stop requested");
        return false;
    }
    return xrtCallResultSet(result, 0, state->Reply);
}
static bool write_text(cstr path, cstr text) {
    xwriter *writer = xrtWriterOpen(path);
    bool ok = writer && xrtWriterWriteFull(writer, text, strlen(text), NULL);
    return xrtWriterDestroy(writer) && ok;
}
static bool equals(cstr path, cstr text) {
    char bytes[32]; size_t size = 0;
    xreader *reader = xrtReaderOpen(path);
    bool ok = reader && xrtReaderRead(reader, bytes, sizeof(bytes), &size) &&
        size == strlen(text) && !memcmp(bytes, text, size);
    return xrtReaderDestroy(reader) && ok;
}

int main(void) {
    str work = xrtDirTemp(NULL, "xlang-xz-transaction-", "");
    char source[1024] = {0}, packed[1024] = {0}, restored[1024] = {0};
    progress_state state = {xrtValueBool(false), false};
    xrtcallable *callable = xrtCallableCreate(NULL, progress, &state, NULL);
    xvalue *callback = callable ? xrtValueCallable(callable) : NULL;
    bool ok = false;
    if (!work || strlen(work) > 900 || !state.Reply || !callback) goto cleanup;
    snprintf(source, sizeof(source), "%s/输入.txt", work);
    snprintf(packed, sizeof(packed), "%s/output.xz", work);
    snprintf(restored, sizeof(restored), "%s/restored.txt", work);
    if (!write_text(source, "hello XZ") || !write_text(packed, "keep")) goto cleanup;

    /* 布尔 false 是取消；C 回调执行失败则通过 xerror 的 cause 保留首因。 */
    int status = xlzma_compress_file(source, packed, 0, true, 1048576, 1048576, callback);
    if (status != -1 || xrtErrorKind(xrtGetError()) != XERR_CANCELLED) goto cleanup;
    xrtClearError();
    if (!equals(packed, "keep")) goto cleanup;
    state.Raise = true;
    status = xlzma_compress_file(source, packed, 0, true, 1048576, 1048576, callback);
    const xerror *cause = xrtErrorFind(xrtGetError(), "example.lzma", 493);
    if (status != -2 || !cause || strcmp(xrtErrorMessage(cause), "stop requested")) goto cleanup;
    xrtClearError();
    if (!equals(packed, "keep")) goto cleanup;

    if (xlzma_compress_file(source, packed, 0, true, 1048576, 1048576, NULL) != 1 ||
        xlzma_decompress_file(packed, restored, true, 1048576, 1048576, 1000, NULL) != 1 ||
        !equals(restored, "hello XZ")) goto cleanup;
    puts("cancelled: true\ncallback error: true\nrestored: hello XZ");
    ok = true;
cleanup:
    if (!ok) fprintf(stderr, "lzma transaction example: %s\n", xrtErrorMessage(xrtGetError()));
    xrtValueRelease(callback); xrtCallableUnref(callable); xrtValueRelease(state.Reply);
    if (source[0] && xrtPathExists(source) && !xrtFileDelete(source)) ok = false;
    if (packed[0] && xrtPathExists(packed) && !xrtFileDelete(packed)) ok = false;
    if (restored[0] && xrtPathExists(restored) && !xrtFileDelete(restored)) ok = false;
    if (work && !xrtDirRemove(work)) ok = false;
    xrtFree(work);
    return ok ? 0 : 1;
}
