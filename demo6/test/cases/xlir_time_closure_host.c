/* Generated module is preincluded. No debug module or fake runtime required. */
#include <stdio.h>
int main(void)
{
    int initialized = xlir_module_init();
    if (!initialized) {
        const xerror* error = xrtGetError();
        if (error) fprintf(stderr, "time closure failure: %s\n", xrtErrorMessage(error));
    }
    xlir_module_fini();
    xrtClearError();
    return initialized ? 0 : 1;
}
