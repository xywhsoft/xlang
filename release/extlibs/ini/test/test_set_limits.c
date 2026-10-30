/* 修改入口与解析入口共享单项字节限额；失败不能改变原配置。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
static unsigned allocationFailures;

static void check(bool condition, const char* message)
{
    if (!condition) {
        ++failures;
        fprintf(stderr, "INI set limits: %s\n", message);
    }
}

static void testItemLimits(void)
{
    static const char* const samples[2][3] = {
        {"abcde", "abcdef", "abcdefg"},
        {"\xe4\xbd\xa0" "ab", "\xe4\xbd\xa0\xe5\xa5\xbd", "\xe4\xbd\xa0\xe5\xa5\xbd" "x"}
    };
    for (unsigned sensitive = 0; sensitive < 2; ++sensitive) {
        for (unsigned field = 0; field < 3; ++field) {
            for (unsigned unicode = 0; unicode < 2; ++unicode) {
                for (unsigned size = 0; size < 3; ++size) {
                    for (unsigned layout = 0; layout < 3; ++layout) {
                        xrtinioptions options;
                        xrtIniOptionsInit(&options);
                        options.bCaseSensitive = sensitive != 0;
                        options.tLimits.iMaxItemBytes = 6;
                        xrtiniconfig* config = xrtIniCreate(&options);
                        const char* seedSection = layout == 2 ? "" : (sensitive ? "s" : "S");
                        const char* seedKey = sensitive ? "k" : "K";
                        if (layout) check(xrtIniSet(config, (str)seedSection, (str)seedKey, (str)"old"),
                            "cannot prepare a named/global entry");
                        const char* borrowed = xrtIniGetRaw(config, (str)seedSection, (str)seedKey);
                        xbuffer* before = xrtIniStringify(config);
                        uint32 sections = xrtIniSectionCount(config);
                        uint32 keys = xrtIniKeyCount(config, (str)seedSection);
                        const char* section = layout == 2 ? NULL : "s";
                        const char* key = "k";
                        const char* value = "new";
                        if (field == 0) section = samples[unicode][size];
                        if (field == 1) key = samples[unicode][size];
                        if (field == 2) value = samples[unicode][size];
                        bool result = xrtIniSet(config, (str)section, (str)key, (str)value);
                        check(result == (size < 2), "setter did not enforce the inclusive byte limit");
                        if (size == 2) {
                            const xrtinierror* error = xrtIniError(config);
                            check(error && error->iCode == XRT_INI_ERROR_LIMIT &&
                                !error->iLine && !error->iColumn && !error->iOffset,
                                "oversized mutation lost its LIMIT diagnostic");
                            check(xrtIniSectionCount(config) == sections &&
                                xrtIniKeyCount(config, (str)seedSection) == keys &&
                                xrtIniGetRaw(config, (str)seedSection, (str)seedKey) == borrowed,
                                "failed mutation changed counts or invalidated a borrowed value");
                        } else {
                            const char* stored = xrtIniGetRaw(config, (str)(section ? section : ""), (str)key);
                            check(stored && strcmp(stored, value) == 0, "an allowed mutation lost text");
                        }
                        xbuffer* after = xrtIniStringify(config);
                        if (size == 2) check(before && after && before->Size == after->Size &&
                            memcmp(before->Data, after->Data, before->Size) == 0,
                            "failed mutation changed canonical text, ordering or spelling");
                        xrtiniconfig* again = after ? xrtIniParseMemory(after->Data, after->Size, &options) : NULL;
                        check(again && xrtIniError(again)->iCode == XRT_INI_OK,
                            "setter produced text that the same item budget cannot parse");
                        xrtIniDestroy(again);
                        xrtBufferDestroy(after);
                        xrtBufferDestroy(before);
                        xrtIniDestroy(config);
                    }
                }
            }
        }
    }
    /* C 的 0 为不限；空值不消耗单项预算，后续有效写入会清除旧错误。 */
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.tLimits.iMaxItemBytes = 0;
    xrtiniconfig* config = xrtIniCreate(&options);
    check(xrtIniSet(config, (str)"long-section", (str)"long-key", (str)"long-value"),
        "zero item limit must remain unlimited in C");
    check(xrtIniSet(config, (str)"long-section", (str)"long-key", (str)""), "empty values must remain valid");
    check(xrtIniGetRaw(config, (str)"long-section", (str)"long-key") &&
        xrtIniGetRaw(config, (str)"long-section", (str)"long-key")[0] == 0, "empty overwrite was lost");
    xrtIniDestroy(config);
    options.tLimits.iMaxItemBytes = 6;
    config = xrtIniCreate(&options);
    check(!xrtIniSet(config, (str)"s", (str)"k", (str)"toolong"), "oversized value was accepted");
    check(xrtIniSet(config, (str)"s", (str)"k", (str)"") &&
        xrtIniError(config)->iCode == XRT_INI_OK && !xrtGetError(), "valid mutation did not recover");
    check(xrtIniSet(config, (str)"s", (str)"k", (str)"abc\nde"), "multiline value at its byte limit was rejected");
    check(!xrtIniSet(config, (str)"s", (str)"k", (str)"abc\ndef") &&
        xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT &&
        strcmp(xrtIniGetRaw(config, (str)"s", (str)"k"), "abc\nde") == 0,
        "a continuation separator must count toward the stored byte limit");
    xbuffer* output = xrtIniStringify(config);
    xrtiniconfig* again = output ? xrtIniParseMemory(output->Data, output->Size, &options) : NULL;
    check(again && xrtIniError(again)->iCode == XRT_INI_OK &&
        strcmp(xrtIniGetRaw(again, (str)"s", (str)"k"), "abc\nde") == 0,
        "multiline value at its byte limit did not round-trip");
    xrtIniDestroy(again);
    xrtBufferDestroy(output);
    xrtIniDestroy(config);
    options.tLimits.iMaxItemBytes = UINT64_MAX;
    config = xrtIniCreate(&options);
    check(xrtIniSet(config, (str)"s", (str)"k", (str)"value"), "uint64 item budget was truncated");
    xrtIniDestroy(config);
}

static void testAllocationRollback(void)
{
    for (unsigned operation = 0; operation < 3; ++operation) {
        unsigned failuresSeen = 0, budget;
        for (budget = 0; budget < 64; ++budget) {
            xmemdebugsnapshot initial, final;
            xrtClearError();
            xrtMemDebugSnapshot(&initial);
            xrtinioptions options;
            xrtIniOptionsInit(&options);
            options.tLimits.iMaxItemBytes = 6;
            xrtiniconfig* config = xrtIniCreate(&options);
            for (unsigned i = 0; i < 8; ++i) {
                char section[8], key[8];
                snprintf(section, sizeof(section), "s%u", i);
                snprintf(key, sizeof(key), "k%u", i);
                check(xrtIniSet(config, (str)section, (str)"k0", (str)"old"), "cannot fill section table");
                check(xrtIniSet(config, (str)"s0", (str)key, (str)"old"), "cannot fill entry table");
            }
            xbuffer* before = xrtIniStringify(config);
            const char* section = operation == 0 ? "extra" : xrtIniSectionName(config, 0);
            const char* key = operation == 2 ? xrtIniKeyName(config, (str)section, 0) : "more";
            const char* value = operation == 2 ? xrtIniGetRaw(config, (str)section, (str)key) : "new";
            check(xrtMemDebugFailAfter(budget), "cannot inject allocation failure");
            bool result = xrtIniSet(config, (str)section, (str)key, (str)value);
            bool triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                ++failuresSeen;
                ++allocationFailures;
                check(!result && xrtIniError(config)->iCode == XRT_INI_ERROR_MEMORY,
                    "failed allocation did not report MEMORY");
                xbuffer* after = xrtIniStringify(config);
                check(before && after && before->Size == after->Size &&
                    memcmp(before->Data, after->Data, before->Size) == 0,
                    "failed insert/overwrite changed existing data");
                xrtBufferDestroy(after);
                check(xrtIniSet(config, (str)section, (str)key, (str)value) &&
                    xrtIniError(config)->iCode == XRT_INI_OK, "mutation did not recover after OOM");
            } else check(result, "valid mutation failed without an injected allocation failure");
            xrtBufferDestroy(before);
            xrtIniDestroy(config);
            xrtClearError();
            xrtMemDebugSnapshot(&final);
            check(initial.LiveCount == final.LiveCount && initial.LiveBytes == final.LiveBytes &&
                initial.InvalidFreeCount == final.InvalidFreeCount &&
                initial.OverflowCount == final.OverflowCount && initial.UnderflowCount == final.UnderflowCount &&
                initial.UseAfterFreeCount == final.UseAfterFreeCount,
                "mutation rollback leaked an allocation");
            if (!triggered) break;
        }
        check(failuresSeen && budget < 64, "allocation sweep never reached success");
    }
}

int main(void)
{
    testItemLimits();
    testAllocationRollback();
    if (failures) fprintf(stderr, "INI set limits failures: %u\n", failures);
    else printf("ini item limits and mutation rollback: PASS (108 cases, %u allocation failures)\n", allocationFailures);
    return failures ? 1 : 0;
}

/* 同一份回归也能由 xl run/build 直接执行。 */
#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
