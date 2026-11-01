/* 高层允许的插值深度不能依赖宿主线程剩余的 C 栈大小。 */
#define XRT_MODULE_ALL
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks, failures, injected;

static void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "INI heap frames: %s\n", message); }
}

static bool value(xrtiniconfig* config, const char* key, const char* expected, int error)
{
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    char* text = xrtIniGet(config, (str)"s", (str)key);
    bool ok = xrtIniError(config)->iCode == error &&
        (expected ? text && strcmp(text, expected) == 0 : text == NULL);
    if (expected) ok = ok && !xrtGetError();
    xrtFree(text);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    // 深层成功、循环、缺失和限额失败都必须释放整个查询的帧与引用。
    ok = ok && before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
        before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
        before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount &&
        before.UseAfterFreeCount == after.UseAfterFreeCount;
    return ok;
}

static xrtiniconfig* chain(unsigned count, unsigned depth)
{
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    options.tLimits.iMaxDepth = depth;
    xrtiniconfig* config = xrtIniCreate(&options);
    if (!config) return NULL;
    for (unsigned i = 0; i < count; ++i) {
        char key[16], value[24];
        snprintf(key, sizeof(key), "k%05u", i);
        if (i + 1 == count) strcpy(value, "OK");
        else snprintf(value, sizeof(value), "${k%05u}", i + 1);
        if (!xrtIniSet(config, (str)"s", key, value)) { xrtIniDestroy(config); return NULL; }
    }
    return config;
}

static void testDeep(unsigned count)
{
    xrtiniconfig* config = chain(count, count + 1);
    check(config != NULL, "create deep chain");
    if (!config) return;
    printf("INI interpolation depth begin: %u\n", count);
    fflush(stdout);
    check(value(config, "k00000", "OK", XRT_INI_OK), "deep acyclic chain");
    char last[16];
    snprintf(last, sizeof(last), "k%05u", count - 1);
    check(xrtIniSet(config, (str)"s", last, (str)"${missing}"), "set deep missing reference");
    check(value(config, "k00000", NULL, XRT_INI_ERROR_NOT_FOUND), "deep missing reference classification");
    check(xrtIniSet(config, (str)"s", last, (str)"${k00000}"), "set deep cycle");
    check(value(config, "k00000", NULL, XRT_INI_ERROR_INTERPOLATION) &&
        strstr(xrtIniError(config)->sMessage, "cycle"), "deep cycle without host stack growth");
    check(xrtIniSet(config, (str)"s", last, (str)"OK") &&
        value(config, "k00000", "OK", XRT_INI_OK), "same config recovers after deep failures");
    xrtIniDestroy(config);
}

static void testLimits(void)
{
    static const unsigned depths[] = {2, 3, 16, 64};
    for (size_t i = 0; i < sizeof(depths) / sizeof(depths[0]); ++i) {
        for (unsigned exact = 0; exact < 2; ++exact) {
            unsigned count = depths[i];
            xrtiniconfig* config = chain(count, count - 1 + exact);
            check(config && value(config, "k00000", exact ? "OK" : NULL,
                exact ? XRT_INI_OK : XRT_INI_ERROR_INTERPOLATION), "exact depth boundary");
            check(config && value(config, "k00000", exact ? "OK" : NULL,
                exact ? XRT_INI_OK : XRT_INI_ERROR_INTERPOLATION), "repeated depth boundary");
            xrtIniDestroy(config);
        }
    }
    for (unsigned count = 64; count <= 65; ++count) {
        xrtiniconfig* config = chain(count, 0);
        check(config && value(config, "k00000", count == 64 ? "OK" : NULL,
            count == 64 ? XRT_INI_OK : XRT_INI_ERROR_INTERPOLATION), "zero option retains default depth 64");
        xrtIniDestroy(config);
    }
}

static void testBranches(void)
{
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    xrtiniconfig* config = xrtIniCreate(&options);
    check(config && xrtIniSet(config, (str)"", (str)"global", (str)"G") &&
        xrtIniSet(config, (str)"s", (str)"local", (str)"L") &&
        xrtIniSet(config, (str)"s", (str)"inner", (str)"${local}!") &&
        xrtIniSet(config, (str)"s", (str)"outer", (str)"A${s.inner}B${.global}C${s.inner}D"), "prepare branch owners");
    if (!config) return;
    check(value(config, "outer", "AL!BGCL!D", XRT_INI_OK), "resume parent after owned-section and global branches");
    for (unsigned budget = 0; budget < 128; ++budget) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        check(xrtMemDebugFailAfter(budget), "inject frame allocation failure");
        char* text = xrtIniGet(config, (str)"s", (str)"outer");
        bool failed = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        check(failed ? !text && xrtIniError(config)->iCode == XRT_INI_ERROR_MEMORY :
            text && strcmp(text, "AL!BGCL!D") == 0, "frame OOM result and classification");
        xrtFree(text);
        check(value(config, "outer", "AL!BGCL!D", XRT_INI_OK), "immediate recovery after frame OOM");
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.OverflowCount == after.OverflowCount && before.UnderflowCount == after.UnderflowCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount, "frame/reference cleanup balance");
        if (!failed) break;
        ++injected;
    }
    check(injected > 0 && injected < 128, "OOM sweep reached success");
    xrtIniDestroy(config);
    options.tLimits.iMaxOutputBytes = 1;
    config = xrtIniCreate(&options);
    check(config && xrtIniSet(config, (str)"s", (str)"empty", (str)"") &&
        xrtIniSet(config, (str)"s", (str)"value", (str)"X${empty}Y") &&
        value(config, "value", NULL, XRT_INI_ERROR_LIMIT), "output limit across an empty child frame");
    check(config && xrtIniSet(config, (str)"s", (str)"value", (str)"X${empty}") &&
        value(config, "value", "X", XRT_INI_OK), "recover at exact output limit");
    xrtIniDestroy(config);
}

int main(int argc, char** argv)
{
    unsigned count = argc == 2 ? (unsigned)strtoul(argv[1], NULL, 10) : 8192;
    if (count < 2 || count > 16384) return 2;
    if (!(xrtMemDebugEnabled() || xrtMemDebugEnable(true))) return 1;
    xrtSetErrorInfo(XERR_VALUE, "ini.test", 0, "warmup");
    xrtClearError();
    testDeep(count);
    testLimits();
    testBranches();
    printf("INI interpolation heap frames: %u checks, %u OOM, %u failures\n", checks, injected, failures);
    return failures ? 1 : 0;
}

/* 独立红绿对照只替换模型来源，测试逻辑、解析器和写出器保持同一份。 */
#ifndef XINI_TEST_MODEL
#define XINI_TEST_MODEL "../xrt_ini_model.c"
#endif
#include XINI_TEST_MODEL
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
