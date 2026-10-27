#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_close_abrupt_finally.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int marks[30], markCount, failures;
void close_abrupt_mark(int64_t id)
{ assert(markCount < 30); marks[markCount++] = (int)id; }
void close_abrupt_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void close_abrupt_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int close_abrupt_failures(void) { return failures; }
void close_abrupt_case(int form, int repeat, int limit)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int expected[30], n = 0, iterations = repeat ? 2 : 1, consumed, i, ok;
    int items = iterations + (form ? 2 : 1);
    int64_t result;
    markCount = 0; assert(xl6ErrorScopeBegin(&scope));
    result = form == 1 ? close_abrupt_internal(repeat != 0, limit)
        : form == 2 ? close_abrupt_outer(repeat != 0, limit)
        : close_abrupt_consume(repeat != 0, limit);
    outcome = xl6ErrorScopeEnd(&scope);
    if (form == 1) expected[n++] = 1;
    for (i = 0; i < iterations && (form == 1 || i < limit); ++i) {
        expected[n++] = 10 + i; expected[n++] = 20 + i; expected[n++] = 30 + i;
    }
    if (form == 1) { expected[n++] = 50; expected[n++] = 51; }
    if (form == 2) {
        if (limit > iterations) expected[n++] = 80;
        expected[n++] = 40; expected[n++] = 41;
    }
    if (limit >= items) expected[n++] = 90;
    consumed = limit < items ? limit : items;
    ok = outcome.ok && !outcome.error && result == consumed && markCount == n;
    for (i = 0; i < n && i < markCount; ++i) ok = ok && marks[i] == expected[i];
    if (!ok) {
        ++failures;
        fprintf(stderr, "abrupt finally close mismatch form=%d repeat=%d limit=%d ok=%d result=%lld actual=[",
            form, repeat, limit, outcome.ok, (long long)result);
        for (i = 0; i < markCount; ++i) fprintf(stderr, "%s%d", i ? "," : "", marks[i]);
        fputs("] expected=[", stderr);
        for (i = 0; i < n; ++i) fprintf(stderr, "%s%d", i ? "," : "", expected[i]);
        fputs("]\n", stderr);
    }
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}
