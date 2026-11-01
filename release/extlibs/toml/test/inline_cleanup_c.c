/* 内联表在每个写出边界失败后，快照、输入与输出必须各自完整释放。 */
#define XRT_MODULE_ALL
#include "../include/xrt_toml.h"
#include <stdio.h>
#include <string.h>

static unsigned checks, failures, quotas, faults;

static void require(bool ok, cstr message)
{
    ++checks;
    if (!ok) { ++failures; fprintf(stderr, "toml inline cleanup: %s\n", message); }
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

static bool same(const xbuffer* a, const xbuffer* b)
{
    return a != NULL && b != NULL && a->Size == b->Size && memcmp(a->Data, b->Data, a->Size) == 0;
}

static void attempt(cstr source, const xbuffer* expected, uint64 limit, int budget)
{
    xmemdebugsnapshot before;
    xrttomloptions options;
    xrttomlerror error;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    xvalue* input = xrtTomlParseMemory(source, strlen(source), NULL, &error);
    require(input != NULL && error.iCode == XRT_TOML_OK, "prepare input");
    if (input == NULL) return;
    xrtTomlOptionsInit(&options); options.tLimits.iMaxOutputBytes = limit;
    if (budget >= 0) require(xrtMemDebugFailAfter((uint64)budget), "arm OOM");
    xbuffer* output = xrtTomlStringify(input, &options, &error);
    bool triggered = budget >= 0 && xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    if (triggered) {
        ++faults;
        require(output == NULL && error.iCode == XRT_TOML_ERROR_MEMORY, "OOM diagnosis");
    } else if (limit != 0 && limit < expected->Size) {
        require(output == NULL && error.iCode == XRT_TOML_ERROR_LIMIT, "short quota diagnosis");
    } else require(same(output, expected) && error.iCode == XRT_TOML_OK, "exact or unrestricted output");
    xrtBufferDestroy(output);

    /* 重试仍使用原对象；只检查返回码不足以发现 backing 被悄悄保留。 */
    output = xrtTomlStringify(input, NULL, &error);
    require(same(output, expected) && error.iCode == XRT_TOML_OK, "same-input retry bytes");
    xvalue* parsed = output != NULL ? xrtTomlParseMemory(output->Data, output->Size, NULL, &error) : NULL;
    require(parsed != NULL && xrtValueEqual(input, parsed), "roundtrip input unchanged");
    xrtValueRelease(parsed); xrtBufferDestroy(output); xrtValueRelease(input);
    require(balanced(before), "seven memory counters after input release");
}

static void document(cstr source, size_t brace)
{
    xrttomlerror error;
    xvalue* input = xrtTomlParseMemory(source, strlen(source), NULL, &error);
    xbuffer* expected = input != NULL ? xrtTomlStringify(input, NULL, &error) : NULL;
    require(expected != NULL, "reference output");
    xrtValueRelease(input);
    if (expected == NULL) return;
    if (brace != 0) require(expected->Size > brace && expected->Data[brace] == '{', "brace at growth boundary");
    /* 小文档逐字节截断；大文档只测真实扩容点附近及完整边界。 */
    if (brace == 0) {
        for (size_t limit = 1; limit <= expected->Size; ++limit) {
            attempt(source, expected, (uint64)limit, -1); ++quotas;
        }
    } else {
        size_t limits[] = {brace - 1u, brace, brace + 1u, expected->Size - 1u, expected->Size};
        for (size_t i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
            attempt(source, expected, (uint64)limits[i], -1); ++quotas;
        }
    }
    /* 覆盖初始缓冲、快照以及数据扩容的实际分配，直到无故障预算。 */
    unsigned initial = faults;
    for (int budget = 0; budget < 128; ++budget) {
        unsigned previous = faults;
        attempt(source, expected, 0u, budget);
        if (faults == previous) break;
        require(budget < 127, "allocation budget reached success");
    }
    require(faults > initial, "actual OOM covered");
    xrtBufferDestroy(expected);
}

int main(void)
{
    static const char* sources[] = {
        "x = [{a = 1}, 0]\n",
        "x = [0, {}, {a = {b = 2}}, 1]\n",
        "x = [[{a = 1}, 0], false]\n",
        "\"中文\" = [{\"空 格\" = \"你好\\nworld\"}, 1]\n",
        "[[rows]]\nx = [{a = [1, {b = 2}, 3]}, 0]\n"
    };
    if (!xrtMemDebugEnabled()) { fputs("TOML cleanup requires memory debugging\n", stderr); return 2; }
    xrtSetErrorInfo(XERR_STATE, "test.toml", 0, "warm diagnostics"); xrtClearError();
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) document(sources[i], 0u);

    /* 从公开空文档结果读取初始容量，不假设 Reserve(1024) 恰好分配 1024。 */
    xrttomlerror error;
    xvalue* empty = xrtValueObject();
    xbuffer* probe = xrtTomlStringify(empty, NULL, &error);
    size_t capacity = probe != NULL ? probe->Capacity : 0u;
    xrtBufferDestroy(probe); xrtValueRelease(empty);
    require(capacity >= 1024u && capacity <= 1024u * 1024u, "initial capacity probe");
    if (capacity >= 1024u && capacity <= 1024u * 1024u) {
        size_t length = capacity - 9u;
        char* large = xrtMalloc(capacity + 64u);
        require(large != NULL, "growth fixture allocation");
        if (large != NULL) {
            memcpy(large, "x = [\"", 6u); memset(large + 6u, 'z', length);
            memcpy(large + 6u + length, "\", {a = 1}, 0]\n", sizeof("\", {a = 1}, 0]\n"));
            document(large, capacity); xrtFree(large);
        }
    }
    printf("toml inline cleanup: %u checks, %u quotas, %u OOM, %u failures\n", checks, quotas, faults, failures);
    return failures != 0;
}

#include "../../_tomlc17/tomlc17_xrt.c"
#include "../xrt_toml_types.c"
#include "../xrt_toml_parser.c"
#include "../xrt_toml_writer.c"
