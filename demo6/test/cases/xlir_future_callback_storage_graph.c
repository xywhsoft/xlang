#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_FUTURE
#define XRT_MODULE_THREAD
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <stdatomic.h>
extern int callback_probe_init(void);
extern void callback_probe_fini(void);
extern xfuture* callback_probe_start(xfuture*);
extern xrtownershipref callback_probe_executor(void);
extern bool callback_probe_idle(void);
static atomic_uint dropped;
static int64_t payload=10;
void callback_boundary_drop(void) { ++dropped; }
xfuture* callback_boundary_input(void)
{ xfuture* f=NULL;xpromise* p=xrtPromiseCreate(&f,NULL);assert(p && xrtPromiseResolve(p,&payload));xrtPromiseDestroy(p);return f; }
int main(void)
{
    xmemdebugsnapshot before,after;assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();xrtMemDebugSnapshot(&before);
    assert(callback_probe_init());
    for(unsigned n=0;n<100;++n){
        xfuture *source=NULL,*output;xrtownershipscope freeze={0};xrtownershipresult graph;
        xpromise* p=xrtPromiseCreate(&source,NULL);assert(p);output=callback_probe_start(source);assert(output && !xrtFutureDone(output));
        xrtownershipref root=xrtFutureOwnership(source),executor=callback_probe_executor();xdeadline limit=xrtDeadlineAfter(5000000);
        for(;;){
            if(xrtOwnershipFreezeTryBegin(&freeze)){
                size_t references;if(executor.Ops->Count(executor.Data,&references))break;
                assert(xrtOwnershipScopeEnd(&freeze));
            }
            assert(!xrtDeadlineExpired(limit));xrtThreadYield();
        }
        assert(xrtOwnershipInspect(&root,1,NULL,0,&graph));
        if(n==0)printf("Generated Future graph: nodes=%llu edges=%llu roots=%llu\n",(unsigned long long)graph.NodeCount,(unsigned long long)graph.EdgeCount,(unsigned long long)graph.ExternalRootCount);
        if(n==0)fflush(stdout);
        /* Source/output observers plus the independent accepted queue
         * execution reference, which remains real while the source waits. */
        assert(graph.NodeCount==11 && graph.EdgeCount==14 && graph.ExternalRootCount==3);
        assert(xrtOwnershipScopeEnd(&freeze));
        if(n%4==0)assert(xrtPromiseResolve(p,&payload));
        else if(n%4==1){xrtSetErrorKind(XERR_STATE);assert(xrtPromiseReject(p,xrtGetError()));xrtClearError();}
        else if(n%4==2)assert(xrtPromiseCancel(p));else assert(xrtPromiseClose(p));
        assert(xrtFutureWaitFor(output,5000000)==XWAIT_OK);limit=xrtDeadlineAfter(5000000);
        while(!callback_probe_idle()){assert(!xrtDeadlineExpired(limit));xrtThreadYield();}
        assert(xrtFutureState(output)==(n%4==0?XFUTURE_RESOLVED:n%4==1?XFUTURE_FAILED:n%4==2?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        assert(dropped==n+1 && !xrtGetError());xrtPromiseDestroy(p);xrtFutureDestroy(source);xrtFutureDestroy(output);
    }
    callback_probe_fini();assert(!xrtGetError());xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    puts("Generated Future callbacks: 100 pending complete graphs and four terminal outcomes, exactly 100 capture drops, zero live delta; no native roots");return 0;
}
