/* 直接输入词法语料，不以 quote/split 互相验证；独立参照见 check_split_records.py。 */
#define XRT_MODULE_MEMORY_DEBUG
#include "../include/xrt_shellwords.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned cases, failures, oom_paths, oom_failures;
static bool records;

static bool balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    return before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        before->InvalidFreeCount == after.InvalidFreeCount && before->DoubleFreeCount == after.DoubleFreeCount &&
        before->OverflowCount == after.OverflowCount && before->UnderflowCount == after.UnderflowCount &&
        before->UseAfterFreeCount == after.UseAfterFreeCount;
}

static void hex(cstr data, size_t size)
{
    for (size_t i = 0; i < size; ++i) printf("%02x", (unsigned char)data[i]);
}

static bool recovered(void)
{
    xvalue* value = xrtShellwordsSplit("recovered");
    xstrview text;
    bool ok = xrtGetError() == NULL && xrtValueType(value) == XVALUE_ARRAY &&
        xrtValueCount(value) == 1 && xrtValueGetString(xrtValueArrayGet(value, 0), &text) &&
        text.Size == 9 && memcmp(text.Data, "recovered", 9) == 0;
    xrtValueRelease(value);
    return ok;
}

static void vector(cstr input)
{
    xmemdebugsnapshot before;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    xvalue* value = xrtShellwordsSplit(input);
    bool invalid = xrtValueType(value) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE;
    bool ok = invalid || (xrtValueType(value) == XVALUE_ARRAY && xrtGetError() == NULL);
    if (records) {
        printf("%u\t", cases); hex(input, strlen(input));
        printf("\t%s", invalid ? "E" : "O");
        if (!invalid && ok) for (size_t i = 0; i < xrtValueCount(value); ++i) {
            xstrview text;
            if (!xrtValueGetString(xrtValueArrayGet(value, i), &text)) { ok = false; break; }
            putchar('\t'); hex(text.Data, text.Size);
        }
        putchar('\n');
    }
    xrtValueRelease(value);
    /* 不预先清除错误，确认成功调用主动退休前一次诊断。 */
    ok = recovered() && ok;
    if (!balanced(&before) || !ok) {
        if (failures < 8) fprintf(stderr, "shellwords split vector %u failed\n", cases);
        ++failures;
    }
    ++cases;
}

static void allocation_path(cstr input, bool invalid)
{
    bool completed = false;
    unsigned triggered_count = 0;
    for (uint64 budget = 0; budget < 512; ++budget) {
        xmemdebugsnapshot before;
        xrtClearError(); xrtMemDebugSnapshot(&before);
        if (!xrtMemDebugFailAfter(budget)) { ++failures; return; }
        xvalue* value = xrtShellwordsSplit(input);
        bool triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        bool ok = triggered ? xrtValueType(value) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY :
            invalid ? xrtValueType(value) == XVALUE_NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE :
            xrtValueType(value) == XVALUE_ARRAY && xrtGetError() == NULL;
        xrtValueRelease(value);
        ok = recovered() && ok;
        if (!balanced(&before) || !ok) {
            fprintf(stderr, "shellwords split OOM path=%u budget=%llu failed\n", oom_paths, (unsigned long long)budget);
            ++failures; return;
        }
        if (triggered) { ++triggered_count; ++oom_failures; }
        else { completed = true; break; }
    }
    if (!completed || triggered_count == 0) ++failures;
    ++oom_paths;
}

/* 六种长片段另含换行续接、相邻引号和末尾不完整词；没有固定8K截断。 */
static void long_case(char* input, size_t size, unsigned mode)
{
    const char* body = mode == 3 ? "a\\ b\"c\"'d'" : mode == 4 ? "x " : "a";
    size_t n = 0, length = strlen(body);
    if (mode == 1) input[n++] = '\'';
    if (mode == 2) input[n++] = '"';
    for (size_t i = 0; i < size; ++i) input[n++] = body[i % length];
    if (mode == 1) input[n++] = '\'';
    if (mode == 2) input[n++] = '"';
    if (mode == 5) input[n++] = '\'';
    input[n] = 0;
}

int main(int argc, char** argv)
{
    if (argc > 2 || (argc == 2 && strcmp(argv[1], "--records") != 0)) return 2;
    records = argc == 2;
    if (!xrtMemDebugEnabled()) return 2;
    xrtSetErrorInfo(XERR_STATE, "test.shellwords.split", 0, "warm diagnostic"); xrtClearError();
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    static const char alphabet[] = "a '\"\\";
    char input[16390];
    size_t extent = 1;
    for (size_t length = 0; length <= 6; ++length) {
        for (size_t number = 0; number < extent; ++number) {
            size_t digits = number;
            for (size_t i = 0; i < length; ++i) { input[i] = alphabet[digits % 5]; digits /= 5; }
            input[length] = 0; vector(input);
        }
        extent *= 5;
    }
    for (unsigned byte = 1; byte <= 255; ++byte) { input[0] = (char)byte; input[1] = 0; vector(input); }
    static const char* fixed[] = {
        "\\\n", "a\\\nb", "a \\\n b", "\"\\\n\"", "\"a\\\nb\"", "'a\\\nb'",
        "\"a\\$b\\`c\"", "\"a\\qb\"", "a\r\nb", "a\vb\fc", "a' b'\"c\"", "''\"\"",
        "head '' tail 'broken", "head \\", "\"a\\\"", "你好 '世界'", "#not-a-comment ; | $HOME"
    };
    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); ++i) vector(fixed[i]);
    static const size_t lengths[] = {0,1,31,32,33,255,4096,16384};
    for (unsigned mode = 0; mode < 6; ++mode) for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
        long_case(input, lengths[i], mode); vector(input);
    }
    if (!records) {
        for (unsigned mode = 0; mode < 6; ++mode) {
            long_case(input, mode == 4 ? 64 : 4096, mode);
            allocation_path(input, mode == 5 || mode == 3); /* 4096字节混合模式在引号中途结束。 */
        }
        allocation_path("first '' \"\" 'two words' tail", false);
        allocation_path("first '' \"\" tail \\", true);
        allocation_path("", false);
    }
    if (!balanced(&before) || cases != 19851 || (!records && oom_paths != 9)) ++failures;
    if (!records) printf("shellwords split matrix: %u vectors, %u OOM paths, %u OOM, %u failures\n", cases, oom_paths, oom_failures, failures);
    return failures != 0 || ferror(stdout) ? 1 : 0;
}

#include "../shellwords_native.c"
