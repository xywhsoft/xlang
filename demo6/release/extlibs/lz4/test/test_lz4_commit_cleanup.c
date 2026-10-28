#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_lz4.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../lz4_hash.c"
#include "../third_party/lz4.c"
#include "../third_party/lz4hc.c"
#include "../lz4_frame.c"
#include "../xrt_lz4.c"
#include "../xrt_lz4_file.c"
#endif

typedef struct { unsigned Budget; bool Armed; } commit_state;

/* 最终通知后仅剩刷新、关闭、改名和回滚；逐点注入第二个故障。 */
static bool arm_on_final(const xrtprogress* value, ptr user)
{
    commit_state* state = user;
    if (value->iFlags & XRT_PROGRESS_FINAL) state->Armed = xrtMemDebugFailAfter(state->Budget);
    return true;
}

static int temporary_count(cstr directory)
{
    xdir dir = xrtDirOpen(directory, 0);
    xdirentry entry;
    xdirnext next;
    int count = 0;
    if (dir == NULL) return -1;
    while ((next = xrtDirNext(dir, &entry)) == XDIR_NEXT_ITEM) {
        if (entry.Name.Size >= 9u && memcmp(entry.Name.Data, ".xrt-lz4-", 9u) == 0) ++count;
    }
    bool closed = xrtDirClose(dir);
    return next == XDIR_NEXT_END && closed ? count : -1;
}

int main(int argc, char** argv)
{
    str ownedRoot = NULL;
    cstr root;
    char source[1024], packed[1024], directory[1024], target[1024];
    bool passed = true;
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 2;
    xrtMemDebugFailClear();
    if (argc == 1) ownedRoot = xrtDirTemp(NULL, "xlang-lz4-commit-", NULL);
    root = argc == 2 ? argv[1] : ownedRoot;
    if (root == NULL || strlen(root) > 900u || !xrtDirCreateAll(root)) return 2;
    (void)snprintf(source, sizeof(source), "%s/source.bin", root);
    (void)snprintf(packed, sizeof(packed), "%s/source.lz4", root);
    if (!xrtFileWriteAll(source, (xbytesview){(cbytes)"payload", 7u}) ||
        !xrtLz4CompressFile(source, packed, NULL, true, NULL, NULL)) return 2;
    for (unsigned i = 0; i < 16u; ++i) {
        bool decode = (i & 1u) != 0u;
        commit_state state = {i / 2u, false};
        xmemdebugsnapshot before, after;
        (void)snprintf(directory, sizeof(directory), "%s/case-%u", root, i);
        (void)snprintf(target, sizeof(target), "%s/case-%u/target", root, i);
        if (!xrtDirCreate(directory) || !xrtDirCreate(target)) return 2;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        bool ok = decode ? xrtLz4DecompressFile(packed, target, NULL, true, arm_on_final, &state) :
            xrtLz4CompressFile(source, target, NULL, true, arm_on_final, &state);
        bool allocated = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        bool failed = !ok && xrtGetError() != NULL;
        int kind = (int)xrtErrorKind(xrtGetError());
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        int temporary = temporary_count(directory);
        bool original = xrtDirExists(target);
        bool onePassed = state.Armed && failed && temporary == 0 && original &&
            before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount;
        printf("lz4 commit cleanup %u: decode=%d budget=%u allocated=%d temporary=%d original=%d kind=%d\n",
            i, (int)decode, state.Budget, (int)allocated, temporary, (int)original, kind);
        passed &= onePassed;
        if (onePassed && (!xrtDirRemove(target) || !xrtDirRemove(directory))) return 2;
    }
    if (passed) passed = xrtFileDelete(source) && xrtFileDelete(packed) && xrtDirRemove(root);
    if (passed) puts("lz4 commit cleanup: closed transaction/rollback double failure PASS");
    else fprintf(stderr, "lz4 commit cleanup: failed artifacts=%s\n", root);
    xrtFree(ownedRoot);
    return passed ? 0 : 1;
}
