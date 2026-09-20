#include <stdio.h>
#include "../../build/test/xlir_module_scalar_finalization.c"
static int initialized;
int64_t module_scalar_seed(void) { return ++initialized; }
int main(void)
{
    xl_script_module_unit();
    if (!xl_script_module_init(NULL) || !xl_script_module_init(NULL) || initialized != 1) return 1;
    xl_script_module_unit();
    xl_script_module_unit();
    if (!xl_script_module_init(NULL) || initialized != 2) {
        fputs("owner-free module cannot reinitialize after unload\n", stderr);
        return 2;
    }
    xl_script_module_unit();
    puts("scalar module finalization: owner-free boundary resets initialization exactly once");
    return 0;
}
