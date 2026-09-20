#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_close_routes.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int routeMarks[32], routeMarkCount, routeFailures;
void generator_close_mark(int64_t id)
{ assert(routeMarkCount < 32); routeMarks[routeMarkCount++] = (int)id; }
void generator_close_raise(int64_t mode)
{
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ? xrtErrorCreate(XERR_STATE, "generator close", 42, "pending failure") : NULL};
        assert(mode != 1 || failure.error); xl6ErrorScopePropagateFailure(failure);
    }
}
void generator_close_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void generator_close_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int generator_close_cases_failed(void) { return routeFailures; }
void generator_close_case(int form, int mode, int limit)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome; int64_t result;
    int expected[32], count = 0, resultCount = 0, ok = 1, i;
    routeMarkCount = 0;
    assert(xl6ErrorScopeBegin(&scope));
    if (form == 0) result = generator_close_consume(mode, limit);
    else if (form == 1) result = generator_close_nested_consume(limit);
    else if (form == 2) result = generator_close_pending_consume(mode);
    else { assert(form == 3); result = generator_close_return_consume(); }
    outcome = xl6ErrorScopeEnd(&scope);
    if (form == 0) {
        resultCount = mode ? 3 : 2;
        if (limit < resultCount) resultCount = limit;
        expected[count++] = 10;
        if (limit != 1) expected[count++] = 11;
        expected[count++] = 20;
        if (mode) expected[count++] = 21;
        expected[count++] = 22;
        if (limit >= (mode ? 3 : 2)) expected[count++] = 30;
    } else if (form == 1) {
        resultCount = limit < 4 ? limit : 4;
        expected[count++] = 20; expected[count++] = 40; expected[count++] = 41; expected[count++] = 21;
        if (limit >= 4) expected[count++] = 30;
    } else {
        expected[count++] = 20; expected[count++] = 21; resultCount = form == 2 ? 7 : 9;
    }
    if (form == 2 && mode) {
        ok = !outcome.ok && (mode == 1 ? outcome.error && xrtErrorCode(outcome.error) == 42 : !outcome.error);
    } else ok = outcome.ok && !outcome.error && result == resultCount;
    ok = ok && count == routeMarkCount;
    for (i = 0; i < count && i < routeMarkCount; ++i) ok = ok && expected[i] == routeMarks[i];
    if (!ok) {
        ++routeFailures;
        fprintf(stderr, "close route mismatch form=%d mode=%d limit=%d: ok=%d error=%d result=%lld actual=[",
            form, mode, limit, outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : 0, (long long)result);
        for (i = 0; i < routeMarkCount; ++i) fprintf(stderr, "%s%d", i ? "," : "", routeMarks[i]);
        fputs("] expected=[", stderr);
        for (i = 0; i < count; ++i) fprintf(stderr, "%s%d", i ? "," : "", expected[i]);
        fputs("]\n", stderr);
    }
    if (outcome.error) xrtErrorFree(outcome.error);
    assert(!xrtGetError());
}
