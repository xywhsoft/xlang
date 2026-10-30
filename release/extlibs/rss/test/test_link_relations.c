/* Atom 链接关系的短名称与 IANA IRI 等价；不对任意 IRI 做猜测性归一化。 */
#define XRT_MODULE_ALL
#include "../include/rss_native.h"
#include <stdio.h>
#include <string.h>

static unsigned failures, cases, oom;

static void check(bool ok, const char *message) {
    if (!ok) { ++failures; fprintf(stderr, "rss relations: %s\n", message); }
}

static xvalue *field(xvalue *object, const char *key) {
    return xrtValueObjectGet(object, (xstrview){key, strlen(key)});
}

static bool text_is(xvalue *value, const char *expected) {
    xstrview text;
    size_t size = strlen(expected);
    return value && xrtValueGetString(value, &text) && text.Size == size &&
        memcmp(text.Data, expected, size) == 0;
}

static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

/* 每份输入同时测 feed/entry；解析后释放输入，再读取拥有型结果。 */
static char *document(const char *links, unsigned names) {
    const char *prefix = names == 1 ? "a:" : "";
    const char *ns = names == 1 ? "xmlns:a='http://www.w3.org/2005/Atom'" :
        "xmlns='http://www.w3.org/2005/Atom'";
    char converted[4096], xml[12288];
    size_t i = 0, j = 0;
    int length;
    /* @ 是测试模板中 Atom 前缀的占位，不参与生产解析。 */
    while (links[i] && j + 4 < sizeof(converted)) {
        if (links[i] == '@') {
            size_t n = strlen(prefix);
            memcpy(converted + j, prefix, n);
            j += n;
            ++i;
        } else converted[j++] = links[i++];
    }
    if (links[i]) return NULL;
    converted[j] = 0;
    length = snprintf(xml, sizeof(xml),
        "<%sfeed %s xmlns:x='urn:foreign'><%stitle>A &amp; B</%stitle>%s"
        "<%sentry><%stitle><![CDATA[C < D]]></%stitle>%s</%sentry></%sfeed>",
        prefix, ns, prefix, prefix, converted,
        prefix, prefix, prefix, converted, prefix, prefix);
    if (length < 0 || (size_t)length >= sizeof(xml)) return NULL;
    return (char *)xrtStrDup((str)xml);
}

static bool correct(xvalue *value, const char *link) {
    xvalue *items, *entry;
    if (!value || xrtValueType(value) != XVALUE_OBJECT ||
        !text_is(field(value, "title"), "A & B") || !text_is(field(value, "link"), link)) return false;
    items = field(value, "items");
    if (!items || xrtValueType(items) != XVALUE_ARRAY || xrtValueCount(items) != 1) return false;
    entry = xrtValueArrayAt(items, 0);
    return text_is(field(entry, "title"), "C < D") && text_is(field(entry, "link"), link);
}

int main(void) {
    static const struct { const char *links, *expected; } inputs[] = {
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/alternate' href='page'/>", "page"},
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/alternate' href='first'/><@link rel='alternate' href='second'/>", "first"},
        {"<@link rel='self' href='self'/><@link rel='alternate' href='first'/><@link rel='http://www.iana.org/assignments/relation/alternate' href='second'/>", "first"},
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/altern&#97;te' href='a&amp;b'/>", "a&b"},
        {"<@link rel='self' href='self'/><@link href='implicit'/>", "implicit"},
        {"<@link rel='self' href='self'/><@link rel='alternate' href='short'/>", "short"},
        {"<@link rel='self' href='first'/><@link rel='hub' href='other'/>", "first"},
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/alternate'/><@link href='present'/>", "present"},
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/alternate' href=''/><@link href='later'/>", ""},
        {"<x:link href='foreign'/><@link rel='self' href='self'/><@link href='page'/>", "page"},
        {"<x:link href='foreign'/>", ""},
        {"<x:box><@link href='nested'/></x:box><@link rel='self' href='self'/>", "self"},
        {"<@link x:href='ignored'/><@link rel='self' href='self'/>", "self"},
        {"<@link rel='self' href='self'/><@link x:rel='self' href='implicit'/>", "implicit"},
        {"<@link rel='self' href='self'/><@link rel='https://www.iana.org/assignments/relation/alternate' href='other'/>", "self"},
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/alternate/' href='other'/>", "self"},
        {"<@link rel='self' href='self'/><@link rel='http://www.iana.org/assignments/relation/Alternate' href='other'/>", "self"},
        {"<@link rel='self' href='self'/><@link rel=' alternate ' href='other'/>", "self"},
        {"<@link rel='self' href='self'/><@link rel='' href='other'/>", "self"},
        {"", ""}
    };
    xmemdebugsnapshot before, after;
    unsigned names, mode, i;
    check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "enable memory tracking");
    /* 预热 XRT 的进程级错误状态；后续比较每条成功和失败路径。 */
    xrtSetErrorInfo(XERR_VALUE, "rss.test", 0, "warmup");
    xrtClearError();
    for (names = 0; names < 2; ++names) {
        for (mode = 0; mode < 2; ++mode) {
            for (i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
                char *xml;
                xvalue *value;
                xrtMemDebugSnapshot(&before);
                xml = document(inputs[i].links, names);
                check(xml != NULL, "fixture allocation");
                if (!xml) return 1;
                value = mode ? xrss_parse(xml, (int64)strlen(xml)) : xrss_parse_atom(xml, (int64)strlen(xml));
                xrtFree(xml);
                check(correct(value, inputs[i].expected), "feed/entry link or owned text mismatch");
                check(xrtGetError() == NULL, "valid document has error");
                xrtValueRelease(value);
                xrtMemDebugSnapshot(&after);
                check(balanced(&before, &after), "successful parse leaked or invalidly freed");
                ++cases;
            }
        }
    }
    /* 扫描真实 XRT 分配点，不能把未触发的失败预算当作 OOM 覆盖。 */
    for (mode = 0; mode < 2; ++mode) {
        char *xml = document(inputs[0].links, mode);
        unsigned budget;
        bool complete = false;
        check(xml != NULL, "OOM fixture allocation");
        if (!xml) return 1;
        for (budget = 0; budget < 512; ++budget) {
            xvalue *value;
            bool triggered;
            xrtMemDebugSnapshot(&before);
            check(xrtMemDebugFailAfter(budget), "set allocation failure");
            value = mode ? xrss_parse(xml, (int64)strlen(xml)) : xrss_parse_atom(xml, (int64)strlen(xml));
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                check(value && xrtValueType(value) == XVALUE_NULL, "OOM did not return null value");
                check(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM lost its error kind");
                ++oom;
            } else {
                check(correct(value, "page") && xrtGetError() == NULL, "recovered parse mismatch");
                complete = true;
            }
            xrtValueRelease(value);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "OOM parse leaked or invalidly freed");
            if (complete) break;
        }
        check(complete, "OOM scan did not reach successful parse");
        xrtFree(xml);
    }
    printf("rss relations: %u cases, %u real OOM, %u failures\n", cases, oom, failures);
    return failures ? 1 : 0;
}
