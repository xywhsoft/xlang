#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_close_catch_dispatch.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int marks[30], markCount, failures;
void close_catch_mark(int64_t id) { assert(markCount < 30); marks[markCount++] = (int)id; }
void close_catch_raise(int64_t mode)
{
    if (mode) {
        xl6_error_result failure = {false, mode == 3 ? NULL :
            xrtErrorCreate(mode == 2 ? XERR_TYPE : XERR_STATE, "catch dispatch", 42, "cleanup failure")};
        assert(mode == 3 || failure.error); xl6ErrorScopePropagateFailure(failure);
    }
}
void close_catch_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void close_catch_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int close_catch_failures(void) { return failures; }
void close_catch_case(int form, int mode, int limit)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int expected[30], n = 0, consumed = 1, closing = limit == 1, i, ok;
    /* The existing language error-kind ABI maps an absent diagnostic to
     * RuntimeError. A false/NULL failure is therefore caught by this handler,
     * not silently treated as success before handler dispatch. */
    int handled = mode == 1 || mode == 3 || (mode == 2 && (form & 1));
    int failed = mode && !handled;
    int64_t result;
    markCount = 0; assert(xl6ErrorScopeBegin(&scope));
    switch (form) {
    case 0: result = close_catch_consume_0(mode, limit); break;
    case 1: result = close_catch_consume_1(mode, limit); break;
    case 2: result = close_catch_consume_2(mode, limit); break;
    case 3: result = close_catch_consume_3(mode, limit); break;
    case 4: result = close_catch_consume_4(mode, limit); break;
    case 5: result = close_catch_consume_5(mode, limit); break;
    case 6: result = close_catch_consume_6(mode, limit); break;
    default: result = close_catch_consume_7(mode, limit); break;
    }
    outcome = xl6ErrorScopeEnd(&scope);
    expected[n++] = 10;
    if (!mode) expected[n++] = 11;
    if (handled) {
        expected[n++] = mode == 2 ? 30 : 20;
        if (form < 4 && !closing) { ++consumed; closing = consumed == limit; }
        if (form >= 4 || !closing) expected[n++] = mode == 2 ? 31 : 21;
    }
    if (form & 2) {
        expected[n++] = 40;
        if (!closing) { ++consumed; closing = consumed == limit; }
        expected[n++] = 41;
    }
    if (!failed && !closing) { expected[n++] = 90; ++consumed; }
    ok = failed ? !outcome.ok && (mode == 3 ? !outcome.error :
        outcome.error && xrtErrorKind(outcome.error) == XERR_TYPE && xrtErrorCode(outcome.error) == 42)
        : outcome.ok && !outcome.error && result == consumed;
    ok = ok && markCount == n;
    for (i = 0; i < n && i < markCount; ++i) ok = ok && marks[i] == expected[i];
    if (!ok) {
        ++failures;
        fprintf(stderr, "catch close mismatch form=%d mode=%d limit=%d ok=%d code=%d result=%lld actual=[",
            form, mode, limit, outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : 0, (long long)result);
        for (i = 0; i < markCount; ++i) fprintf(stderr, "%s%d", i ? "," : "", marks[i]);
        fputs("] expected=[", stderr);
        for (i = 0; i < n; ++i) fprintf(stderr, "%s%d", i ? "," : "", expected[i]);
        fputs("]\n", stderr);
    }
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}
