#include "../include/progress_native.h"
#include "xrt.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "progress C test failed at line %d: %s\n", __LINE__, #test); \
    return 1; \
} } while (0)

static int test_error_recovery(void) {
    void *bar;
    char *rendered;
    CHECK(xprog_create(0) == NULL && xrtGetError() != NULL);
    bar = xprog_create(10);
    CHECK(bar != NULL && xrtGetError() == NULL);

    CHECK(xprog_advance(bar, -1) == 0 && xrtGetError() != NULL);
    CHECK(xprog_advance(bar, 1) == 1 && xrtGetError() == NULL);
    CHECK(xprog_set(NULL, 0) == 0 && xrtGetError() != NULL);
    CHECK(xprog_set(bar, 2) == 1 && xrtGetError() == NULL);
    CHECK(xprog_set_prefix_n(bar, "\n", 1) == 0 && xrtGetError() != NULL);
    CHECK(xprog_set_prefix_n(bar, "ok", 2) == 1 && xrtGetError() == NULL);
    CHECK(xprog_set_width(bar, 1) == 0 && xrtGetError() != NULL);
    CHECK(xprog_set_width(bar, 10) == 1 && xrtGetError() == NULL);

    CHECK(xprog_percent(NULL) == -1 && xrtGetError() != NULL);
    CHECK(xprog_percent(bar) == 20 && xrtGetError() == NULL);
    CHECK(xprog_is_done(NULL) == 0 && xrtGetError() != NULL);
    CHECK(xprog_is_done(bar) == 0 && xrtGetError() == NULL);
    CHECK(xprog_current(NULL) == 0 && xrtGetError() != NULL);
    CHECK(xprog_current(bar) == 2 && xrtGetError() == NULL);
    CHECK(xprog_total(NULL) == 0 && xrtGetError() != NULL);
    CHECK(xprog_total(bar) == 10 && xrtGetError() == NULL);
    CHECK(xprog_render(NULL) == NULL && xrtGetError() != NULL);
    rendered = (char *)xprog_render(bar);
    CHECK(rendered != NULL && xrtGetError() == NULL);
    xrtFree(rendered);
    xprog_destroy(bar);
    return 0;
}

static int test_large_thresholds(void) {
    void *bar = xprog_create(LLONG_MAX);
    int percent;
    CHECK(bar != NULL);
    for (percent = 1; percent < 100; ++percent) {
        long long threshold = (LLONG_MAX / 100) * percent +
            ((LLONG_MAX % 100) * percent + 99) / 100;
        CHECK(xprog_set(bar, threshold - 1) == 1);
        CHECK(xprog_percent(bar) == percent - 1);
        CHECK(xprog_set(bar, threshold) == 1);
        CHECK(xprog_percent(bar) == percent);
    }
    xprog_destroy(bar);
    return 0;
}

static unsigned next_random(unsigned *state) {
    *state = *state * 1664525u + 1013904223u;
    return *state;
}

static int test_ratio_properties(void) {
    unsigned state = 0x51a7u;
    int case_index;
    for (case_index = 0; case_index < 512; ++case_index) {
        long long total = 1 + (long long)(next_random(&state) % 1000000u);
        long long current = (long long)(next_random(&state) % (unsigned)(total + 1));
        int width = 10 + (int)(next_random(&state) % 91u);
        int filled = (int)(current * width / total);
        void *bar = xprog_create(total);
        char *rendered;
        char *open;
        int i;
        CHECK(bar != NULL);
        CHECK(xprog_set(bar, current) == 1 && xprog_set_width(bar, width) == 1);
        CHECK(xprog_percent(bar) == (int)(current * 100 / total));
        CHECK(xprog_is_done(bar) == (current == total));
        rendered = (char *)xprog_render(bar);
        CHECK(rendered != NULL);
        open = strchr(rendered, '[');
        CHECK(open != NULL && open[width + 1] == ']');
        /* 小整数独立计算期望值，逐列核对渲染进度。 */
        for (i = 0; i < width; ++i) {
            char expected = i < filled ? '=' : i == filled ? '>' : ' ';
            CHECK(open[i + 1] == expected);
        }
        xrtFree(rendered);
        xprog_destroy(bar);
    }
    return 0;
}

static int test_allocation_failure(void) {
    void *bar;
    char *rendered;
    size_t failure;
    for (failure = 0; failure < 2; ++failure) {
        CHECK(xrtMemDebugFailAfter(failure));
        bar = xprog_create(10);
        xrtMemDebugFailClear();
        CHECK(bar == NULL && xrtGetError() != NULL &&
              xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    }
    bar = xprog_create(10);
    CHECK(bar != NULL);
    CHECK(xprog_set_prefix_n(bar, "before", 6) == 1);
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(xprog_set_prefix_n(bar, "after", 5) == 0);
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    rendered = (char *)xprog_render(bar);
    CHECK(rendered != NULL && strstr(rendered, "before") != NULL);
    xrtFree(rendered);
    CHECK(xprog_set_prefix_n(bar, "after", 5) == 1);
    rendered = (char *)xprog_render(bar);
    CHECK(rendered != NULL && strstr(rendered, "after") != NULL);
    xrtFree(rendered);
    CHECK(xrtMemDebugFailAfter(0));
    CHECK(xprog_render(bar) == NULL);
    xrtMemDebugFailClear();
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    CHECK(xprog_current(bar) == 0 && xrtGetError() == NULL);
    rendered = (char *)xprog_render(bar);
    CHECK(rendered != NULL && strstr(rendered, "after") != NULL);
    xrtFree(rendered);
    xprog_destroy(bar);
    return 0;
}

int main(void) {
    void *first = xprog_create(100);
    void *second = xprog_create(LLONG_MAX);
    char prefix[1025];
    char *a, *b;
    CHECK(test_error_recovery() == 0);
    CHECK(test_large_thresholds() == 0);
    CHECK(test_ratio_properties() == 0);
    CHECK(test_allocation_failure() == 0);
    CHECK(first != NULL && second != NULL);
    CHECK(xprog_percent(first) == 0 && xprog_current(first) == 0);
    CHECK(xprog_advance(first, 25) == 1 && xprog_percent(first) == 25);
    CHECK(xprog_advance(first, -1) == 0 && xprog_current(first) == 25);
    xrtClearError();
    CHECK(xprog_set(first, -1) == 1 && xprog_current(first) == 0);
    CHECK(xprog_set(first, 200) == 1 && xprog_is_done(first));
    CHECK(xprog_set(first, 50) == 1 && xprog_set_width(first, 20) == 1);
    CHECK(xprog_set_width(first, 9) == 0);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "下载", strlen("下载")) == 1);
    a = (char *)xprog_render(first);
    b = (char *)xprog_render(first);
    CHECK(a != NULL && b != NULL && a != b &&
          strstr(a, "下载 [") != NULL && strstr(a, "50% (50/100)") != NULL);
    xrtFree(b);
    CHECK(strstr(a, "下载 [") != NULL);
    xrtFree(a);

    memset(prefix, 'x', 1024);
    prefix[1024] = 0;
    CHECK(xprog_set_prefix_n(first, prefix, 1024) == 1);
    a = (char *)xprog_render(first);
    CHECK(a != NULL && strlen(a) > 1024 && memcmp(a, prefix, 1024) == 0);
    xrtFree(a);
    CHECK(xprog_set_prefix_n(first, "a\0b", 3) == 0);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\nb", 3) == 0);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\tb", 3) == 0);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "\x1b[31m", 5) == 0);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\xe2\x80\xa8" "b", 5) == 0);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\xe2\x80\xae" "b", 5) == 0);
    CHECK(xrtGetError() != NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\xe2\x80\x8b" "b", 5) == 0); /* U+200B */
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\xe2\x81\xa0" "b", 5) == 0); /* U+2060 */
    xrtClearError();
    CHECK(xprog_set_prefix_n(first, "a\xef\xbb\xbf" "b", 5) == 0); /* U+FEFF */
    a = (char *)xprog_render(first);
    CHECK(a != NULL && memcmp(a, prefix, 1024) == 0);
    xrtFree(a);
    CHECK(xprog_set_prefix_n(first, "\xc0\xaf", 2) == 0);
    xrtClearError();

    CHECK(xprog_advance(second, LLONG_MAX) == 1 && xprog_is_done(second));
    CHECK(xprog_percent(second) == 100);
    CHECK(xprog_set(second, LLONG_MAX - 1) == 1 && xprog_percent(second) == 99);
    CHECK(xprog_advance(second, LLONG_MAX) == 1 && xprog_current(second) == LLONG_MAX);
    xprog_destroy(first);
    xprog_destroy(second);
    CHECK(xprog_is_done(NULL) == 0 && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    CHECK(xprog_current(NULL) == 0 && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    CHECK(xprog_total(NULL) == 0 && xrtGetError() != NULL &&
          xrtErrorKind(xrtGetError()) == XERR_VALUE);
    xrtClearError();
    CHECK(xprog_create(0) == NULL);
    xrtClearError();
    puts("progress C boundaries: PASS");
    return 0;
}
