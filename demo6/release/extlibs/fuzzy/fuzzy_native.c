/* Unicode 编辑距离交给 XRT；本包只负责有界子序列评分和候选选择。 */
#ifndef XRT_MODULE_UNICODE_DISTANCE
#define XRT_MODULE_UNICODE_DISTANCE
#endif
#ifndef XRT_MODULE_STRING
#define XRT_MODULE_STRING
#endif
#include "xrt.h"
#include "include/fuzzy_native.h"

#define XFUZZY_DOMAIN "xlang.extlib.fuzzy"
#define XFUZZY_MAX_SCALARS 4096u
#define XFUZZY_MAX_MATCH_BYTES (1024u * 1024u)
#define XFUZZY_MAX_CANDIDATES (8u * 1024u * 1024u)

static void xfuzzy_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XFUZZY_DOMAIN, 0, message);
}

/* 先按公开上限找终止符，避免无界 strlen 早于限额检查。 */
static bool xfuzzy_view(const char* text, size_t limit, const char* message,
    xstrview* output)
{
    size_t size = 0u;
    if (text == NULL) {
        xfuzzy_error(XERR_ARGUMENT, "text pointer is null");
        return false;
    }
    while (size <= limit && text[size] != '\0') ++size;
    if (size > limit) {
        xfuzzy_error(XERR_RANGE, message);
        return false;
    }
    *output = xrtStrViewN(text, size);
    return true;
}

static bool xfuzzy_distance_input(xstrview text, size_t* count)
{
    if (text.Size > 4u * XFUZZY_MAX_SCALARS) {
        xfuzzy_error(XERR_RANGE, "distance input exceeds 4096 Unicode scalars");
        return false;
    }
    *count = xrtUtf8Count(text);
    if (*count == XRT_NPOS) {
        xfuzzy_error(XERR_VALUE, "input is not valid UTF-8");
        return false;
    }
    if (*count > XFUZZY_MAX_SCALARS) {
        xfuzzy_error(XERR_RANGE, "distance input exceeds 4096 Unicode scalars");
        return false;
    }
    return true;
}

static bool xfuzzy_match_input(xstrview text)
{
    if (text.Size > XFUZZY_MAX_MATCH_BYTES) {
        xfuzzy_error(XERR_RANGE, "match input exceeds 1 MiB");
        return false;
    }
    if (!xrtUtf8Valid(text, NULL)) {
        xfuzzy_error(XERR_VALUE, "input is not valid UTF-8");
        return false;
    }
    return true;
}

int64_t xfuzzy_levenshtein(const char* a, const char* b)
{
    xstrview left, right;
    size_t left_count, right_count, distance;
    xrtClearError();
    if (!xfuzzy_view(a, 4u * XFUZZY_MAX_SCALARS,
            "distance input exceeds 4096 Unicode scalars", &left) ||
        !xfuzzy_view(b, 4u * XFUZZY_MAX_SCALARS,
            "distance input exceeds 4096 Unicode scalars", &right)) return -1;
    if (!xfuzzy_distance_input(left, &left_count) ||
        !xfuzzy_distance_input(right, &right_count)) return -1;
    distance = xrtUtf8Distance(left, right, XRT_NPOS);
    return distance == XRT_NPOS ? -1 : (int64_t)distance;
}

int64_t xfuzzy_ratio(const char* a, const char* b)
{
    xstrview left, right;
    size_t left_count, right_count, maximum, distance;
    xrtClearError();
    if (!xfuzzy_view(a, 4u * XFUZZY_MAX_SCALARS,
            "distance input exceeds 4096 Unicode scalars", &left) ||
        !xfuzzy_view(b, 4u * XFUZZY_MAX_SCALARS,
            "distance input exceeds 4096 Unicode scalars", &right)) return -1;
    if (!xfuzzy_distance_input(left, &left_count) ||
        !xfuzzy_distance_input(right, &right_count)) return -1;
    maximum = left_count > right_count ? left_count : right_count;
    if (maximum == 0u) return 100;
    distance = xrtUtf8Distance(left, right, XRT_NPOS);
    return distance == XRT_NPOS ? -1 :
        (int64_t)(((maximum - distance) * 100u) / maximum);
}

static uint32 xfuzzy_ascii_fold(uint32 scalar)
{
    return scalar >= 'A' && scalar <= 'Z' ? scalar + ('a' - 'A') : scalar;
}

static bool xfuzzy_separator(uint32 scalar)
{
    return scalar == '_' || scalar == '-' || scalar == ' ' || scalar == '/';
}

/* 输入已验证 UTF-8；按 Unicode 标量评分，ASCII 字母忽略大小写。 */
static long long xfuzzy_score_view(xstrview pattern, xstrview text)
{
    size_t pattern_at = 0u, text_at = 0u, index = 0u;
    size_t last_match = XRT_NPOS, consecutive = 0u;
    uint32 previous = 0u, expected;
    size_t expected_size;
    long long score = 0;
    if (pattern.Size == 0u) return 0;
    if (xrtUtf8Decode(pattern, &expected, &expected_size) != XUTF_OK)
        return -2;
    while (pattern_at < pattern.Size && text_at < text.Size) {
        uint32 current;
        size_t current_size;
        xstrview remainder = xrtStrViewN(text.Data + text_at, text.Size - text_at);
        if (xrtUtf8Decode(remainder, &current, &current_size) != XUTF_OK)
            return -2;
        if (xfuzzy_ascii_fold(expected) == xfuzzy_ascii_fold(current)) {
            if (last_match != XRT_NPOS && index == last_match + 1u) {
                ++consecutive;
                score += (long long)(consecutive * 6u);
            } else {
                consecutive = 0u;
            }
            if (index == 0u || xfuzzy_separator(previous)) score += 5;
            ++score;
            last_match = index;
            pattern_at += expected_size;
            if (pattern_at < pattern.Size) {
                remainder = xrtStrViewN(pattern.Data + pattern_at,
                    pattern.Size - pattern_at);
                if (xrtUtf8Decode(remainder, &expected, &expected_size) != XUTF_OK)
                    return -2;
            }
        }
        previous = current;
        text_at += current_size;
        ++index;
    }
    if (pattern_at != pattern.Size) return -1;
    /* 完整命中比同分的前缀命中更靠前。 */
    if (pattern.Size == text.Size) ++score;
    return score;
}

int64_t xfuzzy_match(const char* pattern, const char* text)
{
    xstrview query, target;
    xrtClearError();
    if (!xfuzzy_view(pattern, XFUZZY_MAX_MATCH_BYTES,
            "match input exceeds 1 MiB", &query) ||
        !xfuzzy_view(text, XFUZZY_MAX_MATCH_BYTES,
            "match input exceeds 1 MiB", &target)) return -2;
    if (!xfuzzy_match_input(query) || !xfuzzy_match_input(target)) return -2;
    return xfuzzy_score_view(query, target);
}

void* xfuzzy_best(const char* pattern, const char* candidates)
{
    xstrview query, list;
    xstrview best = xrtStrViewN("", 0u);
    long long best_score = -1;
    size_t start = 0u;
    xrtClearError();
    if (!xfuzzy_view(pattern, XFUZZY_MAX_MATCH_BYTES,
            "match input exceeds 1 MiB", &query) ||
        !xfuzzy_view(candidates, XFUZZY_MAX_CANDIDATES,
            "candidate list exceeds 8 MiB", &list)) return NULL;
    if (!xfuzzy_match_input(query)) return NULL;
    if (!xrtUtf8Valid(list, NULL)) {
        xfuzzy_error(XERR_VALUE, "candidate list is not valid UTF-8");
        return NULL;
    }
    while (start < list.Size) {
        size_t end = start;
        xstrview candidate;
        long long score;
        while (end < list.Size && list.Data[end] != '\n') ++end;
        if (end < list.Size && end > start && list.Data[end - 1u] == '\r') --end;
        candidate = xrtStrViewN(list.Data + start, end - start);
        if (candidate.Size > XFUZZY_MAX_MATCH_BYTES) {
            xfuzzy_error(XERR_RANGE, "candidate exceeds 1 MiB");
            return NULL;
        }
        score = xfuzzy_score_view(query, candidate);
        if (score < -1) {
            xfuzzy_error(XERR_VALUE, "candidate is not valid UTF-8");
            return NULL;
        }
        if (score > best_score) { best_score = score; best = candidate; }
        start = end;
        if (start < list.Size && list.Data[start] == '\r') ++start;
        if (start < list.Size && list.Data[start] == '\n') ++start;
    }
    return xrtStrDupView(best);
}
