/* 文件编解码器的双故障夹具；只复用测试逻辑，调用仍展开为各包公开 C API。 */
#include <stdio.h>
#include <string.h>

typedef struct { unsigned Budget, Calls; bool FinalOnly, Armed; } rollback_state;

static bool rollback_progress(const xrtprogress* value, ptr user)
{
    rollback_state* state = user;
    ++state->Calls;
#if CODEC_TEST_CLOSED
    /* 最终通知后仅剩刷新、关闭、改名和回滚；逐点注入第二个故障。 */
    if (value->iFlags & XRT_PROGRESS_FINAL) state->Armed = xrtMemDebugFailAfter(state->Budget);
    return true;
#else
    if (state->FinalOnly && !(value->iFlags & XRT_PROGRESS_FINAL)) return true;
    xrtSetErrorInfo(XERR_ARGUMENT, "test." CODEC_TEST_NAME ".cleanup", 493, "callback rejected output");
    state->Armed = xrtMemDebugFailAfter(0);
    return false;
#endif
}

static int temporary_count(cstr directory)
{
    xdir dir = xrtDirOpen(directory, 0);
    xdirentry entry;
    xdirnext next;
    int count = 0;
    if (dir == NULL) return -1;
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM) {
        if (entry.Name.Size >= sizeof(CODEC_TEST_PREFIX) - 1u &&
            memcmp(entry.Name.Data, CODEC_TEST_PREFIX, sizeof(CODEC_TEST_PREFIX) - 1u) == 0) ++count;
    }
    bool closed = xrtDirClose(dir);
    return next == XDIR_NEXT_END && closed ? count : -1;
}

static bool rollback_case(cstr root, cstr source, cstr packed, unsigned index)
{
    char directory[1024], target[1024];
    bool decode = (index & 1u) != 0u;
    bool existing = (index & 4u) != 0u;
    rollback_state state = {index / 2u, 0u, (index & 2u) != 0u, false};
    xmemdebugsnapshot before, after;
    (void)snprintf(directory, sizeof(directory), "%s/case-%u", root, index);
    (void)snprintf(target, sizeof(target), "%s/case-%u/target", root, index);
    if (!xrtDirCreate(directory)) return false;
#if CODEC_TEST_CLOSED
    if (!xrtDirCreate(target)) return false;
#else
    if (existing && !xrtFileWriteAll(target, (xbytesview){(cbytes)"keep", 4u})) return false;
#endif
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    bool ok = decode ? CODEC_TEST_DECOMPRESS(packed, target, existing || CODEC_TEST_CLOSED, rollback_progress, &state) :
        CODEC_TEST_COMPRESS(source, target, existing || CODEC_TEST_CLOSED, rollback_progress, &state);
    bool allocated = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
#if CODEC_TEST_CLOSED
    bool firstError = xrtGetError() != NULL;
    int kind = (int)xrtErrorKind(xrtGetError());
#else
    bool firstError = xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
        xrtErrorCode(xrtGetError()) == 493 &&
        strcmp(xrtErrorDomain(xrtGetError()), "test." CODEC_TEST_NAME ".cleanup") == 0 &&
        strcmp(xrtErrorMessage(xrtGetError()), "callback rejected output") == 0;
#endif
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    int temporary = temporary_count(directory);
    bool original;
#if CODEC_TEST_CLOSED
    original = xrtDirExists(target);
#else
    if (existing) {
        size_t size = 0;
        bytes data = xrtFileReadAll(target, &size);
        original = data != NULL && size == 4u && memcmp(data, "keep", 4u) == 0;
        xrtFree(data);
    } else original = !xrtPathExists(target);
#endif
    bool passed = !ok && state.Armed && state.Calls > 0u && firstError && original &&
        (CODEC_TEST_CLOSED || !allocated) && temporary == 0 &&
        before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount;
#if CODEC_TEST_CLOSED
    printf(CODEC_TEST_NAME " commit cleanup %u: decode=%d budget=%u allocated=%d temporary=%d original=%d kind=%d\n",
        index, (int)decode, state.Budget, (int)allocated, temporary, (int)original, kind);
#else
    printf(CODEC_TEST_NAME " cleanup %u: decode=%d final=%d existing=%d allocated=%d temporary=%d first=%d original=%d\n",
        index, (int)decode, (int)state.FinalOnly, (int)existing, (int)allocated, temporary,
        (int)firstError, (int)original);
#endif
    if (passed) {
#if CODEC_TEST_CLOSED
        if (!xrtDirRemove(target)) return false;
#else
        if (existing && !xrtFileDelete(target)) return false;
#endif
        return xrtDirRemove(directory);
    }
    return false;
}

int main(int argc, char** argv)
{
    str ownedRoot = NULL;
    cstr root;
    char source[1024], packed[1024];
    bool passed = true;
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 2;
    xrtMemDebugFailClear();
    if (argc == 1) ownedRoot = xrtDirTemp(NULL, "xlang-" CODEC_TEST_NAME "-rollback-", NULL);
    root = argc == 2 ? argv[1] : ownedRoot;
    if (root == NULL || strlen(root) > 900u || !xrtDirCreateAll(root)) goto setup_failed;
    (void)snprintf(source, sizeof(source), "%s/source.bin", root);
    (void)snprintf(packed, sizeof(packed), "%s/source." CODEC_TEST_NAME, root);
#if CODEC_TEST_CLOSED
    uint8 payload[] = {'p', 'a', 'y', 'l', 'o', 'a', 'd'};
#else
    uint8 payload[65537];
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = (uint8)(i * 31u + i / 17u);
#endif
    if (!xrtFileWriteAll(source, (xbytesview){payload, sizeof(payload)}) ||
        !CODEC_TEST_COMPRESS(source, packed, true, NULL, NULL)) goto setup_failed;
    for (unsigned i = 0; i < (CODEC_TEST_CLOSED ? 16u : 8u); ++i)
        passed &= rollback_case(root, source, packed, i);
    if (passed) passed = xrtFileDelete(source) && xrtFileDelete(packed) && xrtDirRemove(root);
    if (passed) puts(CODEC_TEST_NAME " rollback double failure: PASS");
    else fprintf(stderr, CODEC_TEST_NAME " rollback: failed artifacts=%s\n", root);
    xrtFree(ownedRoot);
    return passed ? 0 : 1;
setup_failed:
    fprintf(stderr, CODEC_TEST_NAME " rollback: setup failed artifacts=%s\n", root ? root : "(no directory)");
    xrtFree(ownedRoot);
    return 2;
}
