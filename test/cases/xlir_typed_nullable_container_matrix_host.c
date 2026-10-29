#include <stdio.h>

#if !defined(XLIR_GENERATED_MODULE_PREINCLUDED)
    #ifndef XLIR_GENERATED_MODULE
        #error "XLIR_GENERATED_MODULE must name the generated C translation unit"
    #endif
    #include XLIR_GENERATED_MODULE
#endif

typedef struct xerror xerror;
extern const xerror* xrtGetError(void);
extern const char* xrtErrorMessage(const xerror* error);

int main(void)
{
    int code = xlang_main(NULL);
    if ( code != 0 ) {
        const xerror* error = xrtGetError();
        fprintf(stderr, "typed nullable container XLIR failed: code=%d error=%s\n",
            code, error != NULL ? xrtErrorMessage(error) : "(none)");
    }
    return code;
}
