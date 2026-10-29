#include "../../build/test/xlir_indexed_accessor.c"
#include "../../build/test/xlir_indexed_accessor.bind.h"
#include <assert.h>
#include <stdio.h>
void indexed_accessor_raise(int64_t code)
{
    xl6_error_result failure = {false, NULL};
    failure.error = xrtErrorCreate(XERR_STATE, "indexed-accessor", (int)code, "indexed accessor failed");
    assert(failure.error); xl6ErrorScopePropagateFailure(failure);
}
void indexed_accessor_cycle(void)
{
    const int errors[] = {70, 103, 71};
    size_t i;
    assert(INDEXED_ACCESSOR_COUNT >= 10 && xlir_module_init() && !xrtGetError());
    assert(indexed_accessor_check() == 0);
    if (xrtGetError()) fprintf(stderr, "indexed accessor check at %d: %d: %s\n",
        xrtErrorLine(xrtGetError()), xrtErrorCode(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(!xrtGetError() && indexed_accessor_balanced());
    for (i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        indexed_accessor_uncaught((int64_t)i);
        assert(xrtGetError() && xrtErrorCode(xrtGetError()) == errors[i]);
        xrtErrorFree(xrtTakeError());
        assert(indexed_accessor_balanced() && !xrtGetError());
    }
    xlir_module_fini(); assert(!xrtGetError());
}
