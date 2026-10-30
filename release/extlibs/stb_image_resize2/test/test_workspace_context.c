/* 原生扩展 API 的分配上下文也必须沿失败清理路径保留。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include <stdio.h>

static int contextMarker;
static unsigned wrongContext;

static void* workspace_alloc(size_t size, void* context)
{
    if (context != &contextMarker) ++wrongContext;
    return xrtMalloc(size);
}

static void workspace_free(void* memory, void* context)
{
    if (memory && context != &contextMarker) ++wrongContext;
    xrtFree(memory);
}

#define STBIR_MALLOC(size, user_data) workspace_alloc(size, user_data)
#define STBIR_FREE(memory, user_data) workspace_free(memory, user_data)
#define STBIR_NO_SIMD
#define STBIR__SEPARATE_ALLOCATIONS
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../third_party/stb_image_resize2.h"

int main(void)
{
    unsigned char input[7 * 5 * 4] = {0}, output[3 * 2 * 4];
    unsigned failures = 0, points = 0, budget;
    for (budget = 0; budget < 64; ++budget) {
        xmemdebugsnapshot before, after;
        STBIR_RESIZE resize;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        stbir_resize_init(&resize, input, 7, 5, 0, output, 3, 2, 0, STBIR_RGBA, STBIR_TYPE_UINT8);
        stbir_set_user_data(&resize, &contextMarker);
        if (!xrtMemDebugFailAfter(budget)) return 2;
        int result = stbir_build_samplers(&resize);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ++points;
            if (result || !xrtGetError() || xrtErrorKind(xrtGetError()) != XERR_MEMORY) ++failures;
        } else if (!result) ++failures;
        stbir_free_samplers(&resize);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
            before.InvalidFreeCount != after.InvalidFreeCount || before.UseAfterFreeCount != after.UseAfterFreeCount ||
            before.OverflowCount != after.OverflowCount || before.UnderflowCount != after.UnderflowCount) ++failures;
        if (!triggered) break;
    }
    if (!points || budget >= 64) ++failures;
    if (failures || wrongContext) {
        fprintf(stderr, "stb workspace: %u failed budgets, %u wrong allocator contexts\n", failures, wrongContext);
        return 1;
    }
    printf("stb workspace context: %u OOM points PASS\n", points);
    return 0;
}
