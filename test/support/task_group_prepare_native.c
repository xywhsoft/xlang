/* Native post-finalization retirement, not automatic language admission. */
#define XRT_MODULE_FUTURE_COMBINE
#define XRT_MODULE_TASK_GROUP
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ATOMIC
#ifdef XL6_TASK_GROUP_PREPARE_LINKED
#define XRT_MODULE_ALL
#define XRUNTIME_MODULE_ALL
#endif
#if !defined(XL6_TASK_GROUP_PREPARE_LINKED) || defined(XL6_TASK_GROUP_PREPARE_KERNEL)
#ifdef XL6_TASK_GROUP_PREPARE_XRUNTIME
#define XRUNTIME_IMPLEMENTATION
#else
#define XRT_IMPLEMENTATION
#endif
#endif
#ifdef XL6_TASK_GROUP_PREPARE_XRUNTIME
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#else
#include "lib/xrt/xrt.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void testRequire(bool condition,const char* message)
{ if(!condition){fprintf(stderr,"TaskGroup retirement: %s\n",message);exit(1);} }
#ifdef XL6_TASK_GROUP_PREPARE_LINKED
#include "src/runtime/xl6_future_wait.h"
#include "src/runtime/xl6_persistence_restore_plan.h"
static void check_open_keys(void)
{
    xstrview claimed[2];claimed[0]=XRT_STR_LITERAL("field");claimed[1]=XRT_STR_LITERAL("");
    XL6_RestoreOpenV1 recipe={0};recipe.storage=XRT_STR_LITERAL("store");recipe.claimed=claimed;recipe.claimedCount=2;
    testRequire(xl6RestoreOpenClaimsKeyV1(&recipe,XRT_STR_LITERAL("store")) &&
        xl6RestoreOpenClaimsKeyV1(&recipe,XRT_STR_LITERAL("$xlir.class.runtime-type")) &&
        xl6RestoreOpenClaimsKeyV1(&recipe,XRT_STR_LITERAL("field")) && xl6RestoreOpenClaimsKeyV1(&recipe,(xstrview){NULL,0}) &&
        !xl6RestoreOpenClaimsKeyV1(&recipe,XRT_STR_LITERAL("other")),"portable counted reserved/claimed/empty key matching");
}
static unsigned wait_probes;
static void probe(xrtownershipref ref,const void* token,bool expected)
{
    XL6_FutureWaitProof proof=xl6FutureWaitCycleMemberV2(ref,token);
    testRequire(proof==(expected?XL6_FUTURE_WAIT_CYCLE_MEMBER:XL6_FUTURE_WAIT_UNPROVEN),"production solver classifies real group/aggregate wait");
    ++wait_probes;
}
#define TASK_GROUP_WAIT_PROBE(Ref,Token,Expected) do { \
    bool wait_expected=(Expected); \
    xrtownershipref wait_reference=(Ref); \
    probe(wait_reference,Token,wait_expected); \
} while(0)
#endif
#ifndef TASK_GROUP_WAIT_PROBE
#define TASK_GROUP_WAIT_PROBE(Reference,Token,Expected) ((void)0)
#endif

typedef struct owner {
    volatile int32 refs;
    struct owner* self;
    xtaskgroup *group,*child,*feeder;
    xfuture *done,*output,*spare;
    xpromise* promise;
    const void* claim;
    bool cleared;
    unsigned* destroyed;
} owner;
static void end(xrtownershipscope* scope) { testRequire(xrtOwnershipScopeEnd(scope),"scope end"); }
static void mutation(xrtownershipscope* scope) { testRequire(xrtOwnershipMutationBegin(scope),"mutation"); }
static void freeze(xrtownershipscope* scope) { testRequire(xrtOwnershipFreezeTryBegin(scope),"freeze"); }
static bool count(const void* data,size_t* refs)
{ const owner* value=data;if(!value || !refs || value->cleared || value->refs<=0)return false;*refs=(size_t)value->refs;return true; }
static bool trace(const void*,xrtownershipvisitor,ptr);
static const xrtownershipops ops={count,trace};
static xrtownershipref view(const owner* value) { return (xrtownershipref){value,&ops}; }
static bool trace(const void* data,xrtownershipvisitor visit,ptr context)
{
    const owner* value=data;size_t refs;if(!visit || !count(data,&refs))return false;
    return (!value->self || visit(view(value->self),context)) &&
        (!value->group || visit(xrtTaskGroupOwnership(value->group),context)) &&
        (!value->child || visit(xrtTaskGroupOwnership(value->child),context)) &&
        (!value->feeder || visit(xrtTaskGroupOwnership(value->feeder),context)) &&
        (!value->done || visit(xrtFutureOwnership(value->done),context)) &&
        (!value->output || visit(xrtFutureOwnership(value->output),context)) &&
        (!value->spare || visit(xrtFutureOwnership(value->spare),context)) &&
        (!value->promise || visit(xrtPromiseOwnership(value->promise),context));
}
static bool hold(const void* data)
{ owner* value=(owner*)data;xrtownershipscope scope={0};mutation(&scope);bool held=!value->cleared && xrtRefRetain(&value->refs)>0;end(&scope);return held; }
static void drop(const void* data)
{
    owner* value=(owner*)data;if(!value)return;xrtownershipscope scope={0};mutation(&scope);
    if(!xrtRefRelease(&value->refs)){testRequire(!value->self && !value->group && !value->child && !value->feeder &&
        !value->done && !value->output && !value->spare && !value->promise,"all actual owner slots returned");++*value->destroyed;xrtFree(value);}end(&scope);
}
static bool claim(const void* data,const void* token)
{ owner* value=(owner*)data;if(!token || value->cleared || (value->claim && value->claim!=token))return false;value->claim=token;return true; }
static void restore(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(token && value->claim==token && !value->cleared,"owner restore");value->claim=NULL; }
static void clear(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(token && value->claim==token && !value->cleared,"owner clear");value->cleared=true; }
static void dispose(owner* value)
{
    xrtownershipscope scope={0};mutation(&scope);owner* self=value->self;xtaskgroup *group=value->group,*child=value->child,*feeder=value->feeder;
    xfuture *done=value->done,*output=value->output,*spare=value->spare;xpromise* promise=value->promise;
    value->self=NULL;value->group=value->child=value->feeder=NULL;value->done=value->output=value->spare=NULL;value->promise=NULL;end(&scope);
    xrtTaskGroupDestroy(group);xrtTaskGroupDestroy(child);xrtTaskGroupDestroy(feeder);
    xrtFutureDestroy(done);xrtFutureDestroy(output);xrtPromiseDestroy(promise);xrtFutureDestroy(spare);drop(self);
}
static bool finish(const void* data,const void* token)
{ owner* value=(owner*)data;testRequire(value->cleared && value->claim==token,"owner finish");dispose(value);return true; }
static const xrtownershipadapterv1 adapter={sizeof(adapter),hold,drop,claim,restore,NULL,clear,finish};
enum { LIMIT=96 };
typedef struct node {
    xrtownershipref ref;
    const xrtownershipadapterv1* adapter;
    const xrtownershippreparationv1* preparation;
    const xrtownershipfinishv1* completion;
    size_t refs,incoming;
    bool finished;
} node;
typedef struct plan { node nodes[LIMIT];size_t size,current;unsigned edges[LIMIT][LIMIT]; } plan;
static size_t find(plan* value,xrtownershipref ref)
{ for(size_t i=0;i<value->size;++i)if(value->nodes[i].ref.Data==ref.Data && value->nodes[i].ref.Ops==ref.Ops)return i;return LIMIT; }
static bool visit(xrtownershipref ref,ptr context)
{
    plan* value=context;testRequire(ref.Data && ref.Ops,"real endpoint");size_t i=find(value,ref);
    if(i==LIMIT){testRequire(value->size<LIMIT,"bounded graph");i=value->size++;value->nodes[i].ref=ref;}
    ++value->edges[value->current][i];++value->nodes[i].incoming;return true;
}
static const xrtownershipadapterv1* query(xrtownershipref ref,const xrtownershippreparationv1** preparation,const xrtownershipfinishv1** completion)
{
    if(ref.Ops==&ops)return &adapter;
    const xfutureproducerownershipv1* producers[]={xrtTaskGroupProducerPolicyV1Get(),xrtFutureCombineProducerPolicyV1Get()};
    const xfuturepayloadownershipv1* payloads[]={xrtFutureCombineAllPayloadPolicyV1Get(),xrtFutureCombinePickPayloadPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xrtTaskGroupWatchPolicyV1Get()};
    const xfuturewatchownershipv2* projected[]={xrtFutureCombineWatchPolicyV2Get()};
    const xcancelwatchownershipv1* cancels[]={xrtTaskGroupCancelPolicyV1Get(),xrtFutureCombineCancelPolicyV1Get()};
    const xfutureownershipadmissionv1 admission={sizeof(admission),payloads,2,producers,2,watches,1,projected,1,cancels,2};
    const xrtownershipadapterv1* result=xrtTaskGroupOwnershipAdapterV2(ref,preparation,completion);
    if(!result)result=xrtFutureCombineOwnershipAdapterV2(ref,NULL,0,completion);
    if(!result)result=xrtFutureOwnershipAdapterV5(ref,&admission,completion);
    if(!result)result=xrtCancelWatchOwnershipAdapterV2(ref,cancels,2,completion);
    if(!result)result=xrtCancelOwnershipAdapterV3(ref,cancels,2,completion);
    return result;
}
static void snapshot(plan* value,owner* root)
{
    memset(value,0,sizeof(*value));value->size=1;value->nodes[0].ref=view(root);
    for(size_t i=0;i<value->size;++i){node* item=&value->nodes[i];value->current=i;item->adapter=query(item->ref,&item->preparation,&item->completion);
        testRequire(item->adapter && (!item->preparation || item->preparation->Adapter==item->adapter) &&
            (!item->completion || item->completion->Adapter==item->adapter),"exact lifecycle admission");
        testRequire(item->ref.Ops->Count(item->ref.Data,&item->refs) && item->ref.Ops->Trace(item->ref.Data,visit,value),"all physical slots");}
    size_t edges=0;for(size_t i=0;i<value->size;++i){testRequire(value->nodes[i].refs==value->nodes[i].incoming,"no external root or fictitious discount");
        for(size_t j=0;j<value->size;++j)edges+=value->edges[i][j];}
    xrtownershipresult observed={0};xrtownershipref anchor=view(root);
    testRequire(xrtOwnershipInspect(&anchor,1,NULL,0,&observed) && observed.NodeCount==value->size && observed.EdgeCount==edges &&
        !observed.ExternalRootCount,"independent pre-pin inspector agrees");
}
typedef struct inputs { xfuture* source;size_t count,limit; } inputs;
static bool input(xfuture* source,ptr context)
{ inputs* value=context;testRequire(source==value->source,"ordered duplicate active inputs");++value->count;return value->count<=value->limit; }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);testRequire(!xrtGetError() && after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount && after.InvalidFreeCount==before->InvalidFreeCount &&
        after.DoubleFreeCount==before->DoubleFreeCount && after.UseAfterFreeCount==before->UseAfterFreeCount,"full allocation ledger balanced");
}
static void cycle(unsigned flags)
{
    bool nested=(flags&1)!=0,open=(flags&2)!=0,any=(flags&4)!=0,abort_plan=(flags&8)!=0,reverse=(flags&16)!=0;
    unsigned destroyed=0,token=0,other=1;xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    owner* root=xrtCalloc(1,sizeof(*root));testRequire(root!=NULL,"owner");root->refs=1;root->self=root;root->destroyed=&destroyed;
    root->group=xrtTaskGroupCreate(NULL);testRequire(root->group!=NULL,"group");
    if(nested){root->child=xrtTaskGroupChild(root->group,NULL);testRequire(root->child!=NULL,"nested group");}
    root->done=xrtTaskGroupFuture(root->group);testRequire(root->done!=NULL,"Done");
    if(open){root->promise=xrtPromiseCreate(&root->spare,NULL);testRequire(root->promise && root->spare,"open completion alternative");}
    xfuture* sources[]={root->done,root->done,root->spare};
    root->output=any?xrtFutureAny(sources,open?3:2):xrtFutureAll(sources,open?3:2);testRequire(root->output!=NULL,"real aggregate producer");
    testRequire(xrtTaskGroupAdd(nested?root->child:root->group,root->output) &&
        xrtTaskGroupAdd(nested?root->child:root->group,root->output),"two distinct registration/list rights");
    root->feeder=xrtTaskGroupCreate(NULL);testRequire(root->feeder && xrtTaskGroupAdd(root->feeder,root->output),"non-member feeder");
    plan value;xrtownershipscope scope={0};freeze(&scope);snapshot(&value,root);
    const xrtownershippreparationv1* refused_preparation=(const xrtownershippreparationv1*)(uintptr_t)1;
    const xrtownershipfinishv1* refused_completion=(const xrtownershipfinishv1*)(uintptr_t)1;
    testRequire(!xrtTaskGroupOwnershipAdapterV2((xrtownershipref){(const void*)(uintptr_t)1,&ops},&refused_preparation,&refused_completion) &&
        refused_preparation==(const xrtownershippreparationv1*)(uintptr_t)1 && refused_completion==(const xrtownershipfinishv1*)(uintptr_t)1,
        "unknown identity refused without dereference or output changes");
    xtaskgroupwaitv1 untouched={sizeof(untouched),(xpromise*)(uintptr_t)1,99},wait=untouched;inputs seen={root->output,0,SIZE_MAX};
    testRequire(!xrtTaskGroupWaitV1(xrtTaskGroupOwnership(root->group),&token,&wait,input,&seen) &&
        !memcmp(&wait,&untouched,sizeof(wait)) && !seen.count,"open/unclaimed group leaves metadata and visitor untouched");
    for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];testRequire(item->adapter->Hold(item->ref.Data) && item->adapter->Claim(item->ref.Data,&token),"real plan pin and claim");}end(&scope);
    /* Normal close propagates to children; it does not cancel the cycle. */
    size_t group_index=find(&value,xrtTaskGroupOwnership(root->group));testRequire(group_index<LIMIT,"group node");
    testRequire(value.nodes[group_index].preparation->Prepare(root->group,&token)==XRT_OWNERSHIP_PREPARE_BUSY &&
        !xrtFutureDone(root->output),"prepare closes admission but never invents terminal completion");
    testRequire(xrtTaskGroupClose(root->feeder),"close feeder");
    freeze(&scope);
    xtaskgroup* target=nested?root->child:root->group;seen=(inputs){root->output,0,SIZE_MAX};wait=untouched;
    testRequire(!xrtTaskGroupWaitV1(xrtTaskGroupOwnership(target),&other,&wait,input,&seen) && !seen.count &&
        !memcmp(&wait,&untouched,sizeof(wait)),"wrong claim refuses without visitation");
    testRequire(xrtTaskGroupWaitV1(xrtTaskGroupOwnership(target),&token,&wait,input,&seen) && wait.Count==2 && seen.count==2,
        "stream actual duplicate slots with no source-array allocation");
    seen=(inputs){root->output,0,0};wait=untouched;testRequire(!xrtTaskGroupWaitV1(xrtTaskGroupOwnership(target),&token,&wait,input,&seen) &&
        seen.count==1 && !memcmp(&wait,&untouched,sizeof(wait)),"visitor failure preserves metadata output");
    TASK_GROUP_WAIT_PROBE(xrtTaskGroupOwnership(root->group),&token,!(any && open));
    TASK_GROUP_WAIT_PROBE(xrtTaskGroupOwnership(target),&token,!(any && open));
    TASK_GROUP_WAIT_PROBE(xrtTaskGroupOwnership(root->feeder),&token,false);
    end(&scope);
    if(abort_plan){
        freeze(&scope);for(size_t i=0;i<value.size;++i)value.nodes[i].adapter->Restore(value.nodes[i].ref.Data,&token);end(&scope);
        testRequire(xrtFutureCancel(root->output),"normal explicit abort cancellation");dispose(root);
    }else{
        testRequire(xrtFutureCancel(root->output) && xrtFutureState(root->done)==XFUTURE_RESOLVED,"normal callbacks complete the real Done producer");
        for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];if(item->preparation)
            testRequire(item->preparation->Prepare(item->ref.Data,&token)==XRT_OWNERSHIP_PREPARE_READY,"normal group drain and observer retirement");}
        freeze(&scope);
        size_t retired=0;
        for(size_t i=0;i<value.size;++i){node* item=&value.nodes[i];const xrtownershippreparationv1* preparation=NULL;const xrtownershipfinishv1* completion=NULL;
            testRequire(query(item->ref,&preparation,&completion)==item->adapter && preparation==item->preparation && completion==item->completion &&
                item->ref.Ops->Count(item->ref.Data,&item->refs),"same lifecycle identity and honest count after normal item unlink");
            if(xrtTaskGroupOwnershipAdapterV2(item->ref,&preparation,&completion) && !preparation){
                testRequire(!xrtTaskGroupOwnershipAdapterV1(item->ref,&refused_completion),"V1 keeps its active-item admission boundary");++retired;}}
        testRequire(retired==3u+(nested?1u:0u),"every normally unlinked physical item remains accounted");
        for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;value.nodes[i].adapter->Clear(value.nodes[i].ref.Data,&token);}end(&scope);
        testRequire(xrtMemDebugFailAfter(0),"zero-allocation physical finish");size_t remaining=value.size;
        for(size_t pass=0;remaining && pass<value.size+1;++pass){size_t progress=0;
            for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;node* item=&value.nodes[i];if(item->finished)continue;
                xrtownershipfinishresult result=item->completion?item->completion->Step(item->ref.Data,&token):
                    item->adapter->Finish(item->ref.Data,&token)?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_FAILED;
                testRequire(result!=XRT_OWNERSHIP_FINISH_FAILED,"physical completion failure is explicit");
                if(result==XRT_OWNERSHIP_FINISH_READY){item->finished=true;--remaining;++progress;}}
            testRequire(progress || !remaining,"completion dependency progress");}
        testRequire(!remaining && !xrtMemDebugFailTriggered(),"all pins retained through zero-allocation finish");xrtMemDebugFailClear();
    }
    for(size_t step=0;step<value.size;++step){size_t i=reverse?value.size-1-step:step;value.nodes[i].adapter->Drop(value.nodes[i].ref.Data);}
    testRequire(destroyed==1,"one terminal owner drop");balanced(&before);
}
int main(void)
{
    testRequire(xrtMemDebugEnable(true),"memory ledger");
    for(unsigned flags=0;flags<32;++flags)cycle(flags);
#ifdef XL6_TASK_GROUP_PREPARE_LINKED
    check_open_keys();
    testRequire(wait_probes==96,"all actual provider/claim decisions checked");
    puts("TaskGroup production wait solver: 96 real decisions; no mock providers");
#endif
    puts("TaskGroup preparation: 32 real nested/duplicate/ALL/ANY/open/feeder graphs; normal completion, stable retired anchors, abort and zero-allocation finish; full allocation ledger balanced");return 0;
}
