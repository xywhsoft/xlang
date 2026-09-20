#include <stdio.h>

#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

int main(void)
{
    int initialized = xlir_module_init();
    const xerror* error = xrtGetError();
    if ( initialized == 1 ) return 0;
    fprintf(stderr, "XLIR module initialization failed: %s\n",
            error != NULL ? xrtErrorMessage(error) : "no runtime error");
    return 1;
}
