#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_close_first_outcome.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
#ifndef XLIR_GENERATOR_STORAGE_BINDING
#define XLIR_GENERATOR_STORAGE_BINDING "../../build/test/xlir_generator_close_first_outcome.h.bind.h"
#endif
#include XLIR_GENERATOR_STORAGE_BINDING
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
static int marks[8], markCount, failures;
void close_outcome_mark(int64_t id) { assert(markCount < 8); marks[markCount++] = (int)id; }
void close_outcome_fail(int64_t mode, int64_t code)
{
    if (mode) {
        xl6_error_result failure = {false, mode == 1 ?
            xrtErrorCreate(XERR_STATE, "close outcome", (int)code, "controlled failure") : NULL};
        assert(mode != 1 || failure.error); xl6ErrorScopePropagateFailure(failure);
    }
}
void close_outcome_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void close_outcome_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int close_outcome_failures(void) { return failures; }
void close_outcome_case(int consumer, int cleanup)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int primary = consumer ? consumer : cleanup;
    int code = consumer ? 31 : 42, ok;
    int64_t result;
    markCount = 0; assert(xl6ErrorScopeBegin(&scope));
    result = close_outcome_consume(consumer, cleanup);
    outcome = xl6ErrorScopeEnd(&scope);
    ok = primary ? !outcome.ok && (primary == 1 ?
        outcome.error && xrtErrorCode(outcome.error) == code : !outcome.error)
        : outcome.ok && !outcome.error && result == 7;
    ok = ok && markCount == 2 && marks[0] == 10 && marks[1] == 11;
    if (!ok) {
        ++failures;
        fprintf(stderr, "close first outcome mismatch consumer=%d cleanup=%d ok=%d code=%d result=%lld marks=%d\n",
            consumer, cleanup, outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : 0,
            (long long)result, markCount);
    }
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}

void close_outcome_repeated(int advances, int ambient, int cleanup)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    xlir_generator* generator;
    int pass, i, primary = ambient ? ambient : advances ? cleanup : 0;
    int expectedMarks = advances == 0 ? 0 : advances == 1 ? 2 : 3;
    markCount = 0; assert(xl6ErrorScopeBegin(&scope));
    generator = xlir_test_observe_generator_launch(CLOSE_OUTCOME_START(cleanup)); outcome = xl6ErrorScopeEnd(&scope);
    assert(outcome.ok && !outcome.error && generator);
    for (i = 0; i < advances; ++i) {
        xlir_generator_step step;
        assert(xl6ErrorScopeBegin(&scope)); step = xlir_generator_next(generator);
        outcome = xl6ErrorScopeEnd(&scope);
        assert(step.ok && step.value && outcome.ok && !outcome.error);
    }
    for (pass = 0; pass < 3; ++pass) {
        int ok, expected = pass == 0 ? primary : 0;
        assert(xl6ErrorScopeBegin(&scope));
        if (pass == 0) close_outcome_fail(ambient, 31);
        if (pass == 2) xlir_generator_destroy(generator);
        else {
            outcome = xlir_generator_close(generator);
            if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
            assert(generator->done && !generator->frame && !generator->has_item);
        }
        outcome = xl6ErrorScopeEnd(&scope);
        ok = expected ? !outcome.ok && (expected == 1 ?
            outcome.error && xrtErrorCode(outcome.error) == (ambient ? 31 : 42) : !outcome.error)
            : outcome.ok && !outcome.error;
        ok = ok && markCount == expectedMarks;
        if (advances == 1) ok = ok && marks[0] == 10 && marks[1] == 11;
        if (advances == 2) ok = ok && marks[0] == 90 && marks[1] == 10 && marks[2] == 11;
        if (!ok) {
            ++failures;
            fprintf(stderr, "repeated close mismatch state=%d ambient=%d cleanup=%d pass=%d ok=%d code=%d marks=%d\n",
                advances, ambient, cleanup, pass, outcome.ok,
                outcome.error ? xrtErrorCode(outcome.error) : 0, markCount);
        }
        xrtErrorFree(outcome.error); assert(!xrtGetError());
    }
}
