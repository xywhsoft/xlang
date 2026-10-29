#include <assert.h>
#include <stdio.h>
#ifndef XLIR_GENERATOR_STORAGE_SOURCE
#define XLIR_GENERATOR_STORAGE_SOURCE "../../build/test/xlir_generator_payload_lifetime.c"
#endif
#include XLIR_GENERATOR_STORAGE_SOURCE
static int drops, finals, failures;
void payload_lifetime_drop(int64_t mode)
{
    ++drops;
    if (mode) {
        xl6_error_result failure = {false, mode == 1
            ? xrtErrorCreate(XERR_STATE, "payload lifetime", 73, "payload destructor failed") : NULL};
        assert(mode != 1 || failure.error);
        xl6ErrorScopePropagateFailure(failure);
    }
}
void payload_lifetime_finally(void) { ++finals; }
void payload_lifetime_prepare(void) { assert(xlir_module_init() && !xrtGetError()); }
void payload_lifetime_finish(void) { xlir_module_fini(); assert(!xrtGetError()); }
int payload_lifetime_failures(void) { return failures; }
void payload_lifetime_cycle(int kind, int stop, int mode)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    int64_t count; int ok;
    drops = finals = 0;
    assert(xl6ErrorScopeBegin(&scope));
    count = payload_lifetime_case(kind, stop, mode);
    outcome = xl6ErrorScopeEnd(&scope);
    ok = drops == 1 && finals == (stop < 0 ? 0 : 1);
    if (mode) ok = ok && !outcome.ok && (mode == 1
        ? outcome.error && xrtErrorCode(outcome.error) == 73 : !outcome.error);
    else ok = ok && outcome.ok && !outcome.error && count == (stop < 0 ? 0 : stop == 1 ? 1 : 2);
    if (!ok) {
        ++failures;
        fprintf(stderr, "payload lifetime mismatch kind=%d stop=%d mode=%d count=%lld drops=%d finals=%d ok=%d error=%d\n",
            kind, stop, mode, (long long)count, drops, finals, outcome.ok, outcome.error ? xrtErrorCode(outcome.error) : 0);
    }
    xrtErrorFree(outcome.error); assert(!xrtGetError());
}
