#include <stdio.h>

/* The quality gate generates this translation unit immediately before build. */
#include "../../build/test/cgen_error_value_lifetime_xlir.c"

int main(void)
{
    const int initialized = xlir_module_init();
    const xerror* error = xrtGetError();
    if ( initialized == 1 ) return 0;
    fprintf(stderr, "XLIR error lifetime failed at %d:%d: %s\n",
            error != NULL ? xrtErrorLine(error) : 0,
            error != NULL ? xrtErrorColumn(error) : 0,
            error != NULL ? xrtErrorMessage(error) : "no runtime error");
    return 1;
}
