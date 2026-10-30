/* 七类字段与重复值，分别核对 3.0/4.0 的字节折行、非法输入和失败清理。 */
#define XRT_MODULE_ALL
#define XRT_MODULE_MEMORY_DEBUG
#include "xrt.h"
#include "../include/vcard_native.h"
#include <stdio.h>
#include <string.h>

#ifdef __TINYC__
#include "../vcard_native.c"
#include "../vcard_parse.c"
#include "../vcard_build.c"
#endif

static const char payload[] = "A\xc2\xa2\xe4\xbd\xa0\xf0\x9f\x98\x80Z";
static unsigned failures, valid_count, invalid_count, oom_count;

static void check(bool ok, const char* message)
{
    if (!ok) { ++failures; fprintf(stderr, "vcard folding: %s\n", message); }
}

static bool balanced(const xmemdebugsnapshot* a, const xmemdebugsnapshot* b)
{
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static bool append(xbuffer* out, const char* text)
{
    return xrtBufferAppend(out, (xbytesview){(cbytes)text, strlen(text)});
}

static xvalue* get(xvalue* card, const char* name)
{
    return xrtValueObjectGet(card, (xstrview){name, strlen(name)});
}

static bool equals(xvalue* value, const char* text, size_t length)
{
    xstrview found;
    return value && xrtValueGetString(value, &found) && found.Size == length &&
        !memcmp(found.Data, text, length);
}

static bool correct(xvalue* card, unsigned version, unsigned field)
{
    if (!card || !equals(get(card, "version"), version == 3 ? "3.0" : "4.0", 3)) return false;
    if (!equals(get(card, "fn"), field == 0 ? payload : "keep", field == 0 ? sizeof(payload) - 1 : 4)) return false;
    if (field >= 1 && field <= 4) {
        const char* key = field == 1 ? "note" : field == 2 ? "n" : field == 3 ? "org" : "email";
        return equals(get(card, key), payload, sizeof(payload) - 1);
    }
    if (field == 7) {
        xvalue* notes = get(card, "note");
        return notes && xrtValueType(notes) == XVALUE_ARRAY && xrtValueCount(notes) == 2 &&
            equals(xrtValueArrayAt(notes, 0), "first", 5) &&
            equals(xrtValueArrayAt(notes, 1), payload, sizeof(payload) - 1);
    }
    return get(card, "x-note") == NULL;
}

static bool make_input(xbuffer* out, unsigned version, unsigned field,
    const char* bytes, size_t size, size_t cut, char whitespace, bool every_byte)
{
    static const char* prefix[] = {"FN:", "NOTE:", "N:", "ORG:", "item1.EMAIL:",
        "X-NOTE:", "FN;X-NOTE=\"", "NOTE:"};
    if (!xrtBufferInit(out) || !append(out, version == 3 ?
        "BEGIN:VCARD\r\nVERSION:3.0\r\n" : "BEGIN:VCARD\r\nVERSION:4.0\r\n")) return false;
    if (field != 0 && field != 6 && !append(out, "FN:keep\r\n")) return false;
    if (field == 7 && !append(out, "NOTE:first\r\n")) return false;
    if (!append(out, prefix[field])) return false;
    for (size_t i = 0; i <= size; ++i) {
        if ((every_byte && i && i < size) || (!every_byte && i == cut)) {
            if (!append(out, "\r\n") || !xrtBufferAppendByte(out, (uint8)whitespace)) return false;
        }
        if (i < size && !xrtBufferAppendByte(out, (uint8)bytes[i])) return false;
    }
    return append(out, field == 6 ? "\":keep\r\nEND:VCARD\r\n" : "\r\nEND:VCARD\r\n");
}

static void valid_case(unsigned version, unsigned field, size_t cut, char whitespace, bool every_byte)
{
    xmemdebugsnapshot before, after;
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    xbuffer input;
    if (!make_input(&input, version, field, payload, sizeof(payload) - 1, cut, whitespace, every_byte)) {
        check(false, "fixture construction failed"); return;
    }
    xvalue* card = xv_parse((char*)input.Data, input.Size);
    // 调用方输入可立即释放；映射结果必须拥有自己的字段和数组。
    xrtBufferUnit(&input);
    bool ok = correct(card, version, field) && !xrtGetError();
    check(ok, "valid folded value was rejected or changed");
    if (ok) {
        char* text = xv_build(card);
        xvalue* back = text ? xv_parse(text, strlen(text)) : NULL;
        check(text && xrtUtf8Valid((xstrview){text, strlen(text)}, NULL) && correct(back, version, field),
            "build/reparse lost the recovered value");
        xrtValueRelease(back);
        xrtFree(text);
    }
    xrtValueRelease(card);
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    check(balanced(&before, &after), "valid parse or roundtrip leaked memory");
    ++valid_count;
}

static void invalid_cases(unsigned version)
{
    static const struct { const char* bytes; size_t size; } invalid[] = {
        {"\x80",1}, {"\xc0\xaf",2}, {"\xed\xa0\x80",3}, {"\xf4\x90\x80\x80",4},
        {"\xc2",1}, {"\xe4\xbd",2}, {"\xf0\x9f\x98",3}, {"\xff",1},
        {"a\rb",3}, {"a\x7f",2}, {"a\0b",3}
    };
    for (unsigned field = 0; field < 8; ++field) {
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            for (unsigned folding = 0; folding < 2; ++folding) {
                xmemdebugsnapshot before, after;
                xrtClearError();
                xrtMemDebugSnapshot(&before);
                xbuffer input;
                if (!make_input(&input, version, field, invalid[i].bytes, invalid[i].size,
                    folding ? 1 : invalid[i].size + 1, ' ', false)) {
                    check(false, "invalid fixture construction failed"); return;
                }
                xvalue* card = xv_parse((char*)input.Data, input.Size);
                check(!card && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_VALUE,
                    "invalid logical UTF8/control/NUL was accepted");
                xrtValueRelease(card);
                xrtBufferUnit(&input);
                xrtClearError();
                xrtMemDebugSnapshot(&after);
                check(balanced(&before, &after), "invalid logical line leaked partial state");
                ++invalid_count;
            }
        }
    }
}

static void allocation_cases(unsigned version)
{
    for (unsigned field = 0; field < 8; ++field) {
        xbuffer input;
        if (!make_input(&input, version, field, payload, sizeof(payload) - 1, 0, '\t', true)) {
            check(false, "OOM fixture construction failed"); return;
        }
        xmemdebugsnapshot before, after;
        xrtClearError();
        xrtMemDebugSnapshot(&before);
        bool complete = false;
        for (unsigned budget = 0; budget < 256; ++budget) {
            if (!xrtMemDebugFailAfter(budget)) { check(false, "cannot inject OOM"); break; }
            xvalue* card = xv_parse((char*)input.Data, input.Size);
            bool injected = xrtMemDebugFailTriggered();
            bool memory = xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY;
            xrtMemDebugFailClear();
            check(injected ? !card && memory : correct(card, version, field) && !xrtGetError(),
                "folded parse lost OOM classification or value");
            xrtValueRelease(card);
            xrtClearError();
            card = xv_parse((char*)input.Data, input.Size);
            check(correct(card, version, field), "same input did not recover");
            xrtValueRelease(card);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "OOM parse did not balance memory");
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
    for (unsigned version = 3; version <= 4; ++version) {
        for (unsigned field = 0; field < 8; ++field) {
            for (size_t cut = 1; cut < sizeof(payload) - 1; ++cut) {
                valid_case(version, field, cut, ' ', false);
                valid_case(version, field, cut, '\t', false);
            }
            valid_case(version, field, 0, ' ', true);
        }
        invalid_cases(version);
        if (!vectors_only) allocation_cases(version);
    }
    if (!failures) printf("vcard UTF8 folding: %u valid, %u invalid, %u OOM PASS\n",
        valid_count, invalid_count, oom_count);
    return failures ? 1 : 0;
}
