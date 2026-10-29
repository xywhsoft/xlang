#include <assert.h>
#include "../../build/test/xlir_continuation_owner_cfg.c"
#include "../../build/test/xlir_continuation_owner_cfg.h.bind.h"
#define XLIR_TEST_LAUNCH_GENERATOR
#include "xlir_launch_test.h"
extern void continuation_owner_fault_arm(int);
extern unsigned continuation_owner_fault_end(int);
unsigned owner_cfg_fault(int kind, int choice, int steps, int fault)
{
    xlir_generator* generator = kind == 0 ? xlir_test_observe_generator_launch(OWNER_CFG_BRANCH("owner", choice != 0))
        : kind == 1 ? xlir_test_observe_generator_launch(OWNER_CFG_SKIP("owner", choice != 0)) : kind == 2 ? xlir_test_observe_generator_launch(OWNER_CFG_LOOP("owner", choice))
        : xlir_test_observe_generator_launch(OWNER_CFG_STORE("owner", choice));
    xlir_generator_step step; unsigned allocations; int i;
    assert(generator);
    for (i = 0; i < steps; ++i) {
        char* item = NULL; step = xlir_generator_next(generator); assert(step.ok && step.value);
        assert(xlir_generator_item_get(generator, &item, sizeof(item), true));
        assert(item && !strcmp(item, "owner")); xrtFree(item);
    }
    continuation_owner_fault_arm(fault);
    step = xlir_generator_next(generator); allocations = continuation_owner_fault_end(fault);
    if (fault >= 0) assert(!step.ok && !step.value && xrtGetError() && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
    else assert(step.ok && step.value && !xrtGetError());
    xlir_generator_destroy(generator); xrtClearError();
    return allocations;
}
void owner_cfg_cycle(int kind, int choice, int stop)
{
    xlir_generator* generator = kind == 0 ? xlir_test_observe_generator_launch(OWNER_CFG_BRANCH("owner", choice != 0))
        : kind == 1 ? xlir_test_observe_generator_launch(OWNER_CFG_SKIP("owner", choice != 0)) : kind == 2 ? xlir_test_observe_generator_launch(OWNER_CFG_LOOP("owner", choice))
        : xlir_test_observe_generator_launch(OWNER_CFG_STORE("owner", choice));
    int count = 0, expected = kind == 0 || kind == 3 ? 2 : kind == 1 ? 2 + choice : 2 * choice;
    assert(generator);
    for (;;) {
        char* item = NULL; xlir_generator_step step = xlir_generator_next(generator); assert(step.ok);
        if (!step.value) break;
        assert(xlir_generator_item_get(generator, &item, sizeof(item), true));
        assert(item && !strcmp(item, "owner")); xrtFree(item);
        if (++count == stop) break;
    }
    assert(count == (stop ? stop : expected)); xlir_generator_destroy(generator);
}
