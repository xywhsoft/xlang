/* 重复键续行与首字节 BOM 不能在解析/写回中静默改变数据。 */
#define XRT_MODULE_ALL
#include "xrt.h"
#include "../include/xrt_ini.h"
#include <stdio.h>
#include <string.h>

static int check(bool condition, const char* message)
{
    if (!condition) fprintf(stderr, "INI updates: %s\n", message);
    return condition ? 1 : 0;
}

static int testDuplicateContinuation(void)
{
    static const char* const endings[] = {"\n", "\r\n", "\r"};
    int ok = 1;
    /* 九个键跨过条目表扩容点；首/中/末键、节重开、大小写与换行组合独立。 */
    for (unsigned sensitive = 0; sensitive < 2; ++sensitive) {
        for (unsigned section = 0; section < 3; ++section) {
            for (unsigned ending = 0; ending < 3; ++ending) {
                for (unsigned target = 0; target < 9; target += 4) {
                    const char* newline = endings[ending];
                    const char* name = section ? "s" : "";
                    char source[512], key[16], expected[32];
                    size_t size = 0;
                    xrtinioptions options;
                    xrtIniOptionsInit(&options);
                    options.bCaseSensitive = sensitive != 0;
                    options.bStrictDuplicates = false;
                    options.tLimits.iMaxEntries = 9;
                    options.tLimits.iMaxNodes = 10;
                    if (section) size += (size_t)snprintf(source, sizeof(source), "[s]%s", newline);
                    for (unsigned i = 0; i < 9; ++i) {
                        size += (size_t)snprintf(source + size, sizeof(source) - size,
                            "key%u=old%u%s", i, i, newline);
                    }
                    if (section == 2) size += (size_t)snprintf(source + size, sizeof(source) - size,
                        "[%s]%s", sensitive ? "s" : "S", newline);
                    size += (size_t)snprintf(source + size, sizeof(source) - size,
                        "%s%u=new%s    first%s\tsecond%s",
                        sensitive ? "key" : "KEY", target, newline, newline, newline);
                    xrtiniconfig* config = xrtIniParseMemory(source, size, &options);
                    ok &= check(config && xrtIniError(config)->iCode == XRT_INI_OK &&
                        xrtIniKeyCount(config, (str)name) == 9, "duplicate update changed the entry budget");
                    for (unsigned i = 0; i < 9; ++i) {
                        snprintf(key, sizeof(key), "key%u", i);
                        if (i == target) snprintf(expected, sizeof(expected), "new\nfirst\nsecond");
                        else snprintf(expected, sizeof(expected), "old%u", i);
                        const char* value = xrtIniGetRaw(config, (str)name, (str)key);
                        ok &= check(value && strcmp(value, expected) == 0,
                            "a continuation did not follow the actual updated key");
                        ok &= check(xrtIniKeyName(config, (str)name, i) &&
                            strcmp(xrtIniKeyName(config, (str)name, i), key) == 0,
                            "overwriting a key must preserve its original order and spelling");
                    }
                    xbuffer* output = config ? xrtIniStringify(config) : NULL;
                    options.bStrictDuplicates = true;
                    xrtiniconfig* again = output ?
                        xrtIniParseMemory(output->Data, output->Size, &options) : NULL;
                    snprintf(key, sizeof(key), "key%u", target);
                    const char* value = xrtIniGetRaw(again, (str)name, (str)key);
                    ok &= check(again && xrtIniError(again)->iCode == XRT_INI_OK &&
                        value && strcmp(value, "new\nfirst\nsecond") == 0,
                        "canonical output must read back with strict duplicates enabled");
                    xrtIniDestroy(again);
                    xrtBufferDestroy(output);
                    xrtIniDestroy(config);
                    config = xrtIniParseMemory(source, size, &options);
                    ok &= check(config && xrtIniError(config)->iCode == XRT_INI_ERROR_DUPLICATE,
                        "strict mode must still reject the repeated key or section");
                    xrtIniDestroy(config);
                }
            }
        }
    }
    return ok;
}

static int testBomKey(void)
{
    static const char* const keys[] = {"\xef\xbb\xbf" "name", "\xef\xbb\xbf"};
    int ok = 1;
    for (unsigned key = 0; key < 2; ++key) {
        for (unsigned layout = 0; layout < 4; ++layout) {
            const char* section = layout == 3 ? "s" : "";
            xrtiniconfig* config = xrtIniCreate(NULL);
            if (layout == 1) ok &= check(xrtIniSet(config, (str)"s", (str)"other", (str)"1"),
                "cannot prepare named section before global section");
            if (layout == 2) ok &= check(xrtIniSet(config, (str)"", (str)"other", (str)"1"),
                "cannot prepare an ordinary first key");
            ok &= check(xrtIniSet(config, (str)section, (str)keys[key], (str)"value"),
                "cannot create a BOM-prefixed key");
            xbuffer* output = xrtIniStringify(config);
            xrtiniconfig* again = output ? xrtIniParseMemory(output->Data, output->Size, NULL) : NULL;
            const char* value = xrtIniGetRaw(again, (str)section, (str)keys[key]);
            ok &= check(again && xrtIniError(again)->iCode == XRT_INI_OK && value &&
                strcmp(value, "value") == 0 && !xrtIniHas(again, (str)section, (str)(keys[key] + 3)) &&
                xrtIniKeyCount(config, (str)section) == xrtIniKeyCount(again, (str)section),
                "the first key's U+FEFF must not be consumed as the file BOM");
            ok &= check(output && (layout < 2 ? output->Data[0] == '\n' : output->Data[0] != '\n'),
                "only an ambiguous first key needs a leading empty line");
            xrtIniDestroy(again);
            xrtBufferDestroy(output);
            xrtIniDestroy(config);
        }
    }
    for (unsigned allowed = 0; allowed < 2; ++allowed) {
        xrtinioptions options;
        xrtIniOptionsInit(&options);
        size_t needed = strlen(keys[0]) + strlen(" = value\n") + 1;
        options.tLimits.iMaxOutputBytes = needed - 1 + allowed;
        xrtiniconfig* config = xrtIniCreate(&options);
        ok &= check(xrtIniSet(config, (str)"", (str)keys[0], (str)"value"), "cannot prepare output limit");
        xbuffer* output = xrtIniStringify(config);
        ok &= check(allowed ? output && output->Size == needed :
            !output && xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT,
            "the BOM-preserving newline must consume the real output budget");
        xrtBufferDestroy(output);
        xrtIniDestroy(config);
    }
    return ok;
}

static int testEmptyNames(void)
{
    static const char* const sources[] = {"=x", ":x", " \t =x", "[]", "[ ]", "[\t]"};
    int ok = 1;
    for (size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i) {
        xrtiniconfig* config = xrtIniParseMemory(sources[i], strlen(sources[i]), NULL);
        ok &= check(config && xrtIniError(config)->iCode == XRT_INI_ERROR_FORMAT,
            "an empty trimmed key/section is a format error, not a resource limit");
        xrtIniDestroy(config);
    }
    static const char* const oversized[] = {"[abcd]", "abcd=x", "x=abcd", "x=a\n    bcd\n"};
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.tLimits.iMaxItemBytes = 3;
    for (size_t i = 0; i < sizeof(oversized) / sizeof(oversized[0]); ++i) {
        xrtiniconfig* config = xrtIniParseMemory(oversized[i], strlen(oversized[i]), &options);
        ok &= check(config && xrtIniError(config)->iCode == XRT_INI_ERROR_LIMIT,
            "a real section/key/value size overflow must remain a resource limit");
        xrtIniDestroy(config);
    }
    xrtClearError();
    return ok;
}

static int testUpdateAllocationFailures(void)
{
    const char source[] = "[s]\na=old\nb=stable\na=new\n    tail\n";
    xrtinioptions options;
    xrtIniOptionsInit(&options);
    options.bStrictDuplicates = false;
    unsigned allocation, failures = 0;
    int ok = 1;
    for (allocation = 0; allocation < 256; ++allocation) {
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        ok &= check(xrtMemDebugFailAfter(allocation), "cannot inject allocation failure");
        xrtiniconfig* config = xrtIniParseMemory(source, sizeof(source) - 1, &options);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            ok &= check(config ? xrtIniError(config)->iCode == XRT_INI_ERROR_MEMORY :
                xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY,
                "an allocation failure during duplicate/continuation must report memory failure");
            ++failures;
        } else ok &= check(config && xrtIniError(config)->iCode == XRT_INI_OK,
            "the parser did not recover after the failure sweep");
        xrtIniDestroy(config);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        ok &= check(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes,
            "a failed duplicate/continuation parse leaked an allocation");
        if (!triggered) break;
    }
    ok &= check(failures != 0 && allocation < 256, "allocation sweep did not reach normal completion");
    return ok;
}

int main(void)
{
    int ok = testDuplicateContinuation();
    ok &= testBomKey();
    ok &= testEmptyNames();
    ok &= testUpdateAllocationFailures();
    if (ok) puts("ini overwrite and write-back boundaries: PASS");
    return ok ? 0 : 1;
}

/* 可直接 xl run；正式包仍独立编译三个实现文件。 */
#include "../xrt_ini_model.c"
#include "../xrt_ini_parser.c"
#include "../xrt_ini_writer.c"
