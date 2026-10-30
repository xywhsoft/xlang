/* native 文件 ABI 的双故障夹具；与普通 C progress/FINAL 协议分开。 */
#ifndef XRT_MODULE_DIR
#define XRT_MODULE_DIR
#endif
#ifndef XRT_MODULE_DIR_TEMP
#define XRT_MODULE_DIR_TEMP
#endif
#ifndef XRT_MODULE_IO_FILE
#define XRT_MODULE_IO_FILE
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#ifndef XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#endif
#include "xruntime.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    xvalue *Reply;
    unsigned Calls, When, Budget;
    bool Reject, Inject, Armed;
} progress_state;

static bool report(ptr context, const xrtcallframe *frame, xrtcallresult *result) {
    progress_state *state = context;
    if (frame->ArgumentCount != 3 || !xrtCallResultSet(result, 0, state->Reply)) return false;
    ++state->Calls;
    if (state->Calls != state->When) return true;
    if (state->Reject)
        xrtSetErrorInfo(XERR_ARGUMENT, "test." NATIVE_CODEC_NAME ".rollback", 493, "callback rejected output");
    if (state->Inject) state->Armed = xrtMemDebugFailAfter(state->Budget);
    return !state->Reject;
}
static int transform(bool decode, cstr source, cstr target, xvalue *callback) {
    return decode ? NATIVE_CODEC_DECOMPRESS(source, target, true, 1048576, 1048576, 1000, callback) :
        NATIVE_CODEC_COMPRESS(source, target, 0, true, 1048576, 1048576, callback);
}
static bool write_bytes(cstr path, const void *data, size_t size) {
    xwriter *writer = xrtWriterOpen(path);
    bool ok = writer && xrtWriterWriteFull(writer, data, size, NULL);
    return xrtWriterDestroy(writer) && ok;
}
static bool original_file(cstr path) {
    unsigned char bytes[5]; size_t size = 0;
    xreader *reader = xrtReaderOpen(path);
    bool ok = reader && xrtReaderRead(reader, bytes, sizeof(bytes), &size) &&
        size == 4 && !memcmp(bytes, "keep", 4);
    return xrtReaderDestroy(reader) && ok;
}
static int temporary_count(cstr path) {
    static const char prefix[] = NATIVE_CODEC_PREFIX;
    xdir directory = xrtDirOpen(path, 0); xdirentry entry; xdirnext next; int count = 0;
    if (!directory) return -1;
    while ((next = xrtDirNext(directory, &entry)) == XDIR_NEXT_ITEM)
        if (entry.Name.Size >= sizeof(prefix) - 1 && !memcmp(entry.Name.Data, prefix, sizeof(prefix) - 1)) ++count;
    return xrtDirClose(directory) && next == XDIR_NEXT_END ? count : -1;
}
static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

int main(int argc, char **argv) {
    bool closed = argc == 2 && !strcmp(argv[1], "--closed");
    bool passed = true;
    unsigned cases = 0, failures = 0, faults = 0, last[2] = {0};
    char source[1024], packed[1024], directory[1024], target[1024];
    str root = NULL;
    progress_state state = {0};
    xrtcallable *callable = NULL; xvalue *callback = NULL;
    if (argc != 1 && !closed) return 2;
    if (!(xrtMemDebugEnabled() || xrtMemDebugEnable(true))) return 2;
    root = xrtDirTemp(NULL, "xlang-" NATIVE_CODEC_NAME "-rollback-", "");
    if (!root || strlen(root) > 900) goto setup_failed;
    snprintf(source, sizeof(source), "%s/source.bin", root);
    snprintf(packed, sizeof(packed), "%s/source.packed", root);
    snprintf(target, sizeof(target), "%s/warmup", root);
    unsigned char payload[65537];
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = (unsigned char)(i * 31u + i / 17u);
    state.Reply = xrtValueBool(true);
    callable = xrtCallableCreate(NULL, report, &state, NULL);
    callback = callable ? xrtValueCallable(callable) : NULL;
    if (!state.Reply || !callback || !write_bytes(source, payload, sizeof(payload)) ||
        transform(false, source, packed, NULL) != 1) goto setup_failed;
    /* 用实际成功回调次数选择最后一次调用，不伪造 FINAL 标志。 */
    for (unsigned decode = 0; decode < 2; ++decode) {
        state.Calls = 0;
        if (transform(decode != 0, decode ? packed : source, target, callback) != 1 ||
            !state.Calls || !xrtFileDelete(target)) goto setup_failed;
        last[decode] = state.Calls;
    }
    /* 预热真实错误包装；故障矩阵不把一次性初始化误算成泄漏。 */
    state.Calls = 0; state.When = 1; state.Reject = true;
    if (transform(false, source, target, callback) != -2 || temporary_count(root) != 0) goto setup_failed;
    xrtClearError();
    /* 双向、首次/末次、旧/新目标；预算覆盖包装首错及后续清理的真实分配点。 */
    for (unsigned mode = 0; mode < (closed ? 2u : 8u); ++mode) {
        bool decode = (mode & 1u) != 0, existing = (mode & 4u) != 0;
        for (unsigned budget = 0; budget < (closed ? 8u : 12u); ++budget) {
            xmemdebugsnapshot before, after;
            snprintf(directory, sizeof(directory), "%s/case-%u-%u", root, mode, budget);
            snprintf(target, sizeof(target), "%s/case-%u-%u/target", root, mode, budget);
            if (!xrtDirCreate(directory) ||
                (closed ? !xrtDirCreate(target) : existing && !write_bytes(target, "keep", 4))) goto setup_failed;
            state.Calls = 0; state.When = closed || (mode & 2u) ? last[decode] : 1;
            state.Budget = budget; state.Reject = !closed; state.Inject = true; state.Armed = false;
            xrtClearError(); xrtMemDebugSnapshot(&before);
            int result = transform(decode, decode ? packed : source, target, callback);
            bool triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
            const xerror *cause = xrtErrorFind(xrtGetError(), "test." NATIVE_CODEC_NAME ".rollback", 493);
            bool error = closed ? xrtGetError() != NULL : cause &&
                xrtErrorKind(cause) == XERR_ARGUMENT && !strcmp(xrtErrorMessage(cause), "callback rejected output");
            xrtClearError(); xrtMemDebugSnapshot(&after);
            int temporary = temporary_count(directory);
            bool original = closed ? xrtDirExists(target) : existing ? original_file(target) : !xrtPathExists(target);
            bool ok = result == (closed ? 0 : -2) && state.Armed && state.Calls == state.When &&
                error && original && temporary == 0 && balanced(&before, &after);
            ++cases; if (triggered) ++faults;
            if (!ok) {
                ++failures;
                printf(NATIVE_CODEC_NAME " rollback mode=%u budget=%u closed=%d result=%d armed=%d faults=%d temporary=%d first=%d original=%d balanced=%d\n",
                    mode, budget, closed, result, state.Armed, triggered, temporary, error, original, balanced(&before, &after));
            } else {
                if ((closed ? !xrtDirRemove(target) : existing && !xrtFileDelete(target)) ||
                    !xrtDirRemove(directory)) goto setup_failed;
            }
        }
    }
    passed = failures == 0 && faults != 0;
    if (passed && (!xrtFileDelete(source) || !xrtFileDelete(packed) || !xrtDirRemove(root))) goto setup_failed;
    printf(NATIVE_CODEC_NAME " %s rollback: %u cases, %u real OOM, %u failures\n", closed ? "closed" : "open", cases, faults, failures);
    if (!passed) fprintf(stderr, NATIVE_CODEC_NAME " rollback artifacts: %s\n", root);
    xrtValueRelease(callback); xrtCallableUnref(callable); xrtValueRelease(state.Reply); xrtFree(root);
    return passed ? 0 : 1;
setup_failed:
    xrtMemDebugFailClear();
    fprintf(stderr, NATIVE_CODEC_NAME " rollback setup failed: %s (%s)\n", root ? root : "(none)", xrtErrorMessage(xrtGetError()));
    xrtValueRelease(callback); xrtCallableUnref(callable); xrtValueRelease(state.Reply); xrtFree(root);
    return 2;
}
