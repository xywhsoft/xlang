#include <assert.h>
#include <stddef.h>
extern void* callback_boundary_dispatch_malloc(size_t);
/* Instrument the actual generated allocations, including the output payload.
 * The fault belongs to the worker executing them, not the resolving thread. */
#define xrtMalloc callback_boundary_dispatch_malloc
#include "xlir_callback_boundary.c"
#undef xrtMalloc
#include "xlir_callback_boundary.h.bind.h"
int callback_boundary_init(void) { return xlir_module_init(); }
void callback_boundary_wait_idle(void)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!xl6ContinuationExecutorIsIdle(xlir_continuation_executor)){
        assert(!xrtGetError()&&!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
void callback_boundary_fini(void)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    for(;;){
        XL6_ModulePrepareResult result=xlir_module_prepare_resources(true,false);
        if(result==XL6_MODULE_PREPARE_READY){xlir_module_fini();return;}
        assert(result==XL6_MODULE_PREPARE_BUSY&&!xrtGetError()&&!xrtDeadlineExpired(deadline));
        xrtThreadYield();
    }
}
xfuture* callback_boundary_start(unsigned kind, xfuture* source)
{
    xlir_result_future result;
    switch (kind) {
    case 0: result = CALLBACK_THEN(source); break;
    case 1: result = CALLBACK_CATCH(source); break;
    case 2: result = CALLBACK_FINALLY(source); break;
    case 4: result = CALLBACK_OWNED(source); break;
    default: result = CALLBACK_TASK(source); break;
    }
    return result.ok ? result.value : NULL;
}
