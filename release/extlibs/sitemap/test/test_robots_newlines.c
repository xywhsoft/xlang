/* RFC 9309 允许 CR、LF、CRLF；逐组核对结构、决策及失败清理。 */
#define XRT_MODULE_ALL
#include "../include/sitemap_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned failures, cases, oom;

static void check(bool ok, const char *why) {
    if (!ok) { ++failures; fprintf(stderr, "robots newlines: %s\n", why); }
}

static xvalue *field(xvalue *object, const char *key) {
    return xrtValueObjectGet(object, (xstrview){key, strlen(key)});
}

static bool text_is(xvalue *value, const char *expected) {
    xstrview text;
    size_t size = strlen(expected);
    return value && xrtValueGetString(value, &text) && text.Size == size &&
        !memcmp(text.Data, expected, size);
}

static bool balanced(const xmemdebugsnapshot *a, const xmemdebugsnapshot *b) {
    return a->LiveCount == b->LiveCount && a->LiveBytes == b->LiveBytes &&
        a->InvalidFreeCount == b->InvalidFreeCount && a->DoubleFreeCount == b->DoubleFreeCount &&
        a->UseAfterFreeCount == b->UseAfterFreeCount;
}

static bool append(xbuffer *out, const char *text) {
    return xrtBufferAppend(out, (xbytesview){(cbytes)text, strlen(text)});
}

static char *document(unsigned ending, bool bom, bool final_newline, size_t *size) {
    static const char *const lines[] = {
        "# policy", "User-agent: Agent", "User-agent: Mirror",
        "Disallow: /private # hidden", "Allow: /private/open",
        "Sitemap: https://example.test/site.xml", "", "# keep group",
        "Disallow: /hidden", "User-agent: Other", "Disallow: /other",
        "User-agent: Agent", "Disallow: /again",
        "Sitemap: https://example.test/news.xml", "User-agent: *", "Disallow: /fallback"
    };
    static const char *const endings[] = {"\n", "\r", "\r\n"};
    xbuffer out;
    unsigned i;
    if (!xrtBufferInit(&out)) return NULL;
    if (bom && !append(&out, "\xef\xbb\xbf")) goto fail;
    for (i = 0; i < sizeof(lines) / sizeof(lines[0]); ++i) {
        unsigned kind = ending < 3 ? ending : (ending == 3 ? i % 3 : 2 - i % 3);
        if (!append(&out, lines[i])) goto fail;
        if ((final_newline || i + 1 < sizeof(lines) / sizeof(lines[0])) &&
            !append(&out, endings[kind])) goto fail;
    }
    *size = out.Size;
    if (!xrtBufferAppendByte(&out, 0)) goto fail;
    return (char *)xrtBufferTake(&out, NULL, NULL);
fail:
    xrtBufferUnit(&out);
    return NULL;
}

static bool correct(xvalue *robots) {
    static const struct { const char *agent, *path; long long allowed; } decisions[] = {
        {"Agent", "/private/a", 0}, {"aGeNt", "/private/open/page", 1},
        {"Mirror", "/private/a", 0}, {"Agent", "/hidden/a", 0},
        {"Mirror", "/hidden/a", 0}, {"Other", "/other/a", 0},
        {"Agent", "/again", 0}, {"Mirror", "/again", 1},
        {"Unknown", "/private/a", 1}, {"Unknown", "/fallback/a", 0},
        {"Agent", "/fallback/a", 1}, {"Agent", "/safe", 1}
    };
    xvalue *groups, *maps, *group, *agents;
    size_t i;
    if (!robots || xrtValueType(robots) != XVALUE_OBJECT) return false;
    groups = field(robots, "groups");
    maps = field(robots, "sitemaps");
    if (!groups || xrtValueType(groups) != XVALUE_ARRAY || xrtValueCount(groups) != 4 ||
        !maps || xrtValueType(maps) != XVALUE_ARRAY || xrtValueCount(maps) != 2 ||
        !text_is(xrtValueArrayAt(maps, 0), "https://example.test/site.xml") ||
        !text_is(xrtValueArrayAt(maps, 1), "https://example.test/news.xml")) return false;
    group = xrtValueArrayAt(groups, 0);
    agents = field(group, "agents");
    if (!agents || xrtValueCount(agents) != 2 ||
        !text_is(xrtValueArrayAt(agents, 0), "Agent") || !text_is(xrtValueArrayAt(agents, 1), "Mirror") ||
        xrtValueCount(field(group, "disallow")) != 2 || xrtValueCount(field(group, "allow")) != 1) return false;
    for (i = 0; i < sizeof(decisions) / sizeof(decisions[0]); ++i) {
        const char *agent = decisions[i].agent, *path = decisions[i].path;
        if (xsm_robots_allows(robots, agent, path) != decisions[i].allowed ||
            xsm_robots_allows_n(robots, agent, strlen(agent), path, strlen(path)) != decisions[i].allowed ||
            xrtGetError() != NULL) return false;
    }
    return true;
}

int main(void) {
    unsigned ending, bom, tail, mode;
    xmemdebugsnapshot before, after;
    check(xrtMemDebugEnabled() || xrtMemDebugEnable(true), "enable memory tracking");
    xrtSetErrorInfo(XERR_VALUE, "sitemap.test", 0, "warmup");
    xrtClearError();
    for (ending = 0; ending < 5; ++ending) for (bom = 0; bom < 2; ++bom)
        for (tail = 0; tail < 2; ++tail) for (mode = 0; mode < 2; ++mode) {
            size_t size;
            char *input;
            xvalue *robots;
            xrtMemDebugSnapshot(&before);
            input = document(ending, bom != 0, tail != 0, &size);
            check(input != NULL, "fixture allocation");
            if (!input) return 1;
            if (mode) {
                robots = xsm_robots_parse(input);
                xrtFree(input);
            } else {
                /* 精确长度且无 NUL 的 CRT 块，让 ASan 能发现末尾 CR 越界读。 */
                char *bounded = (char *)malloc(size);
                check(bounded != NULL, "bounded fixture allocation");
                if (!bounded) { xrtFree(input); return 1; }
                memcpy(bounded, input, size);
                xrtFree(input);
                robots = xsm_robots_parse_n(bounded, size);
                free(bounded);
            }
            check(correct(robots), "owned groups or decisions mismatch");
            xrtValueRelease(robots);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "successful parse leaked or invalidly freed");
            ++cases;
        }
    for (mode = 0; mode < 2; ++mode) {
        size_t size;
        char *input = document(1, true, false, &size);
        unsigned budget;
        bool complete = false;
        check(input != NULL, "OOM fixture allocation");
        if (!input) return 1;
        for (budget = 0; budget < 512; ++budget) {
            xvalue *robots;
            bool triggered;
            xrtMemDebugSnapshot(&before);
            check(xrtMemDebugFailAfter(budget), "set allocation failure");
            robots = mode ? xsm_robots_parse(input) : xsm_robots_parse_n(input, size);
            triggered = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (triggered) {
                check(!robots || xrtValueType(robots) == XVALUE_NULL, "OOM returned partial groups");
                check(xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY, "OOM lost error kind");
                ++oom;
            } else {
                check(correct(robots), "recovery mismatch");
                complete = true;
            }
            xrtValueRelease(robots);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            check(balanced(&before, &after), "OOM parse leaked or invalidly freed");
            if (complete) break;
        }
        check(complete, "OOM scan did not reach success");
        xrtFree(input);
    }
    printf("robots newlines: %u cases, %u real OOM, %u failures\n", cases, oom, failures);
    return failures ? 1 : 0;
}
