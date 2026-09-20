#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_cleanup_detach_reentry.c"

int main(void)
{
    int cycle;
    for (cycle = 0; cycle < 3; ++cycle) {
        xlir_cleanup_detach_observed = 0;
        if (!xlir_module_init()) return 2;
        xlir_module_fini();
        if (xlir_cleanup_detach_observed != 1) {
            fprintf(stderr, "cleanup owner was still attached during destroy: %lld\n",
                (long long)xlir_cleanup_detach_observed);
            return 1;
        }
        xlir_module_fini();
    }
    return 0;
}
