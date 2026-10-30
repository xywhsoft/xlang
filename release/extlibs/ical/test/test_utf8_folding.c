/* 折行先恢复字节序列，再校验 UTF-8；覆盖已映射、忽略字段、参数和嵌套组件。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/ical_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../ical_native.c"
#include "../ical_parse.c"
#include "../ical_build.c"
#endif

static unsigned failures, valid_count, invalid_count, oom_count;
static const char payload[] = "A\xc2\xa2\xe4\xbd\xa0\xf0\x9f\x98\x80Z";

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "ical folding: %s\n", message); }
}

static bool balanced(const xmemdebugsnapshot* before, const xmemdebugsnapshot* after)
{
    return before->LiveCount == after->LiveCount && before->LiveBytes == after->LiveBytes &&
        before->InvalidFreeCount == after->InvalidFreeCount && before->DoubleFreeCount == after->DoubleFreeCount &&
        before->UseAfterFreeCount == after->UseAfterFreeCount;
}

static bool append(xbuffer* out, const char* text)
{
    return xrtBufferAppend(out, (xbytesview){(cbytes)text, strlen(text)});
}

static xvalue* get(xvalue* object, const char* key)
{
    return xrtValueObjectGet(object, (xstrview){key, strlen(key)});
}

static bool equals(xvalue* value, const char* text, size_t length)
{
    xstrview found;
    return value && xrtValueGetString(value, &found) && found.Size == length &&
        !memcmp(found.Data, text, length);
}

static bool value_is_correct(xvalue* calendar, unsigned field)
{
    if (!calendar || xrtValueCount(get(calendar, "events")) != 1) return false;
    xvalue* event = xrtValueArrayAt(get(calendar, "events"), 0);
    if (!equals(get(event, "uid"), "one", 3)) return false;
    if (field == 0) return equals(get(calendar, "prodid"), payload, sizeof(payload) - 1);
    if (field <= 3) return equals(get(event, field == 1 ? "summary" :
        field == 2 ? "description" : "categories"), payload, sizeof(payload) - 1);
    if (field == 5) return equals(get(event, "summary"), "keep", 4);
    return get(event, "description") == NULL;
}

static bool make_input(xbuffer* out, unsigned field, const char* bytes, size_t length,
    size_t cut, char whitespace, bool every_byte)
{
    static const char* prefix[] = {"PRODID:", "SUMMARY:", "DESCRIPTION:",
        "CATEGORIES:", "X-NOTE:", "SUMMARY;X-NOTE=\"", "DESCRIPTION:"};
    if (!xrtBufferInit(out) || !append(out, "BEGIN:VCALENDAR\r\nVERSION:2.0\r\n")) return false;
    if (field != 0 && !append(out, "PRODID:test\r\n")) return false;
    if (field != 0 && !append(out, "BEGIN:VEVENT\r\nUID:one\r\nDTSTAMP:20261030T000000Z\r\nDTSTART:20261030T120000Z\r\n")) return false;
    if (field == 6 && !append(out, "BEGIN:VALARM\r\n")) return false;
    if (!append(out, prefix[field])) return false;
    for (size_t i = 0; i <= length; ++i) {
        if ((every_byte && i && i < length) || (!every_byte && i == cut)) {
            if (!append(out, "\r\n") || !xrtBufferAppendByte(out, (uint8)whitespace)) return false;
        }
        if (i < length && !xrtBufferAppendByte(out, (uint8)bytes[i])) return false;
    }
    if (!append(out, field == 5 ? "\":keep\r\n" : "\r\n")) return false;
    if (field == 6 && !append(out, "END:VALARM\r\n")) return false;
    if (field == 0 && !append(out, "BEGIN:VEVENT\r\nUID:one\r\nDTSTAMP:20261030T000000Z\r\nDTSTART:20261030T120000Z\r\n")) return false;
    return append(out, "END:VEVENT\r\nEND:VCALENDAR\r\n");
}

static void valid_case(unsigned field, size_t cut, char whitespace, bool every_byte)
{
    xbuffer input;
    if (!make_input(&input, field, payload, sizeof(payload) - 1, cut, whitespace, every_byte)) {
        check(false, "fixture construction failed"); return;
    }
    xvalue* calendar = xi_parse((char*)input.Data, input.Size);
    bool ok = value_is_correct(calendar, field) && !xrtGetError();
    check(ok, "valid folded value was rejected or changed");
    if (ok) {
        char* text = xi_build(calendar);
        xvalue* back = text ? xi_parse(text, strlen(text)) : NULL;
        check(text && xrtUtf8Valid((xstrview){text, strlen(text)}, NULL) &&
            value_is_correct(back, field), "build/reparse lost the recovered value");
        xrtValueRelease(back);
        xrtFree(text);
    }
    xrtValueRelease(calendar);
    xrtBufferUnit(&input);
    xrtClearError();
    ++valid_count;
}

static void invalid_cases(void)
{
    static const char* invalid[] = {"\x80", "\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80",
        "\xc2", "\xe4\xbd", "\xf0\x9f\x98", "\xff", "a\rb", "a\x7f"};
    for (unsigned field = 0; field < 7; ++field) {
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            for (unsigned folding = 0; folding < 2; ++folding) {
                xbuffer input;
                xmemdebugsnapshot before, after;
                xrtClearError();
                xrtMemDebugSnapshot(&before);
                size_t length = strlen(invalid[i]);
                if (!make_input(&input, field, invalid[i], length,
                    folding ? 1 : length + 1, ' ', false)) {
                    check(false, "invalid fixture construction failed"); return;
                }
                xvalue* value = xi_parse((char*)input.Data, input.Size);
                check(!value && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE,
                    "invalid logical UTF-8/control was accepted");
                xrtValueRelease(value);
                xrtBufferUnit(&input);
                xrtClearError();
                xrtMemDebugSnapshot(&after);
                check(balanced(&before, &after), "invalid logical line leaked partial state");
                ++invalid_count;
            }
        }
    }
}

static void allocation_cases(void)
{
    for (unsigned field = 0; field < 7; ++field) {
        xbuffer input;
        if (!make_input(&input, field, payload, sizeof(payload) - 1, 0, '\t', true)) {
            check(false, "OOM fixture construction failed"); return;
        }
        bool complete = false;
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        for (unsigned budget = 0; budget < 256; ++budget) {
            if (!xrtMemDebugFailAfter(budget)) { check(false, "cannot inject OOM"); break; }
            xvalue* value = xi_parse((char*)input.Data, input.Size);
            bool injected = xrtMemDebugFailTriggered();
            bool memory = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            check(injected ? !value && memory : value_is_correct(value, field) && !xrtGetError(),
                "folded parse lost OOM classification or value");
            xrtValueRelease(value);
            xrtClearError();
            value = xi_parse((char*)input.Data, input.Size);
            check(value_is_correct(value, field), "same input did not recover");
            xrtValueRelease(value);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "folded parse did not balance memory");
            if (!injected) { complete = true; break; }
            ++oom_count;
        }
        check(complete, "OOM matrix did not reach success");
        xrtBufferUnit(&input);
    }
}

int main(int argc, char** argv)
{
    bool vectors_only = argc > 1 && !strcmp(argv[1], "vectors");
    if (!xrtMemDebugEnabled() && !xrtMemDebugEnable(true)) return 2;
    for (unsigned field = 0; field < 7; ++field) {
        for (size_t cut = 1; cut < sizeof(payload) - 1; ++cut) {
            valid_case(field, cut, ' ', false);
            valid_case(field, cut, '\t', false);
        }
        valid_case(field, 0, ' ', true);
    }
    invalid_cases();
    if (!vectors_only) allocation_cases();
    if (!failures) printf("ical UTF8 folding: %u valid, %u invalid, %u OOM PASS\n",
        valid_count, invalid_count, oom_count);
    return failures ? 1 : 0;
}
