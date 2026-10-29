#include "xlir_cell_allocation_outcome.c.suspension.c"
int main(void)
{
    if (!xlir_module_init()) return 2;
    xlir_module_fini();
    return xrtGetError() != NULL ? 3 : 0;
}
