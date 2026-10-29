#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_managed_storage_inputs.c"
void managed_storage_case(int choose)
{
    xl6_error_scope boundary = {0}; xl6_error_result result; int64_t value;
    assert(xrtGetError() == NULL && xlir_module_init() && xl6ErrorScopeBegin(&boundary));
    value = managed_storage_probe(choose != 0);
    result = xl6ErrorScopeEnd(&boundary);
    if (!result.ok) fprintf(stderr, "managed storage choose=%d code=%d line=%d message=%s\n",
        choose, xrtErrorCode(result.error), xrtErrorLine(result.error), xrtErrorMessage(result.error));
    assert(result.ok && result.error == NULL && value == 43);
    xlir_module_fini(); assert(xrtGetError() == NULL);
}
