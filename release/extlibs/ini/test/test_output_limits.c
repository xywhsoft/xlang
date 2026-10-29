/* 输出预算必须在扩容/复制之前生效，与是否启用插值无关。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

static int check(bool condition, const char* message)
{
    if (!condition) fprintf(stderr, "INI output: %s\n", message);
    return condition ? 1 : 0;
}

static int testLiteralLimits(void)
{
    static const char* const values[] = {"", "1234567", "12345678", "123456789"};
    int ok = 1;
    for (unsigned interpolation = 0; interpolation < 2; ++interpolation) {
        xrtinioptions options;
        xrtIniOptionsInit(&options);
        options.bInterpolation = interpolation != 0;
        options.tLimits.iMaxOutputBytes = 8;
        xrtiniconfig* config = xrtIniCreate(&options);
        for (unsigned i = 0; i < 4; ++i) {
            ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)values[i]), "cannot set a literal");
            str result = xrtIniGet(config, (str)"s", (str)"value");
            ok &= check(i < 3 ? result && !xrtGetError() && strcmp(result, values[i]) == 0 :
                !result && xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT,
                "owned literal results must respect the same byte limit as resolved text");
            xrtFree(result);
        }
        xrtIniDestroy(config);
    }
    return ok;
}

static int testResolvedLimits(void)
{
    static const char* const values[] = {
        "${four}${four}", "${four}${four}x", "x${four}${four}",
        "${four}x${four}", "${other.four}${four}", "${other.four}x${four}"
    };
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    options.tLimits.iMaxOutputBytes = 8;
    xrtiniconfig* config = xrtIniCreate(&options);
    int ok = check(xrtIniSet(config, (str)"s", (str)"four", (str)"1234") &&
        xrtIniSet(config, (str)"other", (str)"four", (str)"1234"), "cannot set reference targets");
    for (unsigned i = 0; i < 6; ++i) {
        ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)values[i]), "cannot set a reference");
        str result = xrtIniGet(config, (str)"s", (str)"value");
        ok &= check(i == 0 || i == 4 ? result && !xrtGetError() && strcmp(result, "12341234") == 0 :
            !result && xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT,
            "prefixes, references and suffixes must share one accumulated budget");
        xrtFree(result);
        result = xrtIniGet(config, (str)"s", (str)"four");
        ok &= check(result && !xrtGetError() && strcmp(result, "1234") == 0,
            "a successful read must recover after an output limit error");
        xrtFree(result);
    }
    xrtIniDestroy(config);
    return ok;
}

static int testNoOverlimitGrowth(void)
{
    char large[8193];
    memset(large, 'x', sizeof(large) - 1);
    large[sizeof(large) - 1] = 0;
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    options.tLimits.iMaxOutputBytes = 8;
    xrtiniconfig* config = xrtIniCreate(&options);
    int ok = check(xrtIniSet(config, (str)"s", (str)"large", large), "cannot set a large literal");
    for (unsigned prefix = 0; prefix < 2; ++prefix) {
        ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)(prefix ? "A${large}" : "${large}")),
            "cannot set a large reference");
        xmemdebugsnapshot before, after;
        xrtMemDebugSnapshot(&before);
        str result = xrtIniGet(config, (str)"s", (str)"value");
        xrtMemDebugSnapshot(&after);
        ok &= check(!result && xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT,
            "an overlarge reference must report an output limit");
        ok &= check(before.Enabled && before.ReallocCount == after.ReallocCount,
            "the interpolator must reject a known overlarge literal before growing its output");
        xrtFree(result);
    }
    const char* raw = xrtIniGetRaw(config, (str)"s", (str)"large");
    ok &= check(raw && strcmp(raw, large) == 0, "a rejected read must preserve the stored value");
    xrtIniDestroy(config);
    options.tLimits.iMaxOutputBytes = 0;
    for (unsigned interpolation = 0; interpolation < 2; ++interpolation) {
        options.bInterpolation = interpolation != 0;
        config = xrtIniCreate(&options);
        ok &= check(xrtIniSet(config, (str)"s", (str)"large", large), "cannot prepare unlimited text");
        str result = xrtIniGet(config, (str)"s", (str)"large");
        ok &= check(result && !xrtGetError() && strcmp(result, large) == 0, "zero must remove the output limit");
        xrtFree(result);
        if (interpolation) {
            ok &= check(xrtIniSet(config, (str)"s", (str)"value", (str)"A${large}B"),
                "cannot prepare an unlimited reference");
            result = xrtIniGet(config, (str)"s", (str)"value");
            ok &= check(result && !xrtGetError() && strlen(result) == sizeof(large) + 1 &&
                result[0] == 'A' && result[sizeof(large)] == 'B', "unlimited expansion must still work");
            xrtFree(result);
        }
        xrtIniDestroy(config);
    }
    return ok;
}

static int testUnicodeAndUnlimited(void)
{
    const char text[] = "\xe4\xbd\xa0\xe5\xa5\xbd";
    static const unsigned limits[] = {0, 5, 6};
    int ok = 1;
    for (unsigned i = 0; i < sizeof(limits) / sizeof(limits[0]); ++i) {
        unsigned limit = limits[i];
        xrtinioptions options;
        xrtIniOptionsInit(&options);
        options.bInterpolation = true;
        options.tLimits.iMaxOutputBytes = limit;
        xrtiniconfig* config = xrtIniCreate(&options);
        ok &= check(xrtIniSet(config, (str)"s", (str)"text", (str)text) &&
            xrtIniSet(config, (str)"s", (str)"value", (str)"${text}"), "cannot set Unicode text");
        str result = xrtIniGet(config, (str)"s", (str)"value");
        ok &= check(limit == 5 ? !result && xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT :
            result && !xrtGetError() && strcmp(result, text) == 0, "output limits count UTF-8 bytes; zero is unlimited");
        xrtFree(result);
        xrtIniDestroy(config);
    }
    return ok;
}

static int testAllocationFailures(void)
{
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bInterpolation = true;
    options.tLimits.iMaxOutputBytes = 8;
    xrtiniconfig* config = xrtIniCreate(&options);
    int ok = check(xrtIniSet(config, (str)"s", (str)"four", (str)"1234") &&
        xrtIniSet(config, (str)"s", (str)"value", (str)"${four}${four}"), "cannot prepare exact-limit text");
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
                "an allocation failure for valid bounded text must remain a memory error");
            ++failures;
        } else ok &= check(result && !xrtGetError() && strcmp(result, "12341234") == 0,
            "bounded text did not recover after the allocation sweep");
        xrtFree(result);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        ok &= check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
            "a bounded expansion failure leaked memory");
        if (!triggered) break;
    }
    ok &= check(failures && allocation < 128, "the allocation sweep did not reach normal completion");
    xrtIniDestroy(config);
    return ok;
}

int main(void)
{
    int ok = testLiteralLimits();
    ok &= testResolvedLimits();
    ok &= testNoOverlimitGrowth();
    ok &= testUnicodeAndUnlimited();
    ok &= testAllocationFailures();
    xrtClearError();
    if (ok) puts("ini incremental output limits: PASS");
    return ok ? 0 : 1;
}

#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
