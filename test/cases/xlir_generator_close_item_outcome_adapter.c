#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_close_item_outcome.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int marks[8], markCount, drops[8], dropCount, failures;
void close_item_mark(int64_t id) { assert(markCount < 8); marks[markCount++] = (int)id; }
void close_item_fail(int64_t mode, int64_t code)
{
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ?
            xrtErrorCreate(XERR_STATE, "close item", (int)code, "controlled failure") : NULL};
        assert(mode != 1 || failure.error); xl6ErrorScopePropagateFailure(failure);
    }
}
void close_item_drop(int64_t mode, int64_t code)
{ assert(dropCount < 8); drops[dropCount++] = (int)code; close_item_fail(mode, code); }
void close_item_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void close_item_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int close_item_failures(void) { return failures; }
void close_item_case(int consumer, int item, int cleanup)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int primary = consumer ? consumer : item ? item : cleanup;
    int code = consumer ? 31 : item ? 73 : 42, ok;
    int64_t result;
    markCount = 0; dropCount = 0; assert(xl6ErrorScopeBegin(&scope));
    result = close_item_consume(consumer, item, cleanup);
    outcome = xl6ErrorScopeEnd(&scope);
    ok = primary ? !outcome.ok && (primary == 1 ?
        outcome.error && xrtErrorCode(outcome.error) == code : !outcome.error)
        : outcome.ok && !outcome.error && result == 7;
    ok = ok && markCount == 2 && marks[0] == 10 && marks[1] == 11 &&
        dropCount == 2 && drops[0] == 73 && drops[1] == 74;
    if (!ok) {
        ++failures;
        fprintf(stderr, "close item mismatch consumer=%d item=%d cleanup=%d ok=%d code=%d result=%lld marks=%d drops=%d\n",
            consumer, item, cleanup, outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : 0,
            (long long)result, markCount, dropCount);
    }
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}
