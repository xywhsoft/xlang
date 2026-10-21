/* 有界行级 LCS：输入不截断，超过明确预算时直接报错。 */
#ifndef XRT_MODULE_HASH64
#define XRT_MODULE_HASH64
#endif
#ifndef XRT_MODULE_STRING_FORMAT
#define XRT_MODULE_STRING_FORMAT
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "xrt.h"
#include "include/diff_native.h"
#include <stdint.h>
#include <string.h>

#define XDIFF_DOMAIN "xlang.extlib.diff"
#define XDIFF_MAX_TEXT (8u * 1024u * 1024u)
#define XDIFF_MAX_LINES 2048u

typedef struct xdiff_line {
    const char* data;
    size_t size;
    uint64 hash;
} xdiff_line;

typedef struct xdiff_step {
    xdiff_line line;
    char prefix;
} xdiff_step;

typedef struct xdiff_plan {
    xdiff_line* old_lines;
    xdiff_line* new_lines;
    uint16_t* table;
    xdiff_step* steps;
    size_t old_count;
    size_t new_count;
    size_t step_count;
    size_t added;
    size_t deleted;
    size_t unchanged;
} xdiff_plan;

static void xdiff_error(const char* message)
{
    xrtSetErrorInfo(XERR_RANGE, XDIFF_DOMAIN, 0, message);
}

static void xdiff_unit(xdiff_plan* plan)
{
    xrtFree(plan->steps);
    xrtFree(plan->table);
    xrtFree(plan->new_lines);
    xrtFree(plan->old_lines);
}

static bool xdiff_split(const char* text, xdiff_line** output, size_t* count)
{
    size_t size = 0u, lines;
    size_t i, start = 0u, next = 0u;
    xdiff_line* result;

    while (size <= XDIFF_MAX_TEXT && text[size] != '\0') ++size;
    if (size > XDIFF_MAX_TEXT) {
        xdiff_error("text exceeds 8 MiB");
        return false;
    }
    if (!xrtUtf8Valid((xstrview){text, size}, NULL)) {
        xrtSetErrorInfo(XERR_VALUE, XDIFF_DOMAIN, 0, "text is not valid UTF-8");
        return false;
    }
    lines = size != 0u ? 1u : 0u;
    for (i = 0u; i + 1u < size; ++i)
        if (text[i] == '\n') ++lines;
    if (lines > XDIFF_MAX_LINES) {
        xdiff_error("text exceeds 2048 lines");
        return false;
    }
    result = lines != 0u ? (xdiff_line*)xrtMalloc(lines * sizeof(*result)) : NULL;
    if (lines != 0u && result == NULL) return false;
    for (i = 0u; i <= size && next < lines; ++i) {
        size_t end;
        if (i != size && text[i] != '\n') continue;
        end = i;
        if (end > start && text[end - 1u] == '\r') --end;
        result[next].data = text + start;
        result[next].size = end - start;
        result[next].hash = xrtHash64(text + start, end - start);
        ++next;
        start = i + 1u;
    }
    *output = result;
    *count = lines;
    return true;
}

static bool xdiff_equal(const xdiff_line* a, const xdiff_line* b)
{
    return a->size == b->size && a->hash == b->hash &&
        memcmp(a->data, b->data, a->size) == 0;
}

static bool xdiff_prepare(const char* old_text, const char* new_text, xdiff_plan* plan)
{
    size_t i, j, stride;
    xrtClearError();
    memset(plan, 0, sizeof(*plan));
    if (!xdiff_split(old_text != NULL ? old_text : "",
        &plan->old_lines, &plan->old_count) ||
        !xdiff_split(new_text != NULL ? new_text : "",
        &plan->new_lines, &plan->new_count)) goto fail;

    stride = plan->new_count + 1u;
    plan->table = (uint16_t*)xrtCalloc((plan->old_count + 1u) * stride,
        sizeof(uint16_t));
    plan->steps = (xdiff_step*)xrtMalloc((plan->old_count + plan->new_count + 1u) *
        sizeof(xdiff_step));
    if (plan->table == NULL || plan->steps == NULL) goto fail;

    for (i = 1u; i <= plan->old_count; ++i) {
        for (j = 1u; j <= plan->new_count; ++j) {
            uint16_t above = plan->table[(i - 1u) * stride + j];
            uint16_t left = plan->table[i * stride + j - 1u];
            plan->table[i * stride + j] = xdiff_equal(&plan->old_lines[i - 1u],
                &plan->new_lines[j - 1u]) ?
                (uint16_t)(plan->table[(i - 1u) * stride + j - 1u] + 1u) :
                (above > left ? above : left);
        }
    }

    i = plan->old_count;
    j = plan->new_count;
    while (i != 0u || j != 0u) {
        xdiff_step* step = &plan->steps[plan->step_count++];
        if (i != 0u && j != 0u &&
            xdiff_equal(&plan->old_lines[i - 1u], &plan->new_lines[j - 1u])) {
            step->prefix = ' ';
            step->line = plan->old_lines[--i];
            --j;
            ++plan->unchanged;
        } else if (j != 0u && (i == 0u ||
            plan->table[i * stride + j - 1u] >= plan->table[(i - 1u) * stride + j])) {
            step->prefix = '+';
            step->line = plan->new_lines[--j];
            ++plan->added;
        } else {
            step->prefix = '-';
            step->line = plan->old_lines[--i];
            ++plan->deleted;
        }
    }
    return true;
fail:
    xdiff_unit(plan);
    memset(plan, 0, sizeof(*plan));
    return false;
}

void* xdiff_lines(const char* old_text, const char* new_text)
{
    xdiff_plan plan;
    size_t i, size = 0u, position = 0u;
    char* result;
    if (!xdiff_prepare(old_text, new_text, &plan)) return NULL;
    for (i = 0u; i < plan.step_count; ++i)
        size += plan.steps[i].line.size + 2u;
    result = (char*)xrtMalloc(size + 1u);
    if (result == NULL) { xdiff_unit(&plan); return NULL; }
    for (i = plan.step_count; i > 0u; --i) {
        const xdiff_step* step = &plan.steps[i - 1u];
        result[position++] = step->prefix;
        memcpy(result + position, step->line.data, step->line.size);
        position += step->line.size;
        result[position++] = '\n';
    }
    result[position] = '\0';
    xdiff_unit(&plan);
    return result;
}

void* xdiff_stats(const char* old_text, const char* new_text)
{
    xdiff_plan plan;
    void* result;
    if (!xdiff_prepare(old_text, new_text, &plan)) return NULL;
    result = xrtFormat("%llu,%llu,%llu", (unsigned long long)plan.added,
        (unsigned long long)plan.deleted, (unsigned long long)plan.unchanged);
    xdiff_unit(&plan);
    return result;
}
