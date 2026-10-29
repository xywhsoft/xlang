#include "../../build/test/xlir_callsite_evaluation.c"
#include "../../build/test/xlir_callsite_evaluation.bind.h"
#include <assert.h>
#include <stdio.h>
void callsite_evaluation_raise(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "callsite-evaluation", (int)code, "callsite operation failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
void callsite_evaluation_cycle(void)
{
    const int syntaxErrors[] = {103, 101, 90, 91};
    size_t i;
    assert(CALLSITE_PROTOCOL_COUNT >= 5);
    assert(xlir_module_init() && !xrtGetError());
    assert(callsite_evaluation_check() == 0);
    if (xrtGetError()) fprintf(stderr, "callsite check at %d: %d: %s\n",
        xrtErrorLine(xrtGetError()), xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(!xrtGetError());
    callsite_evaluation_uncaught(false);
    assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 101);
    xrtErrorFree(xrtTakeError());
    callsite_evaluation_uncaught(true);
    assert(xrtGetError() && xrtErrorCode(xrtGetError()) == 90);
    xrtErrorFree(xrtTakeError());
    for (i = 0; i < sizeof(syntaxErrors) / sizeof(syntaxErrors[0]); ++i) {
        callsite_evaluation_syntax_uncaught((int64_t)i);
        assert(xrtGetError() && xrtErrorCode(xrtGetError()) == syntaxErrors[i]);
        xrtErrorFree(xrtTakeError());
    }
    xlir_module_fini(); assert(!xrtGetError());
}
