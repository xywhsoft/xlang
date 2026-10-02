/* Generated C is preincluded only in this translation unit. */
#include <stdio.h>
#if defined(XL6_TIME_USER_ENTRY)
extern xvalue* xrtValueArray(void);
extern void xrtValueRelease(xvalue* value);
#endif
int main(void)
{
    int initialized = xlir_module_init();
#if defined(XL6_TIME_USER_ENTRY)
    if (initialized) {
        xvalue* arguments = xrtValueArray();
        if (!arguments) initialized = 0;
        else {
            int code = xlang_main(arguments);
            xrtValueRelease(arguments);
            initialized = code == 0 && !xrtGetError();
        }
    }
#endif
    if (!initialized) {
        const xerror* error = xrtGetError();
        if (error) fprintf(stderr, "time text closure failure: %s\n", xrtErrorMessage(error));
    }
    xlir_module_fini(); xrtClearError();
    return initialized ? 0 : 1;
}
