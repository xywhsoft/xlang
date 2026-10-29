#include <stdio.h>

#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

int main(void)
{
    xvalue* arguments = xrtValueArray();
    int result;
    if ( arguments == NULL ) return 1;
    result = xlang_main(arguments);
    xrtValueRelease(arguments);
    const xerror* error = xrtGetError();
    if ( result == 0 ) return 0;
    fprintf(stderr,
            "XLIR Stack runtime failed: result=%d error=%s data=%s at %s:%d:%d\n",
            result,
            error != NULL ? xrtErrorMessage(error) : "no runtime error",
            error != NULL && xrtErrorData(error) != NULL
                ? xrtErrorData(error) : "",
            error != NULL && xrtErrorFile(error) != NULL
                ? xrtErrorFile(error) : "",
            error != NULL ? (int)xrtErrorLine(error) : 0,
            error != NULL ? (int)xrtErrorColumn(error) : 0);
    return result;
}
