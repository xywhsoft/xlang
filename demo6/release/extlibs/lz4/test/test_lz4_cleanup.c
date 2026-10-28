#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_lz4.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
/* 与正式源码包共用实现；普通 GCC 门禁仍分离编译。 */
#include "../lz4_hash.c"
#include "../third_party/lz4.c"
#include "../third_party/lz4hc.c"
#include "../lz4_frame.c"
#include "../xrt_lz4.c"
#include "../xrt_lz4_file.c"
#endif

typedef struct { bool FinalOnly, Armed; unsigned Calls; } cleanup_state;

/* 回调先提供首因，再使清理阶段遇到 OOM；不是正文的单次故障扫描。 */
static bool reject_then_oom(const xrtprogress* value, ptr user)
{
    cleanup_state* state = user;
    ++state->Calls;
    if (state->FinalOnly && !(value->iFlags & XRT_PROGRESS_FINAL)) return true;
    xrtSetErrorInfo(XERR_ARGUMENT, "test.lz4.cleanup", 493, "callback rejected output");
    state->Armed = xrtMemDebugFailAfter(0);
    return false;
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

static bool check_case(cstr root, cstr source, cstr packed,
    unsigned index, bool decode, bool finalOnly, bool existing)
{
    char directory[1024], target[1024];
    cleanup_state state = {finalOnly, false, 0};
    xmemdebugsnapshot before, after;
    bool ok, allocated, original, firstError;
    size_t size = 0;
    bytes data;
    (void)snprintf(directory, sizeof(directory), "%s/case-%u", root, index);
    (void)snprintf(target, sizeof(target), "%s/case-%u/target.bin", root, index);
    if (!xrtDirCreate(directory) ||
        (existing && !xrtFileWriteAll(target, (xbytesview){(cbytes)"keep", 4u}))) return false;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    ok = decode ? xrtLz4DecompressFile(packed, target, NULL, existing, reject_then_oom, &state) :
        xrtLz4CompressFile(source, target, NULL, existing, reject_then_oom, &state);
    allocated = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    firstError = xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT &&
        xrtErrorCode(xrtGetError()) == 493 &&
        strcmp(xrtErrorDomain(xrtGetError()), "test.lz4.cleanup") == 0 &&
        strcmp(xrtErrorMessage(xrtGetError()), "callback rejected output") == 0;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    int temporary = temporary_count(directory);
    if (existing) {
        data = xrtFileReadAll(target, &size);
        original = data != NULL && size == 4u && memcmp(data, "keep", 4u) == 0;
        xrtFree(data);
    } else original = !xrtPathExists(target);
    bool passed = !ok && state.Armed && state.Calls > 0u && firstError && original &&
        !allocated && temporary == 0 && before.LiveCount == after.LiveCount &&
        before.LiveBytes == after.LiveBytes && before.InvalidFreeCount == after.InvalidFreeCount &&
        before.DoubleFreeCount == after.DoubleFreeCount && before.OverflowCount == after.OverflowCount &&
        before.UnderflowCount == after.UnderflowCount;
    printf("lz4 cleanup %u: decode=%d final=%d existing=%d allocated=%d temporary=%d first=%d original=%d\n",
        index, (int)decode, (int)finalOnly, (int)existing, (int)allocated, temporary,
        (int)firstError, (int)original);
    if (passed) {
        if (existing && !xrtFileDelete(target)) return false;
        return xrtDirRemove(directory);
    }
    return false;
}

int main(int argc, char** argv)
{
    str ownedRoot = NULL;
    cstr root;
    char source[1024], packed[1024];
    uint8 payload[65537];
    bool passed = true;
    if (!xrtMemDebugEnabled() || !xrtMemDebugFailAfter(UINT64_MAX)) return 2;
    xrtMemDebugFailClear();
    if (argc == 1) ownedRoot = xrtDirTemp(NULL, "xlang-lz4-cleanup-", NULL);
    root = argc == 2 ? argv[1] : ownedRoot;
    if (root == NULL || strlen(root) > 900u || !xrtDirCreateAll(root)) return 2;
    (void)snprintf(source, sizeof(source), "%s/source.bin", root);
    (void)snprintf(packed, sizeof(packed), "%s/source.lz4", root);
    for (size_t i = 0; i < sizeof(payload); ++i) payload[i] = (uint8)(i * 31u + i / 17u);
    if (!xrtFileWriteAll(source, (xbytesview){payload, sizeof(payload)}) ||
        !xrtLz4CompressFile(source, packed, NULL, true, NULL, NULL)) return 2;
    for (unsigned i = 0; i < 8u; ++i)
        passed &= check_case(root, source, packed, i, (i & 1u) != 0u,
            (i & 2u) != 0u, (i & 4u) != 0u);
    if (passed) passed = xrtFileDelete(source) && xrtFileDelete(packed) && xrtDirRemove(root);
    if (passed) puts("lz4 cleanup: callback/rollback double failure PASS");
    else fprintf(stderr, "lz4 cleanup: failed artifacts=%s\n", root);
    xrtFree(ownedRoot);
    return passed ? 0 : 1;
}
