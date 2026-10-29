/* Real native graphs and resident catalog; no managed-language completion claim. */
#define XRT_MODULE_TASK
#define XRT_MODULE_VALUE_GRAPH
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_ONCE
#define main executor_prerequisite_main
#include "release/include/xl_clock.h"
#include "../cases/xrt_executor_ownership_native.c"
#undef main
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"

static const xexecutordataownershipv1 copied_policy={sizeof(copied_policy),item_run,item_drop,&item_ops};
static xtaskoutcome unused_task_run(xcancel* cancel,ptr data,xtaskvalue* value)
{(void)cancel;(void)data;(void)value;assert(0);return XTASK_FAILED;}
static const xtaskdataownershipv1 task_policy={sizeof(task_policy),unused_task_run,item_drop,&item_ops};
static bool load_policy(XL6_OwnershipCatalog* catalog)
{
    return xl6OwnershipCatalogAddExecutorV1(catalog,&item_policy) &&
        xl6OwnershipCatalogAddExecutorV1(catalog,&item_policy);
}
static bool load_task_policy(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddTaskV1(catalog,&task_policy);}
static const XL6_OwnershipDomainV1 policy_domain={sizeof(policy_domain),load_policy,NULL,NULL};
static const XL6_OwnershipDomainV1 task_domain={sizeof(task_domain),load_task_policy,NULL,NULL};
static XL6_OwnershipCatalog* make_catalog(bool include_policy,bool task_only)
{
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainNativeExecutorV1Get(),
        task_only ? &task_domain : &policy_domain};
    return xl6OwnershipCatalogCreateV1(domains,include_policy ? 2 : 1);
}
static const xrtownershipadapterv1* probe(XL6_OwnershipCatalog* catalog,xrtownershipref ref,
    XL6_OwnershipAdmissionV1 expected,const xrtownershippreparationv1** preparation)
{
    const xrtownershipadapterv1* adapter=(const xrtownershipadapterv1*)catalog;
    const XL6_OwnershipFinishV1* finish=(const XL6_OwnershipFinishV1*)catalog;
    *preparation=(const xrtownershippreparationv1*)catalog;
    assert(xl6OwnershipCatalogProbeV1(ref,catalog,&adapter,preparation,&finish)==expected);
    assert(!finish);
    if(expected==XL6_OWNERSHIP_ADMISSION_ADMITTED)assert(adapter&&*preparation);
    else assert(!adapter&&!*preparation);
    return adapter;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xrtClearError();xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes&&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount&&
        after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void queued_round(XL6_OwnershipCatalog* known,XL6_OwnershipCatalog* native_only,
    XL6_OwnershipCatalog* task_only,XL6_OwnershipCatalog* empty,bool foreign_policy)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xatomic32 completed;xrtAtomic32Init(&completed,0);
    xexecutorconfig config={1,2,0};xexecutor* pool=xrtExecutorCreate(&config);assert(pool);
    xrtownershipref ref=xrtExecutorOwnership(pool);xrtownershipscope freeze={0};freeze_begin(&freeze);
    const xrtownershippreparationv1* preparation=NULL;
    assert(!probe(empty,ref,XL6_OWNERSHIP_ADMISSION_REFUSED,&preparation));
    const xrtownershipadapterv1* adapter=probe(known,ref,XL6_OWNERSHIP_ADMISSION_ADMITTED,&preparation);
    assert(adapter->Hold(ref.Data)&&adapter->Claim(ref.Data,&config));
    Item* item=xrtCalloc(1,sizeof(*item));assert(item);*item=(Item){1,&completed};
    assert(xrtExecutorSubmitOwnedV1(pool,item,foreign_policy ? &copied_policy : &item_policy));
    Slots slots={0};assert(ref.Ops->Trace(ref.Data,remember,&slots)&&slots.count==1);
    xrtownershipref job=slots.items[0];const xrtownershippreparationv1* job_preparation=NULL;
    assert(!probe(native_only,job,XL6_OWNERSHIP_ADMISSION_REFUSED,&job_preparation));
    assert(!probe(task_only,job,XL6_OWNERSHIP_ADMISSION_REFUSED,&job_preparation));
    assert(!probe(empty,job,XL6_OWNERSHIP_ADMISSION_REFUSED,&job_preparation));
    const xrtownershipadapterv1* job_adapter=probe(known,job,
        foreign_policy ? XL6_OWNERSHIP_ADMISSION_REFUSED : XL6_OWNERSHIP_ADMISSION_ADMITTED,&job_preparation);
    if(job_adapter)assert(job_adapter->Hold(job.Data)&&job_adapter->Claim(job.Data,&config));
    assert(!xl6OwnershipCatalogAdmitsReadV1(ref,known));
    assert(!xl6OwnershipCatalogAdmitsReadV1(job,known));
    xrtownershipref opaque={(void*)(uintptr_t)1,&item_ops};
    assert(!probe(known,opaque,XL6_OWNERSHIP_ADMISSION_REFUSED,&job_preparation));
    /* Refused probes clear their outputs, so retain the admitted preparation separately. */
    if(job_adapter)assert(probe(known,job,XL6_OWNERSHIP_ADMISSION_ADMITTED,&job_preparation)==job_adapter);
    assert(xrtOwnershipScopeEnd(&freeze));
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    for(;;){xrtownershipprepareresult result=preparation->Prepare(ref.Data,&config);
        if(result==XRT_OWNERSHIP_PREPARE_READY)break;
        assert(result==XRT_OWNERSHIP_PREPARE_BUSY&&!xl6DeadlineExpiredV1(end));xrtThreadYield();}
    freeze_begin(&freeze);assert(preparation->Ready(ref.Data)&&xrtAtomic32Load(&completed,XMEMORY_ACQUIRE)==2);
    if(job_adapter){assert(job_preparation->Ready(job.Data));job_adapter->Clear(job.Data,&config);}
    adapter->Clear(ref.Data,&config);assert(xrtOwnershipScopeEnd(&freeze));
    assert(adapter->Finish(ref.Data,&config)&&xrtExecutorDestroy(pool));
    if(job_adapter){assert(job_adapter->Finish(job.Data,&config));job_adapter->Drop(job.Data);}
    adapter->Drop(ref.Data);balanced(&before);
}

typedef struct Gated {
    size_t references;
    xatomic32 *stage,*allow,*inspections;
} Gated;
static bool gated_count(const void* data,size_t* count)
{
    const Gated* gate=data;if(!gate||!count||!gate->references)return false;
    (void)xrtAtomic32FetchAdd(gate->inspections,1,XMEMORY_RELAXED);*count=gate->references;return true;
}
static bool gated_trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const Gated* gate=data;(void)context;if(!gate||!visit)return false;
    (void)xrtAtomic32FetchAdd(gate->inspections,1,XMEMORY_RELAXED);return true;
}
static const xrtownershipops gated_ops={gated_count,gated_trace};
static void wait_value(xatomic32* value,uint32 expected)
{
    XL6_DeadlineV1 end=xl6DeadlineAfterMillisecondsV1(5000);
    while((uint32)xrtAtomic32Load(value,XMEMORY_ACQUIRE)<expected){assert(!xl6DeadlineExpiredV1(end));xrtThreadYield();}
}
static void gated_run(ptr data)
{Gated* gate=data;assert(gate->references==1);xrtAtomic32Store(gate->stage,1,XMEMORY_RELEASE);wait_value(gate->allow,1);}
static void gated_drop(ptr data,ptr context)
{
    Gated* gate=data;assert(!context&&gate->references==1);
    xrtAtomic32Store(gate->stage,2,XMEMORY_RELEASE);wait_value(gate->allow,2);
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    gate->references=0;xrtFree(gate);assert(xrtOwnershipScopeEnd(&mutation));
}
static const xexecutordataownershipv1 gated_policy={sizeof(gated_policy),gated_run,gated_drop,&gated_ops};
static bool load_gated(XL6_OwnershipCatalog* catalog)
{return xl6OwnershipCatalogAddExecutorV1(catalog,&gated_policy);}
static const XL6_OwnershipDomainV1 gated_domain={sizeof(gated_domain),load_gated,NULL,NULL};
static void busy_round(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainNativeExecutorV1Get(),&gated_domain};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2);assert(catalog);
    xatomic32 stage_value,allow,inspections;
    xrtAtomic32Init(&stage_value,0);xrtAtomic32Init(&allow,0);xrtAtomic32Init(&inspections,0);
    Gated* gate=xrtCalloc(1,sizeof(*gate));assert(gate);*gate=(Gated){1,&stage_value,&allow,&inspections};
    xexecutorconfig config={1,1,0};xexecutor* pool=xrtExecutorCreate(&config);assert(pool);
    xrtownershipref ref=xrtExecutorOwnership(pool);xrtownershipscope freeze={0};freeze_begin(&freeze);
    const xrtownershippreparationv1* preparation=NULL;
    const xrtownershipadapterv1* adapter=probe(catalog,ref,XL6_OWNERSHIP_ADMISSION_ADMITTED,&preparation);
    assert(adapter->Hold(ref.Data));assert(xrtExecutorSubmitOwnedV1(pool,gate,&gated_policy));
    Slots slots={0};assert(ref.Ops->Trace(ref.Data,remember,&slots)&&slots.count==1);
    const xrtownershippreparationv1* job_preparation=NULL;
    const xrtownershipadapterv1* job_adapter=probe(catalog,slots.items[0],XL6_OWNERSHIP_ADMISSION_ADMITTED,&job_preparation);
    assert(job_adapter->Hold(slots.items[0].Data));assert(xrtOwnershipScopeEnd(&freeze));
    for(uint32 stage=1;stage<=2;++stage){
        wait_value(&stage_value,stage);freeze_begin(&freeze);
        assert(!probe(catalog,ref,XL6_OWNERSHIP_ADMISSION_BUSY,&preparation));
        assert(!probe(catalog,slots.items[0],XL6_OWNERSHIP_ADMISSION_BUSY,&job_preparation));
        assert(!xrtAtomic32Load(&inspections,XMEMORY_RELAXED));
        assert(xrtOwnershipScopeEnd(&freeze));xrtAtomic32Store(&allow,(int32)stage,XMEMORY_RELEASE);
    }
    assert(xrtExecutorWait(pool));job_adapter->Drop(slots.items[0].Data);
    assert(xrtExecutorDestroy(pool));adapter->Drop(ref.Data);xl6OwnershipCatalogDestroyV1(catalog);balanced(&before);
}
int main(void)
{
    assert(executor_prerequisite_main()==0);
    xl6_error_scope warm={0};assert(xl6ErrorScopeBegin(&warm));assert(xl6ErrorScopeEnd(&warm).ok);
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_OwnershipCatalog* known=make_catalog(true,false);assert(known);
    XL6_OwnershipCatalog* native_only=make_catalog(false,false);assert(native_only);
    XL6_OwnershipCatalog* task_only=make_catalog(true,true);assert(task_only);
    XL6_OwnershipCatalog* empty=xl6OwnershipCatalogCreateV1(NULL,0);assert(empty);
    assert(!xl6OwnershipCatalogAddExecutorV1(known,&copied_policy));xrtClearError();
    for(unsigned i=0;i<32;++i){queued_round(known,native_only,task_only,empty,false);queued_round(known,native_only,task_only,empty,true);}
    for(unsigned i=0;i<16;++i)busy_round();
    xl6OwnershipCatalogDestroyV1(empty);xl6OwnershipCatalogDestroyV1(task_only);
    xl6OwnershipCatalogDestroyV1(native_only);xl6OwnershipCatalogDestroyV1(known);balanced(&before);
    unsigned failed=0,succeeded=0;
    for(unsigned budget=0;budget<16;++budget){
        assert(xrtMemDebugFailAfter(budget));XL6_OwnershipCatalog* catalog=make_catalog(true,false);
        bool injected=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(catalog){assert(!injected);++succeeded;xl6OwnershipCatalogDestroyV1(catalog);}else{assert(injected);++failed;}
        balanced(&before);
    }
    assert(failed&&succeeded);
    puts("Executor catalog: 64 queued graphs, 32 busy phases, typed identity refusal, sealed admission and 16 allocation prefixes");
    return 0;
}
