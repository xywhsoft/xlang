/* robots.txt：按规则组保存数据；匹配时只扫描最相关的 user-agent 组。 */
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"

#define XSM_DOMAIN "xlang.extlib.sitemap"
#define XSM_ROBOTS_MAX_BYTES (1024u * 1024u)

typedef struct { const char *data; size_t size; } XsmSpan;

static void xsm_robots_error(const char *message) {
    xrtSetErrorInfo(XERR_VALUE, XSM_DOMAIN, 0, message);
}

static XsmSpan xsm_trim(XsmSpan span) {
    while (span.size && (span.data[0] == ' ' || span.data[0] == '\t' ||
                         span.data[0] == '\r')) { span.data++; span.size--; }
    while (span.size && (span.data[span.size - 1] == ' ' ||
                         span.data[span.size - 1] == '\t' ||
                         span.data[span.size - 1] == '\r')) span.size--;
    return span;
}

static int xsm_iequal(XsmSpan span, const char *text) {
    size_t i, size = strlen(text);
    if (span.size != size) return 0;
    for (i = 0; i < size; ++i)
        if (tolower((unsigned char)span.data[i]) != tolower((unsigned char)text[i])) return 0;
    return 1;
}

static xvalue *xsm_get(xvalue *object, const char *name) {
    int64 i, count;
    size_t size = strlen(name);
    if (object == NULL || xrtValueType(object) != XVALUE_OBJECT) return NULL;
    count = xrtValueCount(object);
    for (i = 0; i < count; ++i) {
        xstrview key;
        xvalue *value = xrtValueObjectAt(object, i, &key);
        if (key.Size == size && memcmp(key.Data, name, size) == 0) return value;
    }
    return NULL;
}

static int xsm_put(xvalue *object, const char *name, xvalue *value) {
    xstrview key = {name, strlen(name)};
    return value != NULL && xrtValueObjectSetNew(object, key, value);
}

static int xsm_add(xvalue *array, XsmSpan span) {
    xvalue *value = xrtValueString((xstrview){span.data, span.size});
    return value != NULL && xrtValueArrayAppendNew(array, value);
}

static xvalue *xsm_group(xvalue *groups) {
    xvalue *group = xrtValueObject();
    xvalue *view;
    if (group == NULL) return NULL;
    if (!xsm_put(group, "agents", xrtValueArray()) ||
        !xsm_put(group, "allow", xrtValueArray()) ||
        !xsm_put(group, "disallow", xrtValueArray())) {
        xrtValueRelease(group);
        return NULL;
    }
    if (!xrtValueArrayAppendNew(groups, group)) return NULL;
    view = xrtValueArrayAt(groups, xrtValueCount(groups) - 1);
    return view;
}

static xvalue *xsm_parse_robots(const char *text, size_t size) {
    xvalue *result, *groups, *sitemaps, *current = NULL;
    size_t cursor = 0;
    int rulesStarted = 0;
    xrtClearError();
    if (text == NULL) goto invalid;
    if (size > XSM_ROBOTS_MAX_BYTES) {
        xrtSetErrorInfo(XERR_RANGE, XSM_DOMAIN, 0,
            "robots.txt exceeds 1 MiB");
        return xrtValueNull();
    }
    if (memchr(text, 0, size) != NULL ||
        !xrtUtf8Valid((xstrview){text, size}, NULL)) goto invalid;
    /* UTF-8 文件开头的 BOM 不属于首个字段名。 */
    if (size >= 3 && memcmp(text, "\xef\xbb\xbf", 3) == 0) cursor = 3;
    result = xrtValueObject();
    if (result == NULL) return NULL;
    if (!xsm_put(result, "groups", xrtValueArray()) ||
        !xsm_put(result, "sitemaps", xrtValueArray())) goto fail;
    groups = xsm_get(result, "groups");
    sitemaps = xsm_get(result, "sitemaps");
    while (cursor < size) {
        size_t start = cursor, end, i, colon = SIZE_MAX;
        XsmSpan line, field, value;
        while (cursor < size && text[cursor] != '\n') cursor++;
        end = cursor;
        if (cursor < size) cursor++;
        for (i = start; i < end; ++i) {
            if (text[i] == '#') { end = i; break; }
        }
        line = xsm_trim((XsmSpan){text + start, end - start});
        /* 空行和注释可出现在组内；下一条 User-agent 才决定是否开始新组。 */
        if (!line.size) continue;
        for (i = 0; i < line.size; ++i)
            if (line.data[i] == ':') { colon = i; break; }
        if (colon == SIZE_MAX) continue;
        field = xsm_trim((XsmSpan){line.data, colon});
        value = xsm_trim((XsmSpan){line.data + colon + 1, line.size - colon - 1});
        if (xsm_iequal(field, "user-agent") && value.size) {
            if (current == NULL || rulesStarted) {
                current = xsm_group(groups);
                if (current == NULL) goto fail;
                rulesStarted = 0;
            }
            if (!xsm_add(xsm_get(current, "agents"), value)) goto fail;
        } else if (xsm_iequal(field, "sitemap") && value.size) {
            if (!xsm_add(sitemaps, value)) goto fail;
        } else if (current != NULL &&
                   (xsm_iequal(field, "allow") || xsm_iequal(field, "disallow"))) {
            xvalue *rules = xsm_get(current, xsm_iequal(field, "allow") ? "allow" : "disallow");
            rulesStarted = 1;
            /* 空 Disallow/Allow 不形成规则。 */
            if (value.size && !xsm_add(rules, value)) goto fail;
        }
    }
    return result;
fail:
    xrtValueRelease(result);
    return NULL;
invalid:
    xsm_robots_error("invalid robots.txt input");
    return xrtValueNull();
}

xvalue *xsm_robots_parse(const char *text) {
    size_t size = 0;
    if (text != NULL) {
        while (size <= XSM_ROBOTS_MAX_BYTES && text[size] != 0) size++;
    }
    return xsm_parse_robots(text, size);
}
xvalue *xsm_robots_parse_n(const char *text, size_t size) {
    return xsm_parse_robots(text, size);
}

static int xsm_matches_agent(XsmSpan agent, XsmSpan token) {
    size_t i;
    if (token.size == 1 && token.data[0] == '*') return 1;
    if (!token.size || agent.size != token.size) return 0;
    for (i = 0; i < token.size; ++i)
        if (tolower((unsigned char)agent.data[i]) !=
            tolower((unsigned char)token.data[i])) return 0;
    return 1;
}

static int64 xsm_agent_score(xvalue *group, XsmSpan agent) {
    xvalue *agents = xsm_get(group, "agents");
    int64 best = -1, i, count;
    if (agents == NULL || xrtValueType(agents) != XVALUE_ARRAY) return -1;
    count = xrtValueCount(agents);
    for (i = 0; i < count; ++i) {
        xvalue *item = xrtValueArrayAt(agents, i);
        xstrview view;
        if (item != NULL && xrtValueType(item) == XVALUE_STRING &&
            xrtValueGetString(item, &view) &&
            xsm_matches_agent(agent, (XsmSpan){view.Data, view.Size})) {
            int64 score = view.Size == 1 && view.Data[0] == '*' ? 0 : (int64)view.Size;
            if (score > best) best = score;
        }
    }
    return best;
}

/* * 可跨任意字节，末尾 $ 要求路径结束；无 $ 的规则按前缀匹配。 */
static int xsm_match(XsmSpan rule, XsmSpan path) {
    size_t r = 0, p = 0, star = SIZE_MAX, retry = 0;
    int anchored = rule.size && rule.data[rule.size - 1] == '$';
    if (anchored) rule.size--;
    while (p < path.size) {
        if (r == rule.size) {
            if (!anchored) return 1;
            if (star == SIZE_MAX) return 0;
            r = star; p = ++retry; continue;
        }
        if (rule.data[r] == '*') { star = ++r; retry = p; continue; }
        if (rule.data[r] == path.data[p]) { r++; p++; continue; }
        if (star != SIZE_MAX) { r = star; p = ++retry; continue; }
        return 0;
    }
    while (r < rule.size && rule.data[r] == '*') r++;
    return r == rule.size;
}

static size_t xsm_specificity(XsmSpan rule) {
    size_t i, score = 0;
    /* 只有尾部 $ 是锚点；中间的 $ 属于字面路径。 */
    if (rule.size && rule.data[rule.size - 1] == '$') rule.size--;
    for (i = 0; i < rule.size; ++i)
        if (rule.data[i] != '*') score++;
    return score;
}

static void xsm_score_rules(xvalue *group, const char *name, XsmSpan path,
                            size_t *best) {
    xvalue *list = xsm_get(group, name);
    int64 i, count;
    if (list == NULL || xrtValueType(list) != XVALUE_ARRAY) return;
    count = xrtValueCount(list);
    for (i = 0; i < count; ++i) {
        xvalue *item = xrtValueArrayAt(list, i);
        xstrview view;
        if (item != NULL && xrtValueType(item) == XVALUE_STRING &&
            xrtValueGetString(item, &view)) {
            XsmSpan rule = {view.Data, view.Size};
            size_t score = xsm_specificity(rule);
            if (xsm_match(rule, path) && (*best == SIZE_MAX || score > *best))
                *best = score;
        }
    }
}

long long xsm_robots_allows_n(xvalue *robots, const char *userAgent,
                              size_t agentSize, const char *path, size_t pathSize) {
    xvalue *groups = xsm_get(robots, "groups");
    int64 i, count, bestAgent = -1;
    size_t bestAllow = SIZE_MAX, bestDisallow = SIZE_MAX;
    XsmSpan agent, request;
    xrtClearError();
    if (groups == NULL || xrtValueType(groups) != XVALUE_ARRAY ||
        userAgent == NULL || path == NULL || memchr(userAgent, 0, agentSize) ||
        memchr(path, 0, pathSize) ||
        !xrtUtf8Valid((xstrview){userAgent, agentSize}, NULL) ||
        !xrtUtf8Valid((xstrview){path, pathSize}, NULL)) {
        xsm_robots_error("invalid robots decision input");
        return -1;
    }
    agent = (XsmSpan){userAgent, agentSize};
    request = (XsmSpan){path, pathSize};
    count = xrtValueCount(groups);
    for (i = 0; i < count; ++i) {
        int64 score = xsm_agent_score(xrtValueArrayAt(groups, i), agent);
        if (score > bestAgent) bestAgent = score;
    }
    if (bestAgent < 0) return 1;
    for (i = 0; i < count; ++i) {
        xvalue *group = xrtValueArrayAt(groups, i);
        if (xsm_agent_score(group, agent) != bestAgent) continue;
        xsm_score_rules(group, "allow", request, &bestAllow);
        xsm_score_rules(group, "disallow", request, &bestDisallow);
    }
    return bestDisallow != SIZE_MAX &&
           (bestAllow == SIZE_MAX || bestDisallow > bestAllow) ? 0 : 1;
}

long long xsm_robots_allows(xvalue *robots, const char *userAgent,
                            const char *path) {
    return xsm_robots_allows_n(robots, userAgent,
        userAgent ? strlen(userAgent) : 0, path, path ? strlen(path) : 0);
}
