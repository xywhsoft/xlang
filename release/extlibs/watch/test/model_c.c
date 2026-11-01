/* 只替换元数据取样；路径分配、事件分配、槽位、游标及销毁均跑生产代码。 */
#define XRT_MODULE_DIR
#define XRT_MODULE_PATH
#define XRT_MODULE_HASH64
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../watch_native.h"
#include <stdio.h>
#include <string.h>

enum { FILES = 16, SLOTS = 32, SEEDS = 64, STEPS = 128 };
typedef struct { bool present, denied; uint64 version; } model_file;
typedef struct { bool active, present; unsigned file; uint64 version; const char* borrowed; } model_slot;
typedef struct { model_slot slots[SLOTS]; unsigned extent, cursor, count; } model_watcher;
static model_file files[FILES];
static char names[FILES][48];
static xerror *missing_error, *io_error;
static unsigned checks, failures, current_seed, current_step, commands, oom, io_failures, seeds_run, steps_run;

static void require(bool ok, cstr label)
{
    ++checks;
    if (!ok) {
        if (failures < 12) fprintf(stderr, "watch model seed=%u step=%u: %s\n", current_seed, current_step, label);
        ++failures;
    }
}

static bool sample_stat(cstr path, bool follow, xfileinfo* info)
{
    const char *leaf = strrchr(path, '/'), *back = strrchr(path, '\\');
    (void)follow;
    if (back != NULL && (leaf == NULL || back > leaf)) leaf = back;
    leaf = leaf != NULL ? leaf + 1 : path;
    for (unsigned i = 0; i < FILES; ++i) {
        if (strcmp(leaf, names[i]) != 0) continue;
        if (files[i].denied || !files[i].present) {
            xrtSetError(files[i].denied ? io_error : missing_error); return false;
        }
        memset(info, 0, sizeof(*info));
        info->Type = XFILE_TYPE_FILE; info->Available = XFILE_INFO_SIZE;
        info->Size = files[i].version;
        return true;
    }
    xrtSetError(io_error); return false;
}

#define xrtPathStat sample_stat
#include "../watch_native.c"
#undef xrtPathStat

static uint32 next(uint32* seed)
{
    *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5;
    return *seed;
}

static void verify(void* watcher, const model_watcher* model)
{
    require(xwatch_count(watcher) == (int64)model->count, "active count");
    for (unsigned i = 0; i < model->extent; ++i) {
        cstr path = xwatch_path(watcher, i);
        if (model->slots[i].active) {
            require(path == model->slots[i].borrowed && strcmp(path, names[model->slots[i].file]) == 0, "borrowed path after unrelated mutations");
        } else require(path != NULL && *path == 0, "removed slot has no path");
    }
}

static void add(void* watcher, model_watcher* model, unsigned file, int budget)
{
    unsigned slot = 0;
    while (slot < model->extent && model->slots[slot].active) ++slot;
    if (slot == SLOTS) return;
    if (budget >= 0) require(xrtMemDebugFailAfter((uint64)budget), "arm add OOM");
    int64 index = xwatch_add(watcher, names[file]);
    bool triggered = budget >= 0 && xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (triggered) {
        ++oom;
        require(index == -1 && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "failed add is MemoryError");
    } else if (files[file].denied) {
        ++io_failures;
        require(index == -1 && xrtGetError() == io_error, "failed add preserves IO cause");
    } else {
        require(index == (int64)slot && xrtGetError() == NULL, "first free stable slot");
        if (index >= 0 && index < SLOTS) {
            model_slot* entry = &model->slots[slot];
            entry->active = true; entry->file = file; entry->present = files[file].present;
            entry->version = files[file].version; entry->borrowed = xwatch_path(watcher, index);
            if (slot == model->extent) ++model->extent;
            ++model->count;
        }
    }
    verify(watcher, model); ++commands;
}

static void remove_slot(void* watcher, model_watcher* model, unsigned slot)
{
    bool expected = slot < model->extent && model->slots[slot].active;
    require(xwatch_remove(watcher, slot) == expected && xrtGetError() == NULL, "remove including holes and invalid indices");
    if (expected) { memset(&model->slots[slot], 0, sizeof(model_slot)); --model->count; }
    verify(watcher, model); ++commands;
}

/* 模型只认识“存在/版本”和已确认事件，不复制生产元数据比较或哈希实现。 */
static void poll(void* watcher, model_watcher* model, int budget)
{
    char expected[96] = "";
    int delivered = -1;
    bool denied = false;
    for (unsigned offset = 0; offset < model->extent; ++offset) {
        unsigned slot = (model->cursor + offset) % model->extent;
        model_slot* entry = &model->slots[slot];
        if (!entry->active) continue;
        model_file* file = &files[entry->file];
        if (file->denied) { denied = true; break; }
        if (entry->present == file->present && (!file->present || entry->version == file->version)) continue;
        cstr kind = !entry->present ? "CREATED" : !file->present ? "DELETED" : "MODIFIED";
        snprintf(expected, sizeof(expected), "%s:%s", kind, names[entry->file]);
        delivered = (int)slot; break;
    }
    if (budget >= 0) require(xrtMemDebugFailAfter((uint64)budget), "arm poll OOM");
    char* event = xwatch_poll(watcher);
    bool triggered = budget >= 0 && xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (triggered) {
        ++oom;
        require(event == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "failed poll is MemoryError");
    } else if (denied) {
        ++io_failures;
        require(event == NULL && xrtGetError() == io_error, "failed poll preserves IO cause");
    } else {
        require(event != NULL && strcmp(event, expected) == 0 && xrtGetError() == NULL, "round-robin event matches model");
        if (delivered >= 0) {
            model_slot* entry = &model->slots[delivered];
            entry->present = files[entry->file].present; entry->version = files[entry->file].version;
            model->cursor = ((unsigned)delivered + 1u) % model->extent;
        }
    }
    xrtFree(event); verify(watcher, model); ++commands;
}

static bool balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;
    xrtClearError(); xrtMemDebugSnapshot(&after);
    return before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount;
}

static void scenario(unsigned seed)
{
    model_watcher model = {0};
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    memset(files, 0, sizeof(files));
    current_seed = seed; current_step = 0;
    void* watcher = xwatch_create();
    require(watcher != NULL, "create");
    if (watcher == NULL) return;
    uint32 random = UINT32_C(0x9e3779b9) ^ seed;
    for (unsigned i = 0; i < seed % (SLOTS + 1u); ++i) add(watcher, &model, i % FILES, -1);
    if (seed == 2) {
        files[0].present = files[1].present = true;
        poll(watcher, &model, 0); poll(watcher, &model, -1);
        ++files[0].version;
        poll(watcher, &model, -1); poll(watcher, &model, -1);
    }
    for (current_step = 0; current_step < STEPS && failures == 0; ++current_step) {
        ++steps_run;
        unsigned file = next(&random) % FILES;
        switch (next(&random) % 6u) {
        case 0: add(watcher, &model, file, (int)(next(&random) % 9u) - 1); break;
        case 1: remove_slot(watcher, &model, next(&random) % (SLOTS + 2u)); break;
        case 2:
            files[file].present = (next(&random) & 1u) != 0;
            files[file].version = (uint64)next(&random) << 32;
            files[file].version |= next(&random);
            ++commands; break;
        case 3: poll(watcher, &model, -1); break;
        case 4: poll(watcher, &model, (int)(next(&random) % 3u)); break;
        case 5: files[file].denied = (next(&random) & 1u) != 0; ++commands; break;
        }
    }
    for (unsigned i = 0; i < FILES; ++i) files[i].denied = false;
    for (unsigned i = 0; i <= model.extent && failures == 0; ++i) poll(watcher, &model, -1);
    xwatch_destroy(watcher);
    require(balanced(before), "seven memory counters after watcher retirement");
}

int main(void)
{
    if (!xrtMemDebugEnabled()) return 2;
    xmemdebugsnapshot initial;
    xrtSetErrorInfo(XERR_IO, "watch.model", 0, "warm diagnostics"); xrtClearError();
    xrtMemDebugSnapshot(&initial);
    missing_error = xrtErrorCreate(XERR_NOT_FOUND, "watch.model", 77, "missing");
    io_error = xrtErrorCreate(XERR_IO, "watch.model", 78, "sample failed");
    if (missing_error == NULL || io_error == NULL) {
        xrtErrorFree(missing_error); xrtErrorFree(io_error); return 2;
    }
    xrtSetError(io_error); xrtClearError();
    for (unsigned i = 0; i < FILES; ++i) snprintf(names[i], sizeof(names[i]), "watch-路径-%u", i);
    for (unsigned seed = 0; seed < SEEDS && failures == 0; ++seed) { scenario(seed); ++seeds_run; }
    xrtClearError(); xrtErrorFree(missing_error); xrtErrorFree(io_error);
    missing_error = io_error = NULL;
    require(balanced(initial), "model error-owner retirement");
    require(oom != 0 && io_failures != 0, "actual OOM and IO paths covered");
    require(seeds_run == SEEDS && steps_run == SEEDS * STEPS, "all generated sequences completed");
    printf("watch model: %u seeds, %u steps, %u commands, %u checks, %u OOM, %u IO, %u failures\n",
        seeds_run, steps_run, commands, checks, oom, io_failures, failures);
    return failures != 0;
}
