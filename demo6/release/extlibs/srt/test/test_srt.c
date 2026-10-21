#define XRT_MODULE_MEMORY_DEBUG
#include "srt_native.h"
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "srt C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int invalid_srt(const char *text) {
    xvalue *result = xsrt_parse(text);
    int invalid = result != NULL && xrtValueType(result) == XVALUE_NULL;
    xrtValueRelease(result);
    xrtClearError();
    return invalid;
}

static int invalid_vtt(const char *text) {
    xvalue *result = xsrt_parse_vtt(text);
    int invalid = result != NULL && xrtValueType(result) == XVALUE_NULL;
    xrtValueRelease(result);
    xrtClearError();
    return invalid;
}

static int test_explicit_lengths(void) {
    static const char srt[] = "1\n00:00:00,000 --> 00:00:01,000\nHi\n";
    static const char vtt[] = "WEBVTT\n\n00:00.000 --> 00:01.000\nHi\n";
    char srtBytes[sizeof(srt) - 1];
    char vttBytes[sizeof(vtt) - 1];
    char withTail[sizeof(srt) + 1];
    xvalue *parsed;

    /* 显式长度入口不能依赖终止符，也不能读取长度外的毒化字节。 */
    memcpy(srtBytes, srt, sizeof(srtBytes));
    memcpy(vttBytes, vtt, sizeof(vttBytes));
    memcpy(withTail, srtBytes, sizeof(srtBytes));
    withTail[sizeof(srtBytes)] = (char)0xff;
    withTail[sizeof(srtBytes) + 1] = '\0';
    parsed = xsrt_parse_n(srtBytes, sizeof(srtBytes));
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
        xrtValueCount(parsed) == 1 && xrtGetError() == NULL);
    xrtValueRelease(parsed);
    parsed = xsrt_parse_vtt_n(vttBytes, sizeof(vttBytes));
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
        xrtValueCount(parsed) == 1 && xrtGetError() == NULL);
    xrtValueRelease(parsed);
    parsed = xsrt_parse_n(withTail, sizeof(srtBytes));
    CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
        xrtValueCount(parsed) == 1 && xrtGetError() == NULL);
    xrtValueRelease(parsed);
    return 0;
}

static int test_vtt_allocation_failure(xvalue *web) {
    xvalue *cue = xrtValueArrayAt(web, 0);
    const char *document = "WEBVTT\n\nfirst\n00:00.500 --> 00:01.500 align:start\nHi\n\n";
    char id[512];
    unsigned failure;
    memset(id, 'i', sizeof(id));
    CHECK(cue != NULL && xrtValueObjectSetNew(cue, XRT_STR_LITERAL("id"),
        xrtValueString((xstrview){id, sizeof(id)})));
    for (failure = 0u; failure < 5u; ++failure) {
        char *result;
        bool triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        result = (char *)xsrt_build_vtt(web);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(result == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else CHECK(result != NULL && xrtGetError() == NULL);
        xrtFree(result);
    }
    for (failure = 0u; failure < 24u; ++failure) {
        xvalue *parsed;
        bool triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        parsed = xsrt_parse_vtt_n(document, strlen(document));
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(parsed == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else CHECK(parsed != NULL && xrtValueType(parsed) == XVALUE_ARRAY &&
            xrtGetError() == NULL);
        xrtValueRelease(parsed);
    }
    xrtClearError();
    CHECK(xrtValueObjectSetNew(cue, XRT_STR_LITERAL("id"),
        xrtValueString(XRT_STR_LITERAL("first"))));
    memset(id, 's', sizeof(id));
    CHECK(xrtValueObjectSetNew(cue, XRT_STR_LITERAL("settings"),
        xrtValueString((xstrview){id, sizeof(id)})));
    for (failure = 0u; failure < 5u; ++failure) {
        char *result;
        bool triggered;
        xrtClearError();
        CHECK(xrtMemDebugFailAfter(failure));
        result = (char *)xsrt_build_vtt(web);
        triggered = xrtMemDebugFailTriggered();
        xrtMemDebugFailClear();
        if (triggered) {
            CHECK(result == NULL && xrtGetError() != NULL &&
                xrtErrorKind(xrtGetError()) == XERR_MEMORY);
        } else CHECK(result != NULL && xrtGetError() == NULL);
        xrtFree(result);
    }
    xrtClearError();
    CHECK(xrtValueObjectSetNew(cue, XRT_STR_LITERAL("settings"),
        xrtValueString(XRT_STR_LITERAL("align:start"))));
    return 0;
}

int main(void) {
    const char *source = "1\r\n00:00:01,000 --> 00:00:02,500\r\nHello\r\n\r\n";
    const char *vtt = "WEBVTT - sample\n\nfirst\n00:00:00.500 --> 00:00:01.500 align:start\nHi\n\n";
    char *longText = (char *)xrtMalloc(70000);
    xvalue *cues = xsrt_parse(source);
    xvalue *web = xsrt_parse_vtt(vtt);
    char *first, *second, *stamp;
    CHECK(cues != NULL && xrtValueType(cues) == XVALUE_ARRAY && xrtValueCount(cues) == 1);
    CHECK(web != NULL && xrtValueType(web) == XVALUE_ARRAY && xrtValueCount(web) == 1);
    CHECK(test_explicit_lengths() == 0);

    {
        const char *utf8Source = "\xef\xbb\xbf" "1\r\n"
            "00:00:01,000 --> 00:00:02,500\r\n"
            "\xe4\xbd\xa0\xe5\xa5\xbd\r\n\r\n";
        const char *normalized = "1\n00:00:01,000 --> 00:00:02,500\n"
            "\xe4\xbd\xa0\xe5\xa5\xbd\n\n";
        xvalue *utf8Cues = xsrt_parse(utf8Source);
        xvalue *again;
        char *encoded;
        CHECK(utf8Cues != NULL && xrtValueType(utf8Cues) == XVALUE_ARRAY);
        encoded = (char *)xsrt_build(utf8Cues);
        CHECK(encoded != NULL && strcmp(encoded, normalized) == 0);
        again = xsrt_parse(encoded);
        CHECK(again != NULL && xrtValueType(again) == XVALUE_ARRAY &&
            xrtValueCount(again) == 1);
        xrtValueRelease(again);
        xrtFree(encoded);
        xrtValueRelease(utf8Cues);
    }

    CHECK(test_vtt_allocation_failure(web) == 0);

    first = (char *)xsrt_build(cues);
    second = (char *)xsrt_build_vtt(web);
    CHECK(first != NULL && second != NULL && first != second);
    CHECK(strstr(first, "00:00:02,500\nHello") != NULL);
    CHECK(strstr(second, "first\n00:00:00.500 --> 00:00:01.500 align:start\nHi") != NULL);
    CHECK(strstr(first, "Hello") != NULL); /* 第二次生成不得覆盖第一次结果。 */
    xrtFree(first);
    xrtFree(second);
    {
        xvalue *cue = xrtValueArrayAt(web, 0);
        CHECK(xrtValueObjectSetNew(cue, XRT_STR_LITERAL("id"), xrtValueString(XRT_STR_LITERAL("NOTE"))));
        CHECK(xsrt_build_vtt(web) == NULL);
        CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
        CHECK(xrtValueObjectSetNew(cue, XRT_STR_LITERAL("id"), xrtValueString(XRT_STR_LITERAL("cue-->bad"))));
        CHECK(xsrt_build_vtt(web) == NULL);
        xrtClearError();
        CHECK(xrtValueObjectSetNew(cue, XRT_STR_LITERAL("id"), xrtValueString(XRT_STR_LITERAL("first"))));
    }

    stamp = (char *)xsrt_format_time(3723004);
    CHECK(stamp != NULL && strcmp(stamp, "01:02:03,004") == 0);
    xrtFree(stamp);
    CHECK(xsrt_parse_time("01:02:03,004") == 3723004);
    CHECK(xsrt_parse_time("00:02.500") == 2500);
    CHECK(xsrt_parse_time_n("00:02.500", 9) == 2500);
    CHECK(xsrt_parse_time("2562047788015:00:00,000") == 9223372036854000000LL);
    stamp = (char *)xsrt_format_time(INT64_MAX);
    CHECK(stamp != NULL && xsrt_parse_time(stamp) == INT64_MAX);
    xrtFree(stamp);
    CHECK(xsrt_parse_time("2562047788015:12:55,808") == -1);
    xrtClearError();
    {
        xvalue *maxIndex = xsrt_parse("9223372036854775807\n00:00:00,000 --> 00:00:01,000\nMax\n");
        CHECK(maxIndex != NULL && xrtValueType(maxIndex) == XVALUE_ARRAY && xrtValueCount(maxIndex) == 1);
        xrtValueRelease(maxIndex);
    }
    {
        xvalue *comment = xsrt_parse_vtt("WEBVTT\n\nNOTE\tcomment\nskipped\n\n00:00.000 --> 00:01.000\nVisible\n");
        CHECK(comment != NULL && xrtValueType(comment) == XVALUE_ARRAY && xrtValueCount(comment) == 1);
        xrtValueRelease(comment);
    }
    {
        xvalue *styled = xsrt_parse_vtt(
            "WEBVTT\n\nSTYLE\n::cue { color: white }\n\n"
            "00:00.000 --> 00:01.000\nVisible\n\nNOTE after cue\ncomment\n");
        CHECK(styled != NULL && xrtValueType(styled) == XVALUE_ARRAY &&
            xrtValueCount(styled) == 1);
        xrtValueRelease(styled);
    }
    CHECK(invalid_vtt("WEBVTT\n\n00:00.000 --> 00:01.000\nHi --> there\n"));
    CHECK(invalid_vtt("WEBVTT\n\n00:00.000 --> 00:01.000\nHi\n\n"
        "STYLE\n::cue { color: white }\n"));
    CHECK(invalid_vtt("WEBVTT\n\n00:02.000 --> 00:03.000\nLater\n\n"
        "00:01.000 --> 00:02.000\nEarlier\n"));
    CHECK(invalid_vtt("WEBVTT\n\nrepeat\n00:00.000 --> 00:01.000\nA\n\n"
        "repeat\n00:01.000 --> 00:02.000\nB\n"));
    {
        xvalue *ordered = xsrt_parse_vtt("WEBVTT\n\nfirst\n"
            "00:01.000 --> 00:02.000\nA\n\nsecond\n"
            "00:02.000 --> 00:03.000\nB\n");
        xvalue *secondCue;
        char *output;
        CHECK(ordered != NULL && xrtValueType(ordered) == XVALUE_ARRAY &&
            xrtValueCount(ordered) == 2);
        secondCue = xrtValueArrayAt(ordered, 1);
        CHECK(xrtValueObjectSetNew(secondCue, XRT_STR_LITERAL("startMs"),
            xrtValueInt(0)));
        CHECK(xsrt_build_vtt(ordered) == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
        CHECK(xrtValueObjectSetNew(secondCue, XRT_STR_LITERAL("startMs"),
            xrtValueInt(1000)));
        output = (char *)xsrt_build_vtt(ordered);
        CHECK(output != NULL && xrtGetError() == NULL);
        xrtFree(output);
        CHECK(xrtValueObjectSetNew(secondCue, XRT_STR_LITERAL("id"),
            xrtValueString(XRT_STR_LITERAL("first"))));
        CHECK(xsrt_build_vtt(ordered) == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
        xrtValueRelease(ordered);
    }
    {
        xvalue *plain = xsrt_parse("1\n00:00:00,000 --> 00:00:01,000\nHi --> there\n");
        char *again = plain != NULL && xrtValueType(plain) == XVALUE_ARRAY
            ? (char *)xsrt_build(plain) : NULL;
        CHECK(again != NULL && strstr(again, "Hi --> there\n") != NULL);
        xrtFree(again);
        xrtValueRelease(plain);
    }
    {
        xvalue *cue = xrtValueArrayAt(web, 0);
        xvalue *lines = xrtValueObjectGet(cue, XRT_STR_LITERAL("lines"));
        CHECK(lines != NULL && xrtValueArraySetNew(lines, 0,
            xrtValueString(XRT_STR_LITERAL("Hi --> there"))));
        CHECK(xsrt_build_vtt(web) == NULL && xrtGetError() != NULL &&
            xrtErrorKind(xrtGetError()) == XERR_VALUE);
        xrtClearError();
        CHECK(xrtValueArraySetNew(lines, 0,
            xrtValueString(XRT_STR_LITERAL("Hi"))));
    }
    {
        xvalue *invalid = xsrt_parse_vtt(
            "WEBVTT\n\n00:00.000 --> 00:01.000 align:start --> hidden\nVisible\n");
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL);
        xrtValueRelease(invalid);
        xrtClearError();
    }
    {
        const char embedded[] = "WEBVTT\n\n\0hidden";
        xvalue *invalid = xsrt_parse_vtt_n(embedded, sizeof(embedded) - 1);
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL);
        xrtValueRelease(invalid);
        xrtClearError();
    }
    {
        const char tiny[] = {'x'};
        const char digit[] = {'0'};
        xvalue *invalid = xsrt_parse_n(tiny, SIZE_MAX);
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL && xrtGetError() != NULL);
        xrtValueRelease(invalid);
        xrtClearError();
        invalid = xsrt_parse_vtt_n(tiny, SIZE_MAX);
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL && xrtGetError() != NULL);
        xrtValueRelease(invalid);
        xrtClearError();
        CHECK(xsrt_parse_time_n(digit, SIZE_MAX) == -1 && xrtGetError() != NULL);
        xrtClearError();
    }

    /* 无截断：正文超过旧版 512 字节行和 64 KiB 静态输出上限。 */
    CHECK(longText != NULL);
    memset(longText, 'x', 69999);
    longText[69999] = '\0';
    {
        char *document = (char *)xrtMalloc(70080);
        xvalue *longCues;
        CHECK(document != NULL);
        sprintf(document, "1\n00:00:00,000 --> 00:00:01,000\n%s\n", longText);
        longCues = xsrt_parse(document);
        CHECK(longCues != NULL && xrtValueType(longCues) == XVALUE_ARRAY);
        first = (char *)xsrt_build(longCues);
        CHECK(first != NULL && strlen(first) > 70000 && strstr(first, longText) != NULL);
        xrtFree(first);
        xrtValueRelease(longCues);
        xrtFree(document);
    }
    xrtFree(longText);

    CHECK(invalid_srt("1\n00:00:01,1234 --> 00:00:02,000\nA\n"));
    CHECK(invalid_srt("1\n00:00:01,000xxx --> 00:00:02,000\nA\n"));
    CHECK(invalid_srt("1\n00:00:60,000 --> 00:01:00,000\nA\n"));
    CHECK(invalid_srt("1\n00:00:02,000 --> 00:00:01,000\nA\n"));
    CHECK(invalid_srt("9223372036854775808\n00:00:00,000 --> 00:00:01,000\nA\n"));
    CHECK(invalid_srt("00:00:01,000 --> 00:00:02,000\nA\n"));
    CHECK(invalid_srt("1\n00:00:00,000 --> 00:00:01,000\n\xff\n"));
    CHECK(invalid_srt("1\n00:00:00,000 --> 00:00:01,000\nline\rbroken\n"));
    CHECK(invalid_srt("1\n00:00:00,000 --> 00:00:01,000\nline\r"));
    {
        xvalue *invalid = xsrt_parse_vtt("WEBVTT\n\n00:00.000 --> 00:01.000\n\xff\n");
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL);
        xrtValueRelease(invalid);
        xrtClearError();
    }
    {
        xvalue *invalid = xsrt_parse_vtt("WEBVTT\n\n00:00.000 --> 00:01.000\nline\rbroken\n");
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL);
        xrtValueRelease(invalid);
        xrtClearError();
    }
    CHECK(xsrt_parse_time("99:99:99,000") == -1);
    xrtClearError();
    CHECK(xsrt_parse_time("99999999999999999999:00:00,000") == -1);
    xrtClearError();
    CHECK(xsrt_build(NULL) == NULL);
    xrtClearError();
    CHECK(xsrt_format_time(-1) == NULL);
    xrtClearError();

    {
        xvalue *cue = xrtValueArrayAt(cues, 0);
        xvalue *lines = xrtValueObjectGet(cue, XRT_STR_LITERAL("lines"));
        xvalue *badLine = xrtValueString((xstrview){"\xff", 1u});
        CHECK(lines != NULL && badLine != NULL &&
            xrtValueArraySetNew(lines, 0u, badLine));
        CHECK(xsrt_build(cues) == NULL);
        xrtClearError();
    }

    CHECK(xsrt_parse_time("bad") == -1 && xrtGetError() != NULL);
    CHECK(xsrt_parse_time("00:00:01,000") == 1000 && xrtGetError() == NULL);
    CHECK(xsrt_parse_time_n("bad", 3) == -1 && xrtGetError() != NULL);
    CHECK(xsrt_parse_time_n("00:01.000", 9) == 1000 && xrtGetError() == NULL);
    CHECK(xsrt_format_time(-1) == NULL && xrtGetError() != NULL);
    stamp = (char *)xsrt_format_time(1000);
    CHECK(stamp != NULL && xrtGetError() == NULL);
    xrtFree(stamp);
    CHECK(xsrt_build(NULL) == NULL && xrtGetError() != NULL);
    stamp = (char *)xsrt_build_vtt(web);
    CHECK(stamp != NULL && xrtGetError() == NULL);
    xrtFree(stamp);
    {
        xvalue *invalid = xsrt_parse(NULL);
        xvalue *recovered;
        CHECK(invalid != NULL && xrtValueType(invalid) == XVALUE_NULL &&
            xrtGetError() != NULL);
        xrtValueRelease(invalid);
        recovered = xsrt_parse(source);
        CHECK(recovered != NULL && xrtValueType(recovered) == XVALUE_ARRAY &&
            xrtGetError() == NULL);
        xrtValueRelease(recovered);
    }

    xrtValueRelease(cues);
    xrtValueRelease(web);
    puts("srt C boundaries: PASS");
    return 0;
}
