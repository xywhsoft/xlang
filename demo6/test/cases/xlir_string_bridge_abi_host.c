/* Preinclude generated C, or use XLIR_NATIVE_PROBE_ONLY for the native oracle. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/runtime/xl6_text_value.h"
static void expectText(xstrview result, const char* expected, size_t size)
{
    assert(result.Data != NULL && result.Size == size);
    assert(memcmp(result.Data, expected, size) == 0 && result.Data[size] == 0);
    xrtFree((void*)result.Data);
}
static void expectBalanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(before->DoubleFreeCount == after.DoubleFreeCount);
    assert(before->InvalidFreeCount == after.InvalidFreeCount);
    assert(before->UseAfterFreeCount == after.UseAfterFreeCount);
}
static int joinWorker(void* input)
{
    unsigned int round;
    for (round = 0; round < 500; ++round) {
        xrtClearError();
        expectText(xl6TextJoinV1(input, (xstrview){"|", 1}, -1, -1), "shared|42", 9);
        assert(xrtGetError() == NULL);
        assert(xl6TextJoinV1(NULL, (xstrview){"|", 1}, -1, -1).Data == NULL);
        assert(xrtErrorKind(xrtGetError()) == XERR_TYPE);
    }
    xrtClearError(); return 0;
}
static void checkJoinThreads(void)
{
    xvalue* values = xrtValueArray();
    xthread* workers[4]; unsigned int i;
    assert(values != NULL);
    assert(xrtValueArrayAppendNew(values, xrtValueString((xstrview){"shared", 6})));
    assert(xrtValueArrayAppendNew(values, xrtValueString((xstrview){"42", 2})));
    for (i = 0; i < 4; ++i) {
        workers[i] = xrtThreadCreate(joinWorker, values, 0);
        assert(workers[i] != NULL);
    }
    /* Immutable inputs are borrowed; each worker's error remains TLS. */
    xrtSetErrorKind(XERR_RANGE);
    for (i = 0; i < 4; ++i) {
        assert(xrtThreadWait(workers[i]) == XWAIT_OK);
        assert(xrtThreadExitCode(workers[i]) == 0);
        xrtThreadDestroy(workers[i]);
    }
    assert(xrtErrorKind(xrtGetError()) == XERR_RANGE);
    xrtClearError();
    assert(xrtValueCount(values) == 2);
    xrtValueRelease(values);
    puts("string join threads ok: 2000 rounds");
}
static void checkJoin(void)
{
    const char* words[] = {"text", "true", "null", "-7", "18446744073709551615", "1.5", "[1,2]"};
    const char expected[] = "text|true|null|-7|18446744073709551615|1.5|[1,2]";
    xvalue* values = xrtValueArray(); xvalue* wrong = xrtValueInt(7);
    xmemdebugsnapshot before; unsigned int i, point, failed = 0, succeeded = 0;
    assert(values != NULL && wrong != NULL);
    expectText(xl6TextJoinV1(values, (xstrview){",", 1}, -1, -1), "", 0);
    assert(xl6TextJoinV1(NULL, (xstrview){",", 1}, -1, -1).Data == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_TYPE); xrtClearError();
    assert(xl6TextJoinV1(wrong, (xstrview){",", 1}, -1, -1).Data == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_TYPE); xrtClearError();
    assert(xrtValueArrayAppendNew(values, xrtValueString((xstrview){"a\0b", 3})));
    assert(xrtValueArrayAppendNew(values, xrtValueString((xstrview){"c", 1})));
    expectText(xl6TextJoinV1(values, (xstrview){"|", 1}, -1, -1), "a\0b|c", 5);
    expectText(xl6TextJoinV1(values, (xstrview){"|\0x", 3}, 2, 2), "a\0|\0c", 5);
    /* The typed join boundary must reject non-string elements without mutation. */
    assert(xrtValueArrayAppendNew(values, xrtValueInt(42)));
    assert(xl6TextJoinV1(values, (xstrview){",", 1}, -1, -1).Data == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_TYPE && xrtValueCount(values) == 3);
    xrtClearError(); xrtValueRelease(values);
    values = xrtValueArray(); assert(values != NULL);
    for (i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        assert(xrtValueArrayAppendNew(values, xrtValueString((xstrview){words[i], strlen(words[i])})));
    for (point = 0; point < 64; ++point) {
        xstrview result; bool triggered;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(point));
        result = xl6TextJoinV1(values, (xstrview){"|", 1}, -1, -1);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (triggered) {
            assert(result.Data == NULL && result.Size == 0 && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
            ++failed;
        } else {
            assert(xrtGetError() == NULL);
            expectText(result, expected, sizeof(expected) - 1); ++succeeded;
        }
        xrtClearError(); expectBalanced(&before);
        assert(xrtValueCount(values) == 7);
    }
    assert(failed != 0 && succeeded != 0);
    xrtValueRelease(values); xrtValueRelease(wrong);
    printf("string join allocation failures ok: rejected=%u completed=%u\n", failed, succeeded);
}
static void checkAllocationFailures(void)
{
    const xstrview comma = {",", 1};
    const xstrview fields = {"a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t", 39};
    const char lines[] = "a\r\nb\nc\rd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\nt";
    unsigned int point, kind, failed = 0, succeeded = 0;
    for (kind = 0; kind < 2; ++kind)
    for (point = 0; point < 128; ++point) {
        xvalue* value; xmemdebugsnapshot before; bool triggered;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(point));
        value = kind ? xl6TextLinesV1((xstrview){lines, sizeof(lines) - 1}, -1) :
            xl6TextSplitV1(fields, comma, -1, -1);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (triggered) {
            assert(value == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++failed;
        } else {
            assert(value != NULL && xrtValueCount(value) == 20 && xrtGetError() == NULL); ++succeeded;
        }
        xrtValueRelease(value); xrtClearError(); expectBalanced(&before);
    }
    assert(failed != 0 && succeeded != 0);
    printf("string bridge allocation failures ok: rejected=%u completed=%u\n", failed, succeeded);
}
static void checkEnumeration(void)
{
    const char* invalid[] = {"\x80", "\xc0\xaf", "\xe4\xbd", "\xed\xa0\x80", "\xf4\x90\x80\x80"};
    const char text[] = "A\xe4\xbd\xa0\xf0\x90\x90\x80" "e\xcc\x81";
    const uint32_t characters[] = {65, 0x4f60, 0x10400, 101, 0x301};
    xvalue* result; xbytesview octets; uint8_t raw[256];
    unsigned int i, kind, point, rejected = 0, completed = 0;
    for (i = 0; i < 256; ++i) raw[i] = (uint8_t)i;
    result = xl6TextBytesV1((xstrview){(const char*)raw, sizeof(raw)}, -1);
    assert(result != NULL && xrtValueGetBytes(result, &octets));
    assert(octets.Size == sizeof(raw) && memcmp(octets.Data, raw, sizeof(raw)) == 0);
    xrtValueRelease(result);
    result = xl6TextCharsV1((xstrview){text, sizeof(text) - 1}, -1);
    assert(result != NULL && xrtValueCount(result) == 5);
    for (i = 0; i < 5; ++i) {
        uint32_t actual;
        assert(xrtValueGetChar(xrtValueArrayGet(result, i), &actual) && actual == characters[i]);
    }
    xrtValueRelease(result);
    result = xl6TextCharsV1((xstrview){"a\0b", 3}, -1);
    assert(result != NULL && xrtValueCount(result) == 3);
    { uint32_t actual; assert(xrtValueGetChar(xrtValueArrayGet(result, 1), &actual) && actual == 0); }
    xrtValueRelease(result);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before);
        assert(xl6TextCharsV1((xstrview){invalid[i], strlen(invalid[i])}, -1) == NULL);
        assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError(); expectBalanced(&before);
    }
    for (kind = 0; kind < 2; ++kind)
    for (point = 0; point < 80; ++point) {
        bool triggered; xmemdebugsnapshot before;
        xrtMemDebugSnapshot(&before); assert(xrtMemDebugFailAfter(point));
        result = kind ? xl6TextCharsV1((xstrview){text, sizeof(text) - 1}, -1) :
            xl6TextBytesV1((xstrview){text, sizeof(text) - 1}, -1);
        triggered = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if (triggered) {
            assert(result == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY); ++rejected;
        } else {
            assert(result != NULL && xrtGetError() == NULL);
            if (kind) assert(xrtValueCount(result) == 5);
            else assert(xrtValueGetBytes(result, &octets) && octets.Size == sizeof(text) - 1);
            ++completed;
        }
        xrtValueRelease(result); xrtClearError(); expectBalanced(&before);
    }
    assert(rejected != 0 && completed != 0);
    printf("string enumeration failures ok: rejected=%u completed=%u\n", rejected, completed);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    checkJoin(); checkJoinThreads(); checkAllocationFailures(); checkEnumeration();
#ifndef XLIR_NATIVE_PROBE_ONLY
    {
        xvalue* arguments = xrtValueArray(); int result;
        assert(arguments != NULL);
        result = xlang_main(arguments); xrtValueRelease(arguments);
        if (xl_script_has_error()) { xl_script_print_uncaught(); result = 1; }
        assert(xl_script_module_unit_checked()); xl_script_error_unit();
        return result;
    }
#else
    return 0;
#endif
}
