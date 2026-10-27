#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_close_extended.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int marks[40], markCount, failures;
void close_extended_mark(int64_t id)
{ assert(markCount < 40); marks[markCount++] = (int)id; }
void close_extended_raise(int64_t mode)
{
    if (mode) {
        xl6_error_result outcome = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "close extended", 42, "cleanup failed") : NULL};
        assert(mode != 1 || outcome.error); xl6ErrorScopePropagateFailure(outcome);
    }
}
void close_extended_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void close_extended_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int close_extended_failures(void) { return failures; }
void close_extended_case(int form, int mode, int limit)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int64_t result; int expected[40], count = 0, value = 0, ok = 1, i;
    markCount = 0; assert(xl6ErrorScopeBegin(&scope));
    if (!form) result = close_extended_unstarted();
    else if (form == 1) result = close_extended_protected(mode, limit);
    else result = close_extended_caught(mode, limit);
    outcome = xl6ErrorScopeEnd(&scope);
    if (!form) { value = 42; }
    else if (form == 1) {
        expected[count++] = 10;
        if (limit != 1) expected[count++] = 11;
        expected[count++] = 20;
        if (!mode) {
            expected[count++] = 21;
            if (limit >= 3) expected[count++] = 30;
        }
        expected[count++] = 40; expected[count++] = 41;
        if (!mode && limit >= 4) expected[count++] = 50;
        value = limit < 4 ? limit : 4;
    } else {
        int items = mode ? 4 : 3;
        if (mode && limit > 1) {
            expected[count++] = 20;
            if (limit > 2) expected[count++] = 21;
        }
        expected[count++] = 40; expected[count++] = 41;
        if (limit >= items) expected[count++] = 50;
        value = limit < items ? limit : items;
    }
    if (form == 1 && mode)
        ok = !outcome.ok && (mode == 1 ? outcome.error && xrtErrorCode(outcome.error) == 42 : !outcome.error);
    else ok = outcome.ok && !outcome.error && result == value;
    ok = ok && markCount == count;
    for (i = 0; i < count && i < markCount; ++i) ok = ok && marks[i] == expected[i];
    if (!ok) {
        ++failures;
        fprintf(stderr, "extended close mismatch form=%d mode=%d limit=%d ok=%d code=%d result=%lld actual=[",
            form, mode, limit, outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : 0, (long long)result);
        for (i = 0; i < markCount; ++i) fprintf(stderr, "%s%d", i ? "," : "", marks[i]);
        fputs("] expected=[", stderr);
        for (i = 0; i < count; ++i) fprintf(stderr, "%s%d", i ? "," : "", expected[i]);
        fputs("]\n", stderr);
    }
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}
