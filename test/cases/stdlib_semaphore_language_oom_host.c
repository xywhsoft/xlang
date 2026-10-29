/* Fail each actual allocation in the generated checked language entry.
 * The physical owner result determines success, not diagnostic allocation.
 * XRT can publish a static OOM diagnostic without allocating. No provider
 * is replaced, and no fake return/carrier or guessed allocator count is used. */
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#if !defined(XLIR_GENERATED_MODULE)
#error "An actual generated language module is required"
#endif
#include XLIR_GENERATED_MODULE
int main(void)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    assert(!xl6ProductMemoryEnd());
    assert(xl_script_module_init(NULL));
    xvalue* arguments = xrtValueArray(); assert(arguments);
    unsigned hits = 0, withoutDiagnostic = 0;
    bool complete = false;
    for (size_t point = 0; point < 128; ++point) {
        assert(!xrtGetError());
        xl6ProductMemoryBegin(point);
        int code = -99;
        bool ok = xl_script_main_checked(arguments, &code) != 0;
        bool hit = xl6ProductMemoryEnd();
        if (hit) {
            ++hits;
            if (!xrtGetError()) ++withoutDiagnostic;
            if (ok || code != 1) {
                fprintf(stderr, "OOM silently succeeded at allocation %zu: ok=%d code=%d\n", point, ok, code);
                return 1;
            }
        } else {
            assert(ok && code == 0 && !xrtGetError());
            complete = true;
        }
        xrtClearError();
        xl6ProductMemoryVerify();
        if (complete) break;
    }
    assert(complete && hits);
    xrtValueRelease(arguments);
    assert(xl_script_module_unit_checked());
    assert(!xrtGetError());
    printf("actual language OOM: %u allocation prefixes, %u without diagnostic; checked failure and lifetimes balanced\n",
        hits, withoutDiagnostic);
    return 0;
}
