#include <assert.h>
#ifndef OWNER_VIEWS_SOURCE
#define OWNER_VIEWS_SOURCE "../../build/test/xlir_continuation_owner_views.c"
#define OWNER_VIEWS_BINDING "../../build/test/xlir_continuation_owner_views.h.bind.h"
#endif
#include OWNER_VIEWS_SOURCE
#include OWNER_VIEWS_BINDING
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
extern void continuation_owner_fault_arm(int);
extern unsigned continuation_owner_fault_end(int);
unsigned owner_views_fault(int fault)
{
    xlir_generator* generator = xlir_test_observe_generator_launch(OWNER_VIEWS_START("owner"));
    xlir_generator_step step; unsigned allocations;
    assert(generator); continuation_owner_fault_arm(fault);
    step = xlir_generator_next(generator); allocations = continuation_owner_fault_end(fault);
    if (fault >= 0) assert(!step.ok && !step.value && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else assert(step.ok && step.value && !xrtGetError());
    xlir_generator_destroy(generator); xrtClearError();
    return allocations;
}
void owner_views_cycle(int stop)
{
    xlir_generator* generator = xlir_test_observe_generator_launch(OWNER_VIEWS_START("owner"));
    xlir_generator_step step; int count = 0;
    assert(generator);
    for (;;) {
        char* item = NULL;
        step = xlir_generator_next(generator); assert(step.ok);
        if (!step.value) break;
        assert(xlir_generator_item_get(generator, &item, sizeof(item), true));
        assert(item && !strcmp(item, "owner")); xrtFree(item);
        if (++count == stop) break;
    }
    assert(count == (stop ? stop : 2)); xlir_generator_destroy(generator);
}
