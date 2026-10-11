/* Preinclude actual generated C. Sweep one batch through the first no-hit
 * logical allocation point, including strict graph reads of partial modules. */
#include "release/include/xrt_decl.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>

typedef struct TraceProbe { size_t count; bool reject; size_t absent; } TraceProbe;
static bool strictVisit(xrtownershipref ref,void* context)
{
    TraceProbe* probe=context;
    /* XRT graph visitors accept empty references as no edge. Keep that
     * existing contract; reject malformed nonempty references only. */
    if(!ref.Data) { ++probe->absent; return true; }
    assert(ref.Ops);
    ++probe->count;
    return !probe->reject;
}
static bool runPoint(size_t point)
{
    xl6ProductMemoryBegin(point);
    bool initialized=xlir_module_init()!=0;
    bool hit=xl6ProductMemoryEnd();
    xerror* failure=xrtTakeError();
    if(hit ? initialized || xrtErrorKind(failure)!=XERR_MEMORY : !initialized || failure)
        fprintf(stderr,"point=%zu initialized=%d hit=%d kind=%d message=%s\n",point,
            initialized,hit,(int)xrtErrorKind(failure),failure ? xrtErrorMessage(failure) : "none");
    assert(hit ? !initialized && xrtErrorKind(failure)==XERR_MEMORY : initialized && !failure);

    xrtownershipscope read={0};
    assert(xrtOwnershipFreezeTryBegin(&read));
    TraceProbe probe={0};
    assert(xl_script_module_ownership_trace(strictVisit,&probe));
    if(initialized) {
        assert(probe.count>0 && probe.absent>0);
        TraceProbe rejected={0,true};
        assert(!xl_script_module_ownership_trace(strictVisit,&rejected));
        assert(rejected.count==1);
    }
    assert(xrtOwnershipScopeEnd(&read));
    assert(!xrtGetError());
    xlir_module_fini();
    assert(!xrtGetError() && !xlir_module_initialized);
    xlir_module_fini(); assert(!xrtGetError());
    xrtErrorFree(failure); xrtClearError();
    xl6ProductMemoryVerify();
    return hit;
}
int main(void)
{
    size_t point=0;
    while(point<10000 && runPoint(point)) ++point;
    assert(point<10000);
    printf("Channel globals strict graph prefixes=%zu first-no-hit=%zu balanced\n",point+1,point);
    return 0;
}
