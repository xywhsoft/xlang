/* Preinclude the unmodified actual generated C. The checked status, not a
 * diagnostic pointer or process exit-code convention, is the failure witness.
 * Allocation failure may prevent allocating the first diagnostic object. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "test/support/module_product_memory.h"
#if defined(XL6_PRODUCT_USER_ENTRY)
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
            int code = -1;
            initialized = xl_script_main_checked(arguments, &code);
            if (initialized) assert(code == 0);
            xrtValueRelease(arguments);
        }
    }
#endif
    bool hit = xl6ProductMemoryEnd();
    xerror* failure = xrtTakeError();
    bool valid = hit ? !initialized && (!failure || xrtErrorKind(failure) == XERR_MEMORY)
                     : initialized && !failure;
    if (!valid)
        fprintf(stderr, "point=%zu initialized=%d hit=%d kind=%d message=%s\n", point,
            initialized, hit, (int)xrtErrorKind(failure), failure ? xrtErrorMessage(failure) : "none");
    assert(valid);
    /* Retain the first diagnostic independently while checking that module
     * cleanup neither replaces it nor manufactures a secondary error. */
    xlir_module_fini();
    assert(!xrtGetError() && !xlir_module_initialized);
    xlir_module_fini();
    assert(!xrtGetError());
    if (failure) assert(xrtErrorKind(failure) == XERR_MEMORY);
    xrtErrorFree(failure);
    xl6ProductMemoryVerify();
    printf("checked language product point=%zu hit=%d balanced\n", point, hit);
    return 0;
}
