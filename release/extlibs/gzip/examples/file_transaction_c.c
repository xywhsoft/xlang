/* 与 file_transaction.xl 等价，沿用 native 文件 ABI 和正式 XRT callable。 */
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_BUFFER
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include "../include/xrt_gzip.h"
#include "../include/gzip_native.h"
#include <stdio.h>
#include <string.h>

typedef struct { xvalue *Reply; bool Raise; } progress_state;
static bool progress(ptr context, const xrtcallframe *frame, xrtcallresult *result) {
    progress_state *state = context;
    if (frame->ArgumentCount != 3) return false;
    if (state->Raise) {
        xrtSetErrorInfo(XERR_ARGUMENT, "example.gzip", 7431, "stop requested");
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
    str work = xrtDirTemp(NULL, "xlang-gzip-transaction-", "");
    char source[1024] = {0}, packed[1024] = {0}, restored[1024] = {0};
    progress_state state = {xrtValueBool(false), false};
    xrtcallable *callable = xrtCallableCreate(NULL, progress, &state, NULL);
    xvalue *callback = callable ? xrtValueCallable(callable) : NULL;
    bool ok = false;
    if (!work || strlen(work) > 900 || !state.Reply || !callback) goto cleanup;
    snprintf(source, sizeof(source), "%s/输入.txt", work);
    snprintf(packed, sizeof(packed), "%s/output.gz", work);
    snprintf(restored, sizeof(restored), "%s/restored.txt", work);
    if (!write_text(source, "hello gzip") || !write_text(packed, "keep")) goto cleanup;
    int status = xgzip_compress_file(source, packed, 6, 0, true, 1048576, 1048576, callback);
    if (status != -1 || xrtErrorKind(xrtGetError()) != XERR_CANCELLED) goto cleanup;
    xrtClearError();
    if (!equals(packed, "keep")) goto cleanup;
    // 业务错误与布尔取消分开，原始 domain/code/message 由 cause 保存。
    state.Raise = true;
    status = xgzip_compress_file(source, packed, 6, 0, true, 1048576, 1048576, callback);
    const xerror *cause = xrtErrorFind(xrtGetError(), "example.gzip", 7431);
    if (status != -2 || !cause || strcmp(xrtErrorMessage(cause), "stop requested")) goto cleanup;
    xrtClearError();
    if (!equals(packed, "keep")) goto cleanup;
    if (xgzip_compress_file(source, packed, 6, 0, true, 1048576, 1048576, NULL) != 1 ||
        xgzip_decompress_file(packed, restored, true, 1048576, 1048576, 1000, NULL) != 1 ||
        !equals(restored, "hello gzip")) goto cleanup;
    puts("cancelled: true\ncallback error: true\nrestored: hello gzip");
    ok = true;
cleanup:
    if (!ok) fprintf(stderr, "gzip transaction example: %s\n", xrtErrorMessage(xrtGetError()));
    xrtValueRelease(callback); xrtCallableUnref(callable); xrtValueRelease(state.Reply);
    if (source[0] && xrtPathExists(source) && !xrtFileDelete(source)) ok = false;
    if (packed[0] && xrtPathExists(packed) && !xrtFileDelete(packed)) ok = false;
    if (restored[0] && xrtPathExists(restored) && !xrtFileDelete(restored)) ok = false;
    if (work && !xrtDirRemove(work)) ok = false;
    xrtFree(work);
    return ok ? 0 : 1;
}
#ifdef __TINYC__
#include "../xrt_gzip.c"
#include "../gzip_native.c"
#endif
