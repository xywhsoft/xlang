/* Preinclude the actual generated program; link a real XRT/resident runtime.
 * One process per current-thread logical allocation prefix, through first no-hit.
 * This does not claim that an unrestricted C harness uses a pruned native plan. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "test/support/module_product_memory.h"
extern int32_t xrtErrorLine(const xerror*);
extern int32_t xrtErrorColumn(const xerror*);
#if defined(XL6_PRODUCT_USER_ENTRY)
/* Generated programs that do not construct arrays need not declare this
 * independent host argument factory. */
extern xvalue* xrtValueArray(void);
#endif
int main(int argc, char** argv)
{
    assert(argc == 1 || argc == 2);
    size_t point = argc == 2 ? (size_t)strtoull(argv[1], NULL, 10) : SIZE_MAX;
    xl6ProductMemoryBegin(point);
    int initialized = xlir_module_init();
#if defined(XL6_PRODUCT_USER_ENTRY)
    if (initialized) {
        xvalue* arguments = xrtValueArray();
        if (!arguments) initialized = 0;
        else {
            initialized = xlang_main(arguments) == 0 && !xrtGetError();
            xrtValueRelease(arguments);
        }
    }
#endif
    bool hit = xl6ProductMemoryEnd();
    xerror* failure = xrtTakeError();
    if (hit ? initialized || xrtErrorKind(failure) != XERR_MEMORY : !initialized || failure)
        fprintf(stderr, "point=%zu initialized=%d hit=%d kind=%d line=%d column=%d message=%s\n", point,
            initialized, hit, (int)xrtErrorKind(failure), (int)xrtErrorLine(failure),
            (int)xrtErrorColumn(failure), failure ? xrtErrorMessage(failure) : "none");
    assert(hit ? !initialized && xrtErrorKind(failure) == XERR_MEMORY : initialized && !failure);
    xlir_module_fini();
    assert(!xrtGetError() && !xlir_module_initialized);
    xlir_module_fini(); assert(!xrtGetError());
    xrtErrorFree(failure); xrtClearError();
    xl6ProductMemoryVerify();
    printf("module product point=%zu hit=%d balanced\n", point, hit);
    return 0;
}
