/* 使用 -include 载入生成 C，直接校验其公开 XRT 声明与故障清理。 */
#include <assert.h>
#include <string.h>

static int joinWorker(void* input)
{
    xvalue* values = input;
    unsigned int round;
    for (round = 0; round < 500; ++round) {
        char* result;
        xrtClearError();
        result = xl6_xrt_string_join(values, xrtStrView("|"));
        assert(result != NULL && strcmp(result, "shared|42") == 0);
        xrtFree(result);
        assert(xrtGetError() == NULL);
        assert(xl6_xrt_string_join(NULL, xrtStrView("|")) == NULL);
        assert(xrtErrorKind(xrtGetError()) == XERR_TYPE);
    }
    xrtClearError();
    return 0;
}

static void checkJoinThreads(void)
{
    xvalue* values = xrtValueArray();
    xthread* workers[4];
    unsigned int i;
    assert(values != NULL);
    assert(xrtValueArrayAppendNew(values, xrtValueString(xrtStrView("shared"))));
    assert(xrtValueArrayAppendNew(values, xrtValueInt(42)));
    for (i = 0; i < 4; ++i) {
        workers[i] = xrtThreadCreate(joinWorker, values, 0);
        assert(workers[i] != NULL);
    }
    // 同一只读输入可并发借用；每线程的错误不得泄漏到调用者。
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
    xvalue* values = xrtValueArray();
    xvalue* wrong = xrtValueInt(7);
    xvalue* nested = xrtValueArray();
    xmemdebugsnapshot before, after;
    char* result;
    unsigned int point, failed = 0, succeeded = 0;
    assert(values != NULL && wrong != NULL && nested != NULL);

    result = xl6_xrt_string_join(values, xrtStrView(","));
    assert(result != NULL && strcmp(result, "") == 0);
    xrtFree(result);
    xrtClearError();
    assert(xl6_xrt_string_join(NULL, xrtStrView(",")) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_TYPE);
    xrtClearError();
    assert(xl6_xrt_string_join(wrong, xrtStrView(",")) == NULL);
    assert(xrtErrorKind(xrtGetError()) == XERR_TYPE);
    xrtClearError();

    /* 原生字符串视图必须保留 NUL，语言字符串全链路另有独立回归用例。 */
    assert(xrtValueArrayAppendNew(values, xrtValueString((xstrview){"a\0b", 3})));
    assert(xrtValueArrayAppendNew(values, xrtValueString(xrtStrView("c"))));
    result = xl6_xrt_string_join(values, xrtStrView("|"));
    assert(result != NULL && memcmp(result, "a\0b|c", 6) == 0);
    xrtFree(result);
    xrtValueRelease(values);
    values = xrtValueArray();
    assert(values != NULL);

    assert(xrtValueArrayAppendNew(values, xrtValueString(xrtStrView("text"))));
    assert(xrtValueArrayAppendNew(values, xrtValueBool(true)));
    assert(xrtValueArrayAppendNew(values, xrtValueNull()));
    assert(xrtValueArrayAppendNew(values, xrtValueInt(-7)));
    assert(xrtValueArrayAppendNew(values, xrtValueUInt(UINT64_MAX)));
    assert(xrtValueArrayAppendNew(values, xrtValueFloat(1.5)));
    assert(xrtValueArrayAppendNew(nested, xrtValueInt(1)));
    assert(xrtValueArrayAppendNew(nested, xrtValueInt(2)));
    assert(xrtValueArrayAppendNew(values, nested));

    // 逐点故障必须返回失败而非部分结果，输入、错误及活动分配保持一致。
    for (point = 0; point < 64; ++point) {
        bool triggered;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        assert(xrtMemDebugFailAfter(point));
        result = xl6_xrt_string_join(values, xrtStrView("|"));
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            assert(result == NULL && xrtGetError() != NULL);
            ++failed;
        } else {
            assert(result != NULL && xrtGetError() == NULL);
            assert(strcmp(result, "text|true|null|-7|18446744073709551615|1.5|[1,2]") == 0);
            ++succeeded;
        }
        xrtFree(result);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(xrtValueCount(values) == 7 && xrtValueCount(nested) == 2);
    }
    assert(failed != 0 && succeeded != 0);
    xrtValueRelease(values);
    xrtValueRelease(wrong);
    printf("string join allocation failures ok: rejected=%u completed=%u\n", failed, succeeded);
}

static void checkAllocationFailures(void)
{
    unsigned int point;
    unsigned int failed = 0;
    unsigned int succeeded = 0;
    xmemdebugsnapshot snapshot;

    for ( point = 0; point < 128; ++point ) {
        xvalue* value;
        bool triggered;
        xrtClearError();
        assert(xrtMemDebugFailAfter(point));
        value = xl6_xrt_string_split(xrtStrView("a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p,q,r,s,t"), xrtStrView(","));
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if ( value != NULL ) {
            assert(xrtValueCount(value) == 20);
            ++succeeded;
        } else {
            assert(triggered);
            ++failed;
        }
        xrtValueRelease(value);
        xrtClearError();

        assert(xrtMemDebugFailAfter(point));
        value = xl6_xrt_string_split_lines(xrtStrView("a\r\nb\nc\rd\ne\nf\ng\nh\ni\nj\nk\nl\nm\nn\no\np\nq\nr\ns\nt"));
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if ( value != NULL ) {
            assert(xrtValueCount(value) == 20);
            ++succeeded;
        } else {
            assert(triggered);
            ++failed;
        }
        xrtValueRelease(value);
        xrtClearError();
    }
    assert(failed != 0 && succeeded != 0);
    xrtMemDebugSnapshot(&snapshot);
    assert(snapshot.DoubleFreeCount == 0 && snapshot.InvalidFreeCount == 0);
    printf("string bridge allocation failures ok: rejected=%u completed=%u\n", failed, succeeded);
}

static void checkEnumeration(void)
{
    const char* invalid[] = {"\x80", "\xc0\xaf", "\xe4\xbd", "\xed\xa0\x80", "\xf4\x90\x80\x80"};
    const char* text = "A\xe4\xbd\xa0\xf0\x90\x90\x80" "e\xcc\x81";
    const char* characters[] = {"A", "\xe4\xbd\xa0", "\xf0\x90\x90\x80", "e", "\xcc\x81"};
    xvalue* result;
    xmemdebugsnapshot before, after;
    char raw[256];
    unsigned int i, kind, point, rejected = 0, completed = 0;
    for (i = 1; i < 256; ++i) raw[i - 1] = (char)i;
    raw[255] = 0;
    result = xl6_xrt_string_bytes((xstrview){raw, 255});
    assert(result != NULL && xrtValueCount(result) == 255);
    for (i = 0; i < 255; ++i) {
        int64 number = 0;
        assert(xrtValueGetInt(xrtValueArrayGet(result, i), &number) && number == i + 1);
    }
    xrtValueRelease(result);
    result = xl6_xrt_string_chars(xrtStrView(text));
    assert(result != NULL && xrtValueCount(result) == 5);
    for (i = 0; i < 5; ++i) {
        xstrview actual;
        assert(xrtValueGetString(xrtValueArrayGet(result, i), &actual));
        assert(actual.Size == strlen(characters[i]) && memcmp(actual.Data, characters[i], actual.Size) == 0);
    }
    xrtValueRelease(result);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        assert(xl6_xrt_string_chars(xrtStrView(invalid[i])) == NULL);
        assert(xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    }
    /* 字节不做编码检查；字符接口失败后不保留半份结果或污染后续调用。 */
    for (kind = 0; kind < 2; ++kind) {
        for (point = 0; point < 80; ++point) {
            bool triggered;
            xrtClearError();
            xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            result = kind ? xl6_xrt_string_chars(xrtStrView(text)) : xl6_xrt_string_bytes(xrtStrView(text));
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                assert(result == NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++rejected;
            } else {
                assert(result != NULL && xrtGetError() == NULL);
                assert(xrtValueCount(result) == (kind ? 5 : strlen(text)));
                ++completed;
            }
            xrtValueRelease(result);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        }
    }
    assert(rejected != 0 && completed != 0);
    printf("string enumeration failures ok: rejected=%u completed=%u\n", rejected, completed);
}

int main(void)
{
    xvalue* arguments;
    int result;
    assert(xrtMemDebugEnable(true));
    checkJoin();
    checkJoinThreads();
    checkAllocationFailures();
    checkEnumeration();
    arguments = xrtValueArray();
    assert(arguments != NULL);
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    if ( xl_script_has_error() ) {
        xl_script_print_uncaught();
        result = 1;
    }
    xl_script_module_unit();
    xl_script_error_unit();
    return result;
}
