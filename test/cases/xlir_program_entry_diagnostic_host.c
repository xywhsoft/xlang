#include <stdio.h>

#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

/* Keep the reusable host independent of generated-module feature discovery. */
extern xvalue* xrtValueArray(void);
extern void xrtValueRelease(xvalue*);

int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    const xerror* error = xrtGetError();
    if ( result == 0 ) return 0;
    fprintf(stderr, "XLIR program failed: %s\n",
            error != NULL ? xrtErrorMessage(error) : "no runtime error");
    return result;
}
