#include "../../build/test/xlir_virtual_call_outcome.c"
#include "../../build/test/xlir_virtual_call_outcome.bind.h"
#include <assert.h>
#include <stdio.h>
void virtual_outcome_cycle(void)
{
    int ok = xlir_module_init();
    assert(XLIR_VIRTUAL_SLOT_COUNT == 3);
    if (!ok || xrtGetError()) fprintf(stderr, "virtual outcome at %d: %s\n",
        xrtErrorLine(xrtGetError()), xrtErrorMessage(xrtGetError()));
    assert(ok && !xrtGetError());
    xlir_module_fini(); assert(!xrtGetError());
}
