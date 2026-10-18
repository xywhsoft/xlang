/* Link only a normal product and its extracted declaration-only SDK. */
#include <__xl_runtime/xruntime_decl.h>
#include <__xl_runtime/xl6_continuation_executor.h>
#include <__xl_runtime/xl6_continuation_future.h>
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>

enum {COUNT=1024};
typedef struct State {atomic_uint runs,releases;uint64_t producer;} State;
static void run(void* data)
{State* state=data;assert(xrtThreadCurrentId()!=state->producer);atomic_fetch_add(&state->runs,1);}
static void release(void* data)
{State* state=data;atomic_fetch_add_explicit(&state->releases,1,memory_order_release);}
static bool edge(xrtownershipref ref,void* data)
{assert(ref.Data&&ref.Ops);++*(size_t*)data;return true;}
static void parked(XL6_ContinuationExecutor* executor,size_t workers)
{
    xdeadline deadline=xrtDeadlineAfter(10000000);xrtownershipref ref=xl6ContinuationExecutorOwnership(executor);
    for(;;){
        xrtownershipscope freeze={0};
        if(xrtOwnershipFreezeTryBegin(&freeze)){
            size_t count;bool ready=ref.Ops->Count(ref.Data,&count);
            if(ready){size_t edges=0;assert(count==1&&ref.Ops->Trace(ref.Data,edge,&edges)&&edges==workers+1);}
            assert(xrtOwnershipScopeEnd(&freeze));if(ready)return;
        }
        assert(!xrtDeadlineExpired(deadline));xrtThreadYield();
    }
}
int main(void)
{
    for(size_t workers=0;workers<3;++workers){
        State state={0};state.producer=xrtThreadCurrentId();
        XL6_ContinuationTicket* tickets=calloc(COUNT,sizeof(*tickets));assert(tickets);
        xfuturewatch* watches=calloc(COUNT,sizeof(*watches));assert(watches);
        xfuture* source=NULL;xpromise* promise=xrtPromiseCreate(&source,NULL);assert(promise&&source);
        XL6_ContinuationExecutor* executor=xl6ContinuationExecutorCreateFixedV1(COUNT,workers);assert(executor);
        parked(executor,workers?workers:4);
        for(size_t i=0;i<COUNT;++i){
            assert(xl6ContinuationExecutorReserveOwned(executor,&tickets[i],run,release,&state,NULL)==XL6_CONTINUATION_ADMITTED);
            assert(xl6ContinuationWatchRegister(&tickets[i],&watches[i],source));
            xl6ContinuationCommit(&tickets[i]);
        }
        parked(executor,workers?workers:4);
        XL6_ContinuationTicket extra={0};
        assert(xl6ContinuationExecutorReserveOwned(executor,&extra,run,release,&state,NULL)==XL6_CONTINUATION_FULL);
        xl6ContinuationExecutorClose(executor);assert(!xl6ContinuationExecutorDestroyCompleted(executor));
        assert(xl6ContinuationExecutorReserveOwned(executor,&extra,run,release,&state,NULL)==XL6_CONTINUATION_CLOSED);
        int payload=1;assert(xrtPromiseResolve(promise,&payload));
        xdeadline deadline=xrtDeadlineAfter(10000000);
        while(atomic_load_explicit(&state.releases,memory_order_acquire)!=COUNT){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
        assert(atomic_load(&state.runs)==COUNT);
        while(!xl6ContinuationExecutorDestroyCompleted(executor)){assert(!xrtDeadlineExpired(deadline));xrtThreadYield();}
        xrtFutureDestroy(source);xrtPromiseDestroy(promise);free(watches);free(tickets);
    }
    puts("normal product fixed executor: default/1/2 workers, 3072 dormant roots, exact parked owners, ready delivery and checked join");
    return 0;
}
