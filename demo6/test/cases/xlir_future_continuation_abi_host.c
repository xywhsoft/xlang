#include <stdio.h>

#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

int main(void)
{
    int result = xlir_module_init();
    if ( result != 1 ) {
        const xerror* error = xrtGetError();
        fprintf(stderr,
            "XLIR Future continuation module failed: kind=%d domain=%s operation=%s data=%s message=%s source=%s:%d:%d\n",
            error != NULL ? (int)xrtErrorKind(error) : -1,
            error != NULL && xrtErrorDomain(error) != NULL
                ? xrtErrorDomain(error) : "",
            error != NULL && xrtErrorOperation(error) != NULL
                ? xrtErrorOperation(error) : "",
            error != NULL && xrtErrorData(error) != NULL
                ? xrtErrorData(error) : "",
            error != NULL && xrtErrorMessage(error) != NULL
                ? xrtErrorMessage(error) : "no runtime error",
            error != NULL && xrtErrorFile(error) != NULL
                ? xrtErrorFile(error) : "",
            error != NULL ? (int)xrtErrorLine(error) : 0,
            error != NULL ? (int)xrtErrorColumn(error) : 0);
        return 1;
    }
    return 0;
}
