/* Test callbacks only: TLS fault arming remains in the generated worker.
 * Keep a real native thread reference through XRT completion and its release.
 * XRT wait is logical completion, NOT an OS-thread join. Keep products loaded.
 * Never replace an allocator, container, thread entry or runtime provider. */
#if defined(XL6_WORKER_PROBE_EMBEDDED_SDK)
#include <__xl_runtime/xruntime_decl.h>
#else
#include "xruntime_decl.h"
#endif
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
static uint64_t workerBudget,callerId;
static int64_t workerCase;
static bool configured;
static _Atomic(xthread*) workerNative;
static _Atomic(unsigned) workerStarted,workerFinished;
static _Atomic(bool) workerHit;
static _Atomic(bool) workerCaught;

static uint64_t readNumber(const char* name,uint64_t fallback)
{
    const char* text=getenv(name);if(!text)return fallback;
    char* end=NULL;errno=0;unsigned long long value=strtoull(text,&end,10);
    assert(*text && *text!='-' && end && !*end && !errno);return (uint64_t)value;
}
void workerProbeConfigure(int64_t kind,uint64_t budget)
{
    assert(kind>=0 && kind<10 && !atomic_load(&workerNative));
    workerCase=kind;workerBudget=budget;callerId=xrtThreadCurrentId();configured=true;
    atomic_store(&workerStarted,0);atomic_store(&workerFinished,0);atomic_store(&workerHit,false);
    atomic_store(&workerCaught,false);
    assert(!xrtMemDebugFailTriggered());
}
int64_t workerProbeCase(void)
{
    if(!configured)workerProbeConfigure((int64_t)readNumber("XL6_WORKER_CASE",0),readNumber("XL6_WORKER_BUDGET",UINT64_MAX));
    assert(xrtThreadCurrentId()==callerId);return workerCase;
}
uint64_t workerProbeBudget(void)
{assert(configured && xrtThreadCurrentId()==callerId);return workerBudget;}
bool workerProbeStart(void)
{
    assert(configured && xrtThreadCurrentId()!=callerId && xrtThreadCurrent());
    assert(atomic_fetch_add_explicit(&workerStarted,1,memory_order_acq_rel)==0);
    assert(!xrtMemDebugFailTriggered());
    xthread* owned=xrtThreadRef(xrtThreadCurrent());assert(owned);
    assert(!atomic_exchange_explicit(&workerNative,owned,memory_order_acq_rel));return true;
}
bool workerProbeFinish(bool caught)
{
    assert(xrtThreadCurrentId()!=callerId && atomic_load(&workerStarted)==1);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(caught!=hit)fprintf(stderr,"worker fault mismatch case=%lld budget=%llu caught=%d hit=%d\n",
        (long long)workerCase,(unsigned long long)workerBudget,caught,hit);
    assert(caught==hit);
    atomic_store_explicit(&workerHit,hit,memory_order_release);
    atomic_store_explicit(&workerCaught,caught,memory_order_release);
    assert(atomic_fetch_add_explicit(&workerFinished,1,memory_order_acq_rel)==0);return true;
}
bool workerProbeCompleted(void)
{
    assert(xrtThreadCurrentId()==callerId && atomic_load_explicit(&workerFinished,memory_order_acquire)==1);
    xthread* owned=atomic_exchange_explicit(&workerNative,NULL,memory_order_acq_rel);assert(owned);
    assert(xrtThreadWaitFor(owned,INT64_C(30000))==XWAIT_OK && xrtThreadExitCode(owned)==0);
    xrtThreadDestroy(owned);assert(!xrtMemDebugFailTriggered());
    printf("worker-prefix case=%lld budget=%llu hit=%d completed=1 caller-isolated=1\n",
        (long long)workerCase,(unsigned long long)workerBudget,(int)atomic_load_explicit(&workerHit,memory_order_acquire));
    return true;
}
bool workerProbeTriggered(void)
{assert(atomic_load(&workerStarted)==1 && atomic_load(&workerFinished)==1 && !atomic_load(&workerNative));return atomic_load(&workerHit);}
void workerProbeDescribe(void)
{
    xthread* observed=atomic_load_explicit(&workerNative,memory_order_acquire);
    fprintf(stderr,"worker-observed started=%u finally=%u hit=%d caught=%d native-exit=%d\n",
        atomic_load(&workerStarted),atomic_load(&workerFinished),(int)atomic_load(&workerHit),
        (int)atomic_load(&workerCaught),observed ? xrtThreadExitCode(observed) : -999);
}
