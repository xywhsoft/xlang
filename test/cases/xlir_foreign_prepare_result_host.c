/* Generated C is preincluded with only the foreign Begin boundary replaced. */
#include <assert.h>
#include <stdio.h>
#undef xl6ForeignCallBegin
extern bool xl6ForeignCallBegin(xl6_foreign_error_scope*);
extern bool xrtMemDebugEnable(bool);
extern uint64_t prepare_live_count(void), prepare_live_bytes(void);
typedef struct xpromise xpromise;
typedef struct xcancel xcancel;
extern xpromise* xrtPromiseCreate(xfuture**, xcancel*);
extern bool xrtPromiseResolve(xpromise*, void*);
extern void xrtPromiseDestroy(xpromise*);
static unsigned attempts, invoked;
static int64_t payload;
bool prepare_result_begin(xl6_foreign_error_scope* scope)
{
    ++attempts;
    if (attempts == 1) return xl6ForeignCallBegin(scope);
    assert(attempts <= 3 && xrtGetError() == NULL);
    if (attempts == 3) xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "prepare-result", 74, "not entered"));
    return false;
}
xfuture* prepare_future(void)
{
    xfuture* future = NULL; xpromise* promise = xrtPromiseCreate(&future, NULL);
    ++invoked; assert(promise);
    assert(xrtPromiseResolve(promise, &payload));
    xrtPromiseDestroy(promise); return future;
}
int main(void)
{
    unsigned round;
    assert(xrtMemDebugEnable(true));
    for (round = 0; round < 200; ++round) {
        uint64_t count = prepare_live_count(), bytes = prepare_live_bytes();
        attempts = invoked = 0;
        assert(xlir_module_init());
        assert(attempts == 3 && invoked == 1 && xrtGetError() == NULL);
        xlir_module_fini();
        assert(xlir_module_initialized == 0 && prepare_live_count() == count && prepare_live_bytes() == bytes);
    }
    puts("foreign preparation result: 200 loop transactions, one entered call then two skipped calls, empty owner result, false/NULL and diagnostic failure, balanced memory");
    return 0;
}
