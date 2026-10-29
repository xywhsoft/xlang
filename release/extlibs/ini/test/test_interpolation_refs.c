/* 插值中的空键是表达式错误，不是 OOM；首个点之后的内容仍属于键名。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

static int check(bool condition, const char* message)
{
    if (!condition) fprintf(stderr, "INI references: %s\n", message);
    return condition ? 1 : 0;
}

int main(void)
{
    static const struct { const char* expression; const char* expected; } valid[] = {
        {"${key}", "local"}, {"${.key}", "global"}, {"${s.key}", "local"},
        {"${s..}", "dot"}, {"${s.a.b}", "dotted"}, {"${...}", "global-dots"},
        {"${s.empty}", ""}, {"pre${s.empty}post", "prepost"},
        {"${key}}literal", "local}literal"}
    };
    static const struct { const char* expression; int error; } invalid[] = {
        {"${s.}", XRT_INI_ERROR_INTERPOLATION}, {"${.}", XRT_INI_ERROR_INTERPOLATION},
        {"${}", XRT_INI_ERROR_INTERPOLATION}, {"prefix${s.}suffix", XRT_INI_ERROR_INTERPOLATION},
        {"${s.key", XRT_INI_ERROR_INTERPOLATION}, {"${", XRT_INI_ERROR_INTERPOLATION},
        {"${missing}", XRT_INI_ERROR_NOT_FOUND}, {"${other.key}", XRT_INI_ERROR_NOT_FOUND},
        {"${cycle}", XRT_INI_ERROR_INTERPOLATION}
    };
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    xrtiniconfig* config = xrtIniCreate(&options);
    int ok = check(config && xrtIniSet(config, (str)"", (str)"key", (str)"global") &&
        xrtIniSet(config, (str)"", (str)"..", (str)"global-dots") &&
        xrtIniSet(config, (str)"s", (str)"key", (str)"local") &&
        xrtIniSet(config, (str)"s", (str)".", (str)"dot") &&
        xrtIniSet(config, (str)"s", (str)"a.b", (str)"dotted") &&
        xrtIniSet(config, (str)"s", (str)"empty", (str)"") &&
        xrtIniSet(config, (str)"s", (str)"cycle", (str)"${cycle}"), "cannot prepare reference targets");

    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
        ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)valid[i].expression), "cannot set reference");
        str result = xrtIniGet(config, (str)"s", (str)"value");
        ok &= check(result && !xrtGetError() && strcmp(result, valid[i].expected) == 0,
            "a valid global/dotted/empty-value reference changed meaning");
        xrtFree(result);
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)invalid[i].expression), "cannot set invalid expression");
        str result = xrtIniGet(config, (str)"s", (str)"value");
        if (result || xrtIniError(config)->iCode != invalid[i].error) {
            fprintf(stderr, "expression=%s expected=%d actual=%d\n", invalid[i].expression,
                invalid[i].error, xrtIniError(config)->iCode);
            ok = 0;
        }
        xrtFree(result);
        result = xrtIniGet(config, (str)"s", (str)"key");
        ok &= check(result && !xrtGetError() && xrtIniError(config)->iCode == XRT_INI_OK &&
            strcmp(result, "local") == 0, "a correct lookup did not clear the previous reference error");
        xrtFree(result);
    }

    /* 真正的分配故障仍须报告 Memory，而不能被表达式检查改写。 */
    ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)"${.key}/${s.a.b}/${s..}"), "cannot prepare OOM sweep");
    unsigned allocation, failures = 0;
    for (allocation = 0; allocation < 128; ++allocation) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        ok &= check(xrtMemDebugFailAfter(allocation), "cannot inject allocation failure");
        str result = xrtIniGet(config, (str)"s", (str)"value");
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ok &= check(!result && xrtIniError(config)->iCode == XRT_INI_ERROR_MEMORY,
                "a real reference allocation failure was misclassified");
            ++failures;
        } else ok &= check(result && !xrtGetError() && strcmp(result, "global/dotted/dot") == 0,
            "reference lookup did not recover after OOM");
        xrtFree(result);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        ok &= check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
            "reference failure left an owned allocation live");
        if (!triggered) break;
    }
    ok &= check(failures && allocation < 128, "allocation sweep did not reach success");
    xrtIniDestroy(config);

    options.bInterpolation = false;
    config = xrtIniCreate(&options);
    ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)"${s.}"), "cannot set a raw expression");
    str result = xrtIniGet(config, (str)"s", (str)"value");
    ok &= check(result && !xrtGetError() && strcmp(result, "${s.}") == 0,
        "disabled interpolation must leave the stored text literal");
    xrtFree(result);
    xrtIniDestroy(config);
    if (ok) puts("ini interpolation references: PASS");
    return ok ? 0 : 1;
}

#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
