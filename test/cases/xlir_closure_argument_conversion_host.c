#include <stdio.h>

/* Included after the generated module. Check finalization and reinitialization
 * as well as the assertions executed by the argument-conversion fixture. */
int main(void)
{
    int cycle;
    for (cycle = 0; cycle < 3; ++cycle) {
        if (!xlir_module_init()) {
            const xerror* error = xrtGetError();
            fprintf(stderr, "closure argument conversion failed: %s\n",
                error != NULL ? xrtErrorMessage(error) : "missing error");
            return 1;
        }
        xlir_module_fini();
        xlir_module_fini();
        if (xrtGetError() != NULL) return 2;
    }
    return 0;
}
