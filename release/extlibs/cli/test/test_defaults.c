#define XRT_MODULE_MEMORY_DEBUG
#include "../include/cli_native.h"
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

static xstrview view(const char* text)
{
    return (xstrview){text, strlen(text)};
}

static xvalue* field(xvalue* object, const char* name)
{
    return xrtValueObjectGet(object, view(name));
}

static void text_is(xvalue* value, const char* expected)
{
    xstrview text;
    assert(value && xrtValueGetString(value, &text));
    assert(text.Size == strlen(expected) && !memcmp(text.Data, expected, text.Size));
}

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.InvalidFreeCount == before->InvalidFreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.OverflowCount == before->OverflowCount && after.UnderflowCount == before->UnderflowCount);
}

static xvalue* make_spec(const char* default_text)
{
    xvalue *spec = xrtValueObject(), *options = xrtValueObject(), *defaults = xrtValueObject();
    xvalue *flags = xrtValueArray(), *shorts = xrtValueObject();
    xvalue* shared = xrtValueString(view(default_text));
    assert(spec && options && defaults && flags && shorts && shared);
    assert(xrtValueObjectSetNew(options, view("output"), xrtValueString(view("o"))));
    assert(xrtValueObjectSetNew(options, view("level"), xrtValueString(view("l"))));
    // 两个默认值共享同一不可变标量；结果必须各自持有有效引用。
    assert(xrtValueObjectSetNew(defaults, view("output"), xrtValueRetain(shared)));
    assert(xrtValueObjectSetNew(defaults, view("level"), shared));
    assert(xrtValueArrayAppendNew(flags, xrtValueString(view("verbose"))));
    assert(xrtValueObjectSetNew(shorts, view("verbose"), xrtValueString(view("v"))));
    assert(xrtValueObjectSetNew(spec, view("flags"), flags));
    assert(xrtValueObjectSetNew(spec, view("shorts"), shorts));
    assert(xrtValueObjectSetNew(spec, view("options"), options));
    assert(xrtValueObjectSetNew(spec, view("defaults"), defaults));
    return spec;
}

typedef struct {
    const char* Args[4];
    const char *Output, *Level, *Error;
    bool Verbose;
    size_t Rest;
} parse_case;

static const parse_case cases[] = {
    {{NULL}, NULL, NULL, "", false, 0},
    {{"--output=", NULL}, "", NULL, "", false, 0},
    {{"-o", "--", "-v", NULL}, "--", NULL, "", true, 0},
    {{"-vovalue", "--level=9", NULL}, "value", "9", "", true, 0},
    {{"--output=first", "-osecond", "-o", ""}, "", NULL, "", false, 0},
    {{"--", "-o", "--verbose", NULL}, NULL, NULL, "", false, 2},
    {{"--missing", "-v", NULL}, NULL, NULL, "unknown option --missing", false, 0},
    {{"-vz", "--output=ignored", NULL}, NULL, NULL, "unknown short option -z", true, 0},
    {{"--verbose=yes", "-v", NULL}, NULL, NULL, "flag --verbose does not take a value", false, 0},
    {{"--output", NULL}, NULL, NULL, "missing value for --output", false, 0},
    {{"-o", NULL}, NULL, NULL, "missing value for -o", false, 0},
    {{"-o中文.txt", "-l=literal", NULL}, "中文.txt", "=literal", "", false, 0}
};

static void result_is(xvalue* result, const parse_case* test, const char* fallback)
{
    bool ok, verbose;
    assert(result && xrtValueGetBool(field(result, "ok"), &ok));
    assert(ok == (test->Error[0] == '\0'));
    text_is(field(result, "error"), test->Error);
    assert(xrtValueGetBool(field(field(result, "flags"), "verbose"), &verbose));
    assert(verbose == test->Verbose);
    assert(xrtValueCount(field(result, "flags")) == 1);
    assert(xrtValueCount(field(result, "options")) == 2);
    text_is(field(field(result, "options"), "output"), test->Output ? test->Output : fallback);
    text_is(field(field(result, "options"), "level"), test->Level ? test->Level : fallback);
    assert(xrtValueCount(field(result, "positional")) == 0);
    assert(xrtValueCount(field(result, "rest")) == test->Rest);
    if (test->Rest) {
        text_is(xrtValueArrayAt(field(result, "rest"), 0), "-o");
        text_is(xrtValueArrayAt(field(result, "rest"), 1), "--verbose");
    }
    assert(!xrtGetError());
}

/* 每个故障点后立即恢复，用同一份输入再解析，并保留一份旧结果跨故障存活。 */
static unsigned int fault_points(const char* fallback, const parse_case* test)
{
    xmemdebugsnapshot outer, before;
    xvalue *spec, *args, *held;
    unsigned int failures = 0;
    xrtClearError();
    xrtMemDebugSnapshot(&outer);
    spec = make_spec(fallback);
    args = xrtValueArray();
    assert(args);
    for (size_t i = 0; i < 4 && test->Args[i]; ++i)
        assert(xrtValueArrayAppendNew(args, xrtValueString(view(test->Args[i]))));
    held = xcli_parse(args, spec);
    result_is(held, test, fallback);
    xrtMemDebugSnapshot(&before);
    for (unsigned int budget = 0; budget < 512; ++budget) {
        xvalue* result;
        bool triggered;
        xerrkind kind;
        assert(xrtMemDebugFailAfter(budget));
        result = xcli_parse(args, spec);
        triggered = xrtMemDebugFailTriggered();
        kind = xrtErrorKind(xrtGetError());
        xrtMemDebugFailClear();
        if (triggered) {
            assert(!result && kind == XERR_MEMORY);
            ++failures;
            xrtClearError();
            result = xcli_parse(args, spec);
        }
        result_is(result, test, fallback);
        result_is(held, test, fallback);
        // 结果和失败清理不能改写原始声明、参数正文或共享默认值。
        text_is(field(field(spec, "defaults"), "output"), fallback);
        text_is(field(field(spec, "defaults"), "level"), fallback);
        for (size_t i = 0; i < 4 && test->Args[i]; ++i)
            text_is(xrtValueArrayAt(args, (int64)i), test->Args[i]);
        xrtValueRelease(result);
        balanced(&before);
        if (!triggered) break;
        assert(budget != 511);
    }
    assert(failures > 0);
    // 替换默认值后，新结果使用新文本；旧结果仍与原输入寿命解耦。
    assert(xrtValueObjectSetNew(field(spec, "defaults"), view("output"), xrtValueString(view("replacement"))));
    {
        parse_case changed = *test;
        xvalue* result = xcli_parse(args, spec);
        if (!changed.Output) changed.Output = "replacement";
        result_is(result, &changed, fallback);
        xrtValueRelease(result);
    }
    xrtValueRelease(args);
    xrtValueRelease(spec);
    result_is(held, test, fallback);
    xrtValueRelease(held);
    balanced(&outer);
    return failures;
}

int main(void)
{
    static char long_text[8193];
    const char* fallbacks[] = {"", "默认值", long_text};
    unsigned int failures = 0;
    assert(xrtMemDebugEnable(true));
    memset(long_text, 'x', sizeof(long_text) - 1);
    for (size_t f = 0; f < sizeof(fallbacks) / sizeof(fallbacks[0]); ++f)
        for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
            failures += fault_points(fallbacks[f], &cases[c]);
    printf("cli defaults: 36 combinations, %u fault points, immediate recovery and lifetime passed\n", failures);
    return 0;
}
