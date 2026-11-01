/* 检查保存失败后的整个目录，不只检查旧目标和进程内存。 */
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_MEMORY_DEBUG
#if defined(BSON_TEST_XRT_ONLY)
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_VALUE_CONTAINER
#include "xrt.h"
#else
#include "../bson_native.c"
#endif
#include <stdio.h>
#include <string.h>

static int64_t save_data(cstr path, xvalue* document)
{
#if defined(BSON_TEST_XRT_ONLY)
    static const uint8 wire[] = {5, 0, 0, 0, 0};
    (void)document;
    return xrtFileWriteAtomic(path, (xbytesview){wire, sizeof(wire)}) ? (int64_t)sizeof(wire) : 0;
#else
    return xbson_save(path, document);
#endif
}

static int extras(cstr path)
{
    xdir directory = xrtDirOpen(path, 0);
    xdirentry entry;
    xdirnext next;
    int count = 0;
    static const char target[] = "保留.bson";
    if (!directory) return -1;
    while ((next = xrtDirNext(directory, &entry)) == XDIR_NEXT_ITEM)
        if (entry.Name.Size != sizeof(target)-1 ||
            memcmp(entry.Name.Data, target, sizeof(target)-1)) ++count;
    return xrtDirClose(directory) && next == XDIR_NEXT_END ? count : -1;
}

static bool balanced(const xmemdebugsnapshot* a, const xmemdebugsnapshot* b)
{
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->OverflowCount == b->OverflowCount && a->UnderflowCount == b->UnderflowCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

int main(int argc, char** argv)
{
    bool locked = argc == 2 && !strcmp(argv[1], "--locked");
    if (argc != 1 && !locked) return 2;
#if !defined(_WIN32)
    if (locked) return 77;
#endif
    if (!xrtMemDebugEnabled()) return 2;
    str root = xrtDirTemp(NULL, "xlang-bson-save-cleanup-", "");
    xvalue* document = xrtValueObject();
    xvalue* number = xrtValueInt(1);
    char directory[1024], target[1100];
    unsigned cases = 0, faults = 0, failures = 0;
    if (!root || strlen(root) > 900 || !document || !number ||
        !xrtValueObjectSetTake(document, XRT_STR_LITERAL("n"), &number)) return 2;
    snprintf(target, sizeof(target), "%s/warmup.bson", root);
    if (!save_data(target, document) || !xrtFileDelete(target)) return 2;

    for (unsigned existing = locked ? 1 : 0; existing != 2; ++existing) {
        bool completed = false;
        for (unsigned budget = 0; budget < 128; ++budget) {
            snprintf(directory, sizeof(directory), "%s/%u-%u", root, existing, budget);
            snprintf(target, sizeof(target), "%s/保留.bson", directory);
            if (!xrtDirCreate(directory) || (existing &&
                !xrtFileWriteAll(target, (xbytesview){(const uint8*)"keep", 4}))) return 2;
            // Windows 正常共享策略可禁止替换，构造真实的发布失败再叠加分配故障。
            xfile held = NULL;
            int expected_kind = 0;
            if (locked) {
                xfileoptions options;
                xrtFileOptionsInit(&options);
                options.Share = XFILE_SHARE_READ | XFILE_SHARE_WRITE;
                held = xrtFileOpen(target, &options);
                if (!held) return 2;
                // 用无故障对照取得真实系统错误分类，不假设它必定属于 XERR_IO。
                if (save_data(target, document) != 0 || !xrtGetError()) return 2;
                expected_kind = xrtErrorKind(xrtGetError());
                if (extras(directory) != 0) return 2;
            }
            xrtClearError();
            xmemdebugsnapshot before, after;
            xrtMemDebugSnapshot(&before);
            if (!xrtMemDebugFailAfter(budget)) return 2;
            int64_t written = save_data(target, document);
            bool triggered = xrtMemDebugFailTriggered();
            bool memory = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            int kind = xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : 0;
            xrtMemDebugFailClear();
            bool correct = locked ? written == 0 && (memory || kind == expected_kind) :
                triggered ? written == 0 && memory : written > 0 && !xrtGetError();
            if (triggered) ++faults;
            if (triggered || locked) {
                if (existing) {
                    size_t size = 0;
                    bytes old = xrtFileReadAllLimit(target, 16, &size);
                    correct = correct && old && size == 4 && !memcmp(old, "keep", 4);
                    xrtFree(old);
                } else {
                    correct = correct && !xrtFileExists(target);
                }
            }
            int remaining = extras(directory);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            bool ok = correct && remaining == 0 && balanced(&before, &after);
            if (held && !xrtClose(held)) return 2;
            ++cases;
            printf("save existing=%u budget=%u fault=%u extra=%d kind=%d correct=%u balanced=%u\n",
                existing, budget, (unsigned)triggered, remaining,
                kind, (unsigned)correct, (unsigned)balanced(&before, &after));
            if (!ok) ++failures;
            else if ((xrtFileExists(target) && !xrtFileDelete(target)) || !xrtDirRemove(directory)) return 2;
            if (!triggered) { completed = true; break; }
        }
        if (!completed) ++failures;
    }
    xrtValueRelease(document);
    if (!failures && !xrtDirRemove(root)) return 2;
    printf("bson save cleanup: %u cases, %u allocation faults, %u failures; root=%s\n",
        cases, faults, failures, root);
    xrtFree(root);
    return failures ? 1 : 0;
}
