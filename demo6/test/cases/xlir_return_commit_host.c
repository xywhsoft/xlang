#include <assert.h>
#include "../../build/test/xlir_return_commit.c"

void xlir_return_prepare(void) { assert(xlir_module_init()); }
void xlir_return_run(void)
{
    assert(xlir_return_probe() == 41);
    assert(xrtGetError() == NULL);
}
void xlir_return_finish(void) { xlir_module_fini(); }
