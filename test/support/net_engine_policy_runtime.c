/* Real owned native task/timer/exit policies. Engine only: no sockets or traffic. */
#define main existingNetEngineRuntimeMain
/* Observe the actual ABI argument lists, then delegate to the unchanged native
 * adapter. This is a contract spy, not a replacement graph or policy result. */
#define xrtNetEngineOwnershipAdapterV3 testRealNetEngineOwnershipAdapterV3
#include "net_engine_runtime.c"
#undef xrtNetEngineOwnershipAdapterV3
#undef main

typedef struct PolicyData {
    volatile int32 references;
    xvalue *child,*retired;
    const void* claim;
    bool cleared;
    xatomic32 runs;
} PolicyData;
static unsigned policyCountCalls,policyTraceCalls,policyGraphs,policyPrefixes;
static bool policyCount(const void* raw,size_t* count)
{
    const PolicyData* p=raw;++policyCountCalls;
    if(!p||!count||p->references<=0||p->cleared)return false;
    *count=(size_t)p->references;return true;
}
static bool policyTrace(const void* raw,xrtownershipvisitor visit,void* context)
{
    const PolicyData* p=raw;size_t count;++policyTraceCalls;
    return visit&&policyCount(raw,&count)&&(!p->child||visit(xrtValueOwnership(p->child),context));
}
static const xrtownershipops policyOps={policyCount,policyTrace};
static void policyRelease(const void* raw)
{
    PolicyData* p=(PolicyData*)raw;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    int32 refs=xrtRefRelease(&p->references);assert(refs>=0);assert(xrtOwnershipScopeEnd(&mutation));
    if(!refs){assert(!p->claim||p->cleared);xrtValueRelease(p->child);xrtValueRelease(p->retired);xrtFree(p);}
}
static bool policyHold(const void* raw)
{
    PolicyData* p=(PolicyData*)raw;xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    bool ok=!p->cleared&&p->references>0&&p->references<INT32_MAX;
    if(ok)assert(xrtRefRetain(&p->references)>0);
    assert(xrtOwnershipScopeEnd(&mutation));return ok;
}
static PolicyData* policyCreate(void)
{
    PolicyData* p=xrtCalloc(1,sizeof(*p));assert(p);p->references=1;p->child=xrtValueInt(17);assert(p->child);return p;
}
static bool policyClaim(const void* raw,const void* token)
{PolicyData* p=(PolicyData*)raw;if(!token||p->cleared||(p->claim&&p->claim!=token))return false;p->claim=token;return true;}
static void policyRestore(const void* raw,const void* token)
{PolicyData* p=(PolicyData*)raw;assert(token&&p->claim==token&&!p->cleared);p->claim=NULL;}
static void policyClear(const void* raw,const void* token)
{PolicyData* p=(PolicyData*)raw;assert(token&&p->claim==token&&!p->cleared);p->cleared=true;p->retired=p->child;p->child=NULL;}
static bool policyFinish(const void* raw,const void* token)
{PolicyData* p=(PolicyData*)raw;assert(token&&p->claim==token&&p->cleared);xrtValueRelease(p->retired);p->retired=NULL;return true;}
static const xrtownershipadapterv1 policyAdapter={sizeof(policyAdapter),policyHold,policyRelease,policyClaim,policyRestore,NULL,policyClear,policyFinish};
static void policyDrop(void* raw,void* unused){assert(!unused);policyRelease(raw);}
static void policyWorkDrop(void* raw){policyRelease(raw);}
static void policyPost(xnetworker* worker,void* raw)
{assert(worker&&xrtNetWorkerIsCurrent(worker));PolicyData* p=raw;xrtAtomic32FetchAdd(&p->runs,1,XMEMORY_RELEASE);}
static void policyTimer(xnetworker* worker,uint64 id,xnetresult result,void* raw)
{assert(worker&&id&&(result==XNET_RESULT_CANCELLED||result==XNET_RESULT_CLOSED));policyPost(worker,raw);}
static void policyExit(void* raw){PolicyData* p=raw;xrtAtomic32FetchAdd(&p->runs,1,XMEMORY_RELEASE);}
static const xnettaskownershipv1 addedTask={sizeof(addedTask),policyPost,policyWorkDrop,&policyOps};
static const xnettimerownershipv1 addedTimer={sizeof(addedTimer),policyTimer,policyWorkDrop,&policyOps};
static const xnetengineworkerexitownershipv1 addedExit={sizeof(addedExit),policyExit,policyDrop,&policyOps};
enum {COPIES=8};
static xnettaskownershipv1 taskCopies[COPIES];
static xnettimerownershipv1 timerCopies[COPIES];
static xnetengineworkerexitownershipv1 exitCopies[COPIES];
static size_t expectedPolicyCount;
const xrtownershipadapterv1* xrtNetEngineOwnershipAdapterV3(xrtownershipref ref,
    const xnettaskownershipv1* const* tasks,size_t taskCount,const xnettimerownershipv1* const* timers,size_t timerCount,
    const xnetengineworkerexitownershipv1* const* exits,size_t exitCount,
    const xnetruntimeownershipv1* const* runtimes,size_t runtimeCount,const xrtownershippreparationv1** prep)
{
    if(expectedPolicyCount){
        assert(taskCount==expectedPolicyCount&&timerCount==expectedPolicyCount&&exitCount==expectedPolicyCount);
        bool builtinTask=false,builtinTimer=false,builtinExit=false,extraTask=false,extraTimer=false,extraExit=false;
        for(size_t i=0;i<taskCount;++i){builtinTask|=tasks[i]==xl6NetEngineWorkPolicyV1Get();extraTask|=tasks[i]==&addedTask;}
        for(size_t i=0;i<timerCount;++i){builtinTimer|=timers[i]==xl6NetEngineTimerPolicyV1Get();extraTimer|=timers[i]==&addedTimer;}
        for(size_t i=0;i<exitCount;++i){builtinExit|=exits[i]==xl6NetEngineExitPolicyV1Get();extraExit|=exits[i]==&addedExit;}
        assert(builtinTask&&builtinTimer&&builtinExit);
        assert(extraTask==(expectedPolicyCount==COPIES+2)&&extraTimer==extraTask&&extraExit==extraTask);
    }
    return testRealNetEngineOwnershipAdapterV3(ref,tasks,taskCount,timers,timerCount,exits,exitCount,runtimes,runtimeCount,prep);
}
static bool copiedLoad(XL6_OwnershipCatalog* c)
{
    for(size_t i=0;i<COPIES;++i){
        if(!xl6OwnershipCatalogAddNetTaskV1(c,&taskCopies[i])||!xl6OwnershipCatalogAddNetTimerV1(c,&timerCopies[i])||
           !xl6OwnershipCatalogAddNetWorkerExitV1(c,&exitCopies[i]))return false;
    }
    return true;
}
static bool fullLoad(XL6_OwnershipCatalog* c)
{
    if(!copiedLoad(c))return false;
    for(unsigned repeat=0;repeat<2;++repeat)
        if(!xl6OwnershipCatalogAddNetTaskV1(c,&addedTask)||!xl6OwnershipCatalogAddNetTimerV1(c,&addedTimer)||
           !xl6OwnershipCatalogAddNetWorkerExitV1(c,&addedExit))return false;
    return true;
}
static bool ignoredLoadFailure(XL6_OwnershipCatalog* c)
{assert(!xl6OwnershipCatalogAddNetTimerV1(c,NULL));xrtClearError();return true;}
static bool policiesInspect(xrtownershipref ref,const XL6_OwnershipPoliciesV1* p,const xrtownershipadapterv1** a,
    const xrtownershippreparationv1** prep,const XL6_OwnershipFinishV1** finish)
{
    (void)ref;(void)a;(void)prep;(void)finish;
    assert(p->NetTaskCount==COPIES+2&&p->NetTimerCount==COPIES+2&&p->NetWorkerExitCount==COPIES+2);
    bool task=false,timer=false,exit=false;
    for(size_t i=0;i<p->NetTaskCount;++i)task=task||p->NetTask[i]==xl6NetEngineWorkPolicyV1Get();
    for(size_t i=0;i<p->NetTimerCount;++i)timer=timer||p->NetTimer[i]==xl6NetEngineTimerPolicyV1Get();
    for(size_t i=0;i<p->NetWorkerExitCount;++i)exit=exit||p->NetWorkerExit[i]==xl6NetEngineExitPolicyV1Get();
    assert(task&&timer&&exit);return false;
}
static bool policyResolve(xrtownershipref ref,const XL6_OwnershipPoliciesV1* p,const xrtownershipadapterv1** a,
    const xrtownershippreparationv1** prep,const XL6_OwnershipFinishV1** finish)
{
    (void)p;(void)prep;(void)finish;if(ref.Ops!=&policyOps)return false;
    size_t count;if(!policyCount(ref.Data,&count))return false;*a=&policyAdapter;return true;
}
static const XL6_OwnershipDomainV1 dataDomain={sizeof(dataDomain),NULL,policyResolve,NULL};
static const XL6_OwnershipDomainV1 fullDomain={sizeof(fullDomain),fullLoad,policiesInspect,NULL};
static const XL6_OwnershipDomainV1 copiedDomain={sizeof(copiedDomain),copiedLoad,NULL,NULL};
static XL6_OwnershipCatalog* policyCatalog(const XL6_OwnershipDomainV1* extra,bool reverse)
{
    const XL6_OwnershipDomainV1* domains[]={
        #include "net-engine-domains.h"
        &dataDomain,extra
    };
    size_t n=sizeof(domains)/sizeof(*domains);
    if(reverse)for(size_t i=0;i<n/2;++i){const XL6_OwnershipDomainV1* d=domains[i];domains[i]=domains[n-1-i];domains[n-1-i]=d;}
    return xl6OwnershipCatalogCreateV1(domains,n);
}
static XL6_OwnershipAdmissionV1 policyProbe(xnetengine* engine,XL6_OwnershipCatalog* c,size_t expected)
{
    expectedPolicyCount=expected;
    xdeadline until=xrtDeadlineAfter(UINT64_C(5000000));
    for(;;){xrtownershipscope frozen={0};freeze(&frozen);const xrtownershipadapterv1* a=NULL;
        const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
        XL6_OwnershipAdmissionV1 result=xl6OwnershipCatalogProbeV1(xrtNetEngineOwnership(engine),c,&a,&prep,&finish);
        assert(xrtOwnershipScopeEnd(&frozen));if(result!=XL6_OWNERSHIP_ADMISSION_BUSY)return result;
        assert(!a&&!prep&&!finish&&!xrtDeadlineExpired(until));xrtThreadYield();}
}
static bool policyAdmit(xrtownershipref ref,void* data)
{
    const xrtownershipadapterv1* a=NULL;const xrtownershippreparationv1* prep=NULL;const XL6_OwnershipFinishV1* finish=NULL;
    return xl6OwnershipCatalogResolveV1(ref,data,&a,&prep,&finish);
}
static void registration(bool reverse)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    XL6_OwnershipCatalog *complete=policyCatalog(&fullDomain,reverse),*copyOnly=policyCatalog(&copiedDomain,!reverse);assert(complete&&copyOnly);
    PolicyData *exit=policyCreate(),*timer=policyCreate(),*post=policyCreate();
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=XNET_PORT_SELECT;
    assert(policyHold(exit));xnetengine* engine=xrtNetEngineCreateOwnedExitV1(&config,exit,&addedExit);assert(engine&&xrtNetEngineStart(engine));
    policyCountCalls=policyTraceCalls=0;
    assert(policyProbe(engine,copyOnly,COPIES+1)==XL6_OWNERSHIP_ADMISSION_REFUSED&&!policyCountCalls&&!policyTraceCalls);
    assert(policyProbe(engine,complete,COPIES+2)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(policyHold(timer));uint64 id=xrtNetEngineScheduleOwnedV1(engine,0,xrtDeadlineAfter(UINT64_C(300000000)),timer,&addedTimer);assert(id);
    policyCountCalls=policyTraceCalls=0;
    assert(policyProbe(engine,copyOnly,COPIES+1)==XL6_OWNERSHIP_ADMISSION_REFUSED&&!policyCountCalls&&!policyTraceCalls);
    assert(policyProbe(engine,complete,COPIES+2)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(policyHold(post)&&xrtNetEnginePostOwnedV1(engine,0,post,&addedTask));waitFor(&post->runs,1);
    assert(policyProbe(engine,complete,COPIES+2)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    xrtownershipref roots[4]={{0}};roots[0]=xrtNetEngineOwnership(engine);
    roots[1]=(xrtownershipref){exit,&policyOps};roots[2]=(xrtownershipref){timer,&policyOps};roots[3]=(xrtownershipref){post,&policyOps};
    xrtownershipscope frozen={0};freeze(&frozen);xrtownershipsnapshot* snapshot=NULL;
    assert(xrtOwnershipSnapshotCreate(roots,4,roots,4,policyAdmit,complete,&snapshot));
    assert(xrtOwnershipSnapshotNodeCount(snapshot)>=7);xrtOwnershipSnapshotDestroy(snapshot);assert(xrtOwnershipScopeEnd(&frozen));
    assert(!xl6OwnershipCatalogAddNetTaskV1(complete,&addedTask));xrtClearError();
    assert(!xl6OwnershipCatalogAddNetTimerV1(complete,&addedTimer));xrtClearError();
    assert(!xl6OwnershipCatalogAddNetWorkerExitV1(complete,&addedExit));xrtClearError();
    assert(policyProbe(engine,complete,COPIES+2)==XL6_OWNERSHIP_ADMISSION_ADMITTED);
    assert(xrtNetEngineTimerCancel(engine,id));waitFor(&timer->runs,1);
    assert(xrtNetEngineDestroy(engine));assert(xrtAtomic32Load(&exit->runs,XMEMORY_ACQUIRE)==1);
    assert(exit->references==1&&timer->references==1&&post->references==1);
    policyRelease(exit);policyRelease(timer);policyRelease(post);xl6OwnershipCatalogDestroyV1(copyOnly);xl6OwnershipCatalogDestroyV1(complete);
    balanced(&before);++policyGraphs;
}
static void registrationOom(void)
{
    for(unsigned budget=0;;++budget){
        assert(budget<128);xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
        xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors)&&xrtMemDebugFailAfter(budget));
        XL6_OwnershipCatalog* c=policyCatalog(&fullDomain,budget&1);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&errors);assert((c!=NULL)==outcome.ok&&(!c)==hit);
        xrtErrorFree(outcome.error);xl6OwnershipCatalogDestroyV1(c);balanced(&before);++policyPrefixes;if(!hit)break;
    }
    const XL6_OwnershipDomainV1 bad={sizeof(bad),ignoredLoadFailure,NULL,NULL};
    xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));assert(!policyCatalog(&bad,false));
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);assert(!outcome.ok);xrtErrorFree(outcome.error);xrtClearError();
}
int main(void)
{
    assert(existingNetEngineRuntimeMain()==0);
    for(size_t i=0;i<COPIES;++i){taskCopies[i]=addedTask;timerCopies[i]=addedTimer;exitCopies[i]=addedExit;}
    registrationOom();registration(false);registration(true);assert(xrtMemDebugReset());
    printf("Engine policy registry: graphs=%u allocation-prefixes=%u; typed task/timer/exit growth, dedup, seal, exact identity and actual native owners; balanced\n",policyGraphs,policyPrefixes);return 0;
}
