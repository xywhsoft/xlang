#define XRT_MODULE_ALL
#include "lib/xruntime/xruntime_features.h"
#if defined(XRUNTIME_IMPLEMENTATION) && !defined(XRT_IMPLEMENTATION)
#define XRT_IMPLEMENTATION
#endif
#include "lib/xrt/xrt.h"
#include "lib/xruntime/xruntime.h"
#include "src/runtime/xl6_execution_net_engine.h"
#include "src/runtime/xl6_execution_net_file.h"
#include "src/runtime/xl6_execution_resource_internal.h"
#include "src/runtime/xl6_ownership_catalog.h"
#include "src/runtime/xl6_ownership_domains.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef struct Root {
    volatile int32 refs;
    XL6_ExecutionSourceV1* source;
    XL6_ExecutionResourceV1 *engine,*file;
    xfuture* future;
    const void* claim;
    bool finalized,cleared,resurrect;
} Root;
static Root* escaped;
static unsigned cycles,finalizers,freed,resurrections,automatic,rollbacks,failures;
static void begin(xrtownershipscope* scope){assert(xrtOwnershipMutationBegin(scope));}
static void end(xrtownershipscope* scope){assert(xrtOwnershipScopeEnd(scope));}
static bool rootHold(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope scope={0};begin(&scope);
    bool ok=!root->cleared&&xrtRefRetain(&root->refs)>0;end(&scope);return ok;
}
static void rootDrop(const void* data)
{
    Root* root=(Root*)data;xrtownershipscope scope={0};begin(&scope);
    int32 refs=xrtRefRelease(&root->refs);assert(refs>=0);
    if(!refs){assert(root->cleared&&root->finalized&&!root->source&&!root->file&&!root->engine&&!root->future);xrtFree(root);++freed;}
    end(&scope);
}
static bool rootCount(const void* data,size_t* count)
{const Root* root=data;if(root->cleared||root->refs<=0)return false;*count=(size_t)root->refs;return true;}
static bool rootTrace(const void* data,xrtownershipvisitor visit,void* context)
{
    const Root* root=data;
    return (!root->source||visit(xl6ExecutionSourceOwnershipV1(root->source),context))&&
        (!root->engine||visit(xl6ExecutionResourceOwnershipV1(root->engine),context))&&
        (!root->file||visit(xl6ExecutionResourceOwnershipV1(root->file),context))&&
        (!root->future||visit(xrtFutureOwnership(root->future),context));
}
static const xrtownershipops rootOps={rootCount,rootTrace};
static xrtownershipref rootRef(const Root* root){return(xrtownershipref){root,&rootOps};}
static bool rootClaim(const void* data,const void* token)
{Root* root=(Root*)data;if(root->claim&&root->claim!=token)return false;root->claim=token;return true;}
static void rootRestore(const void* data,const void* token)
{Root* root=(Root*)data;assert(token&&root->claim==token&&!root->cleared);root->claim=NULL;}
static bool rootFinalize(const void* data,const void* token)
{
    Root* root=(Root*)data;assert(token&&root->claim==token&&!root->cleared);
    if(root->finalized)return true;
    root->finalized=true;++finalizers;
    if(root->resurrect){assert(!escaped&&rootHold(root));escaped=root;}
    return true;
}
static void rootClear(const void* data,const void* token)
{Root* root=(Root*)data;assert(token&&root->claim==token&&root->finalized&&!root->cleared);root->cleared=true;}
static const xrtownershipadapterv1 rootAdapter={sizeof(rootAdapter),rootHold,rootDrop,rootClaim,rootRestore,rootFinalize,rootClear,NULL};
static xrtownershipfinishresult rootFinish(const void* data,const void* token)
{
    Root* root=(Root*)data;assert(token&&root->claim==token&&root->cleared);
    xrtownershipscope scope={0};begin(&scope);
    XL6_ExecutionResourceV1* file=root->file;root->file=NULL;
    XL6_ExecutionResourceV1* engine=root->engine;root->engine=NULL;
    xfuture* future=root->future;root->future=NULL;end(&scope);
    xl6ExecutionResourceReleaseV1(file);xl6ExecutionResourceReleaseV1(engine);xrtFutureDestroy(future);
    XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(&root->source);
    if(!result.ok)return XRT_OWNERSHIP_FINISH_FAILED;
    return result.ready?XRT_OWNERSHIP_FINISH_READY:XRT_OWNERSHIP_FINISH_BUSY;
}
static const xrtownershipfinishv1 rootCompletion={sizeof(rootCompletion),&rootAdapter,rootFinish};
static bool resolveRoot(xrtownershipref ref,const XL6_OwnershipPoliciesV1* policies,
    const xrtownershipadapterv1** adapter,const xrtownershippreparationv1** prepare,const XL6_OwnershipFinishV1** finish)
{
    (void)policies;(void)prepare;if(ref.Ops!=&rootOps)return false;
    *adapter=&rootAdapter;*finish=&rootCompletion;return true;
}
static xnetportbackend backend(void)
{
#if defined(_WIN32)
    return XNET_PORT_IOCP;
#else
    return XNET_PORT_URING;
#endif
}
static XL6_ExecutionResourceV1* engineCreate(XL6_ExecutionSourceV1* source)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend();config.Workers=1;config.NodeCacheBytes=0;
    XL6_ExecutionResourceV1* engine=xl6ExecutionNetEngineCreateV1(source,&config);
    assert(engine&&xl6ExecutionNetEngineStartV1(engine));return engine;
}
static XL6_ExecutionResourceV1* fileCreate(XL6_ExecutionSourceV1* source,XL6_ExecutionResourceV1* engine,const char* path)
{
    xfileoptions options;xrtFileOptionsInit(&options);options.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE;
    return xl6ExecutionNetFileCreateV1(source,engine,0,path,&options);
}
static void waitFuture(xfuture* future)
{assert(future);double deadline=xrtTimer()+10;while(xrtFutureState(future)==XFUTURE_PENDING){assert(xrtTimer()<deadline);xrtThreadYield();}}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount||before.LiveBytes!=after.LiveBytes)
        fprintf(stderr,"memory before=%zu/%zu after=%zu/%zu\n",before.LiveCount,before.LiveBytes,after.LiveCount,after.LiveBytes);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount);
}
static void destroySource(XL6_ExecutionSourceV1** source,bool failed)
{
    assert(xl6ExecutionSourceCloseV1(*source));double deadline=xrtTimer()+10;
    while(*source){XL6_ExecutionRetireResultV1 result=xl6ExecutionSourceDestroyV1(source);
        assert(result.ok!=failed);if(result.ready)break;assert(xrtTimer()<deadline);xrtClearError();xrtThreadYield();}
}
static XL6_OwnershipPlanResult createPlan(Root* root,XL6_OwnershipCatalog* catalog,XL6_OwnershipPlan** plan)
{
    double deadline=xrtTimer()+10;
    for(;;){XL6_OwnershipPlanResult state=xl6OwnershipPlanTryCreateV6(rootRef(root),xl6OwnershipCatalogProbeV1,NULL,false,catalog,plan);
        if(state>XL6_OWNERSHIP_PLAN_BUSY)return state;
        assert(xrtTimer()<deadline);xrtClearError();xrtThreadYield();}
}
static void destroyPlan(XL6_OwnershipPlan* plan)
{double deadline=xrtTimer()+10;while(xl6OwnershipPlanTryDestroy(plan)!=XL6_OWNERSHIP_PLAN_READY){assert(xrtTimer()<deadline);xrtThreadYield();}}
static void cycle(XL6_OwnershipCatalog* catalog,unsigned mode,bool resurrect)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    const char* path="net-file-plan.bin";Root* root=xrtCalloc(1,sizeof(*root));assert(root);root->refs=1;root->resurrect=resurrect;
    root->source=xl6ExecutionSourceCreateV1();assert(root->source);root->engine=engineCreate(root->source);
    root->file=fileCreate(root->source,root->engine,path);assert(root->file);
    if(mode==1)root->future=xl6ExecutionNetFileWriteV1(root->file,0,XRT_BYTES_LITERAL("a\0b"),NULL);
    if(mode==2)root->future=xl6ExecutionNetFileReadV1(root->file,0,8,NULL);
    if(root->future){waitFuture(root->future);assert(xrtFutureState(root->future)==XFUTURE_RESOLVED);}
    XL6_OwnershipPlan* plan=NULL;assert(createPlan(root,catalog,&plan)==XL6_OWNERSHIP_PLAN_ROOTED&&plan);
    rootDrop(root);double deadline=xrtTimer()+10;
    for(;;){
        XL6_OwnershipStepResult step=xl6OwnershipPlanPrepare(plan);assert(step!=XL6_OWNERSHIP_STEP_FAILED);
        XL6_OwnershipPlanResult state=xl6OwnershipPlanTryRefresh(plan);
        if(state==XL6_OWNERSHIP_PLAN_FINALIZERS){assert(xl6OwnershipPlanFinalizeV2(plan)!=XL6_OWNERSHIP_STEP_FAILED);}
        state=xl6OwnershipPlanTryCommit(plan);
        if(state==XL6_OWNERSHIP_PLAN_READY)break;
        if(escaped){
            assert(escaped==root&&state==XL6_OWNERSHIP_PLAN_ROOTED);
            destroyPlan(plan);plan=NULL;assert(createPlan(root,catalog,&plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            rootDrop(escaped);escaped=NULL;++resurrections;
        }
        if(xrtTimer()>=deadline){fprintf(stderr,"file plan timeout: mode=%u step=%d state=%d finalized=%u\n",mode,(int)step,(int)state,root->finalized);abort();}
        xrtClearError();xrtThreadYield();
    }
    for(;;){XL6_OwnershipStepResult result=xl6OwnershipPlanFinishV2(plan);if(result==XL6_OWNERSHIP_STEP_READY)break;
        assert(result==XL6_OWNERSHIP_STEP_BUSY&&xrtTimer()<deadline);xrtThreadYield();}
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);destroyPlan(plan);
    assert(remove(path)==0);balanced(before);++cycles;
}
static void automaticClose(void)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    const char* path="net-file-auto.bin";XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    XL6_ExecutionResourceV1* engine=engineCreate(source);XL6_ExecutionResourceV1* file=fileCreate(source,engine,path);assert(file);
    xfuture* pending=xl6ExecutionNetFileReadV1(file,0,8,NULL);assert(pending);xrtFutureDestroy(pending);
    xl6ExecutionResourceReleaseV1(file);xl6ExecutionResourceReleaseV1(engine);
    destroySource(&source,false);assert(remove(path)==0);balanced(before);++automatic;
}

/* Real native file, synthetic resident PROVIDER cleanup outcome. This tests
 * generic rollback retention/checked failure, not an injected OS close error. */
typedef struct RejectedInput {XL6_ExecutionSourceV1* source;xnetengine* engine;bool fail;unsigned* drops;} RejectedInput;
/* Serialized fixture-only factory input, borrowed for this synchronous call;
 * never invent a conflicting private production options layout or erase it. */
static const RejectedInput* rejectedInput;
typedef struct Rejected {XL6_ExecutionResourceV1 base;xnetfile* file;xfuture* close;bool fail;unsigned* drops;} Rejected;
static bool rejectedCreate(XL6_ExecutionResourceV1* base,const XL6_ExecutionCreateOptionsV1* options)
{
    Rejected* r=(Rejected*)base;const RejectedInput* input=rejectedInput;(void)options;assert(input);
    xfileoptions opts;xrtFileOptionsInit(&opts);opts.Flags=XFILE_READ|XFILE_WRITE|XFILE_CREATE|XFILE_TRUNCATE;
    r->file=xrtNetFileCreate(input->engine,0,"net-file-rejected.bin",&opts);assert(r->file);
    r->fail=input->fail;r->drops=input->drops;
    assert(xl6ExecutionSourceCloseV1(input->source));return true;
}
static bool rejectedRollback(XL6_ExecutionResourceV1* base){(void)base;return false;}
static bool rejectedPresent(const XL6_ExecutionResourceV1* base){return ((const Rejected*)base)->file!=NULL;}
static bool rejectedClose(XL6_ExecutionResourceV1* base){(void)base;return true;}
static bool rejectedTrace(const XL6_ExecutionResourceV1* base,xrtownershipvisitor visit,void* context)
{const Rejected* r=(const Rejected*)base;return !r->file||visit(xrtNetFileOwnership(r->file),context);}
static void rejectedDrop(XL6_ExecutionResourceV1* base)
{Rejected* r=(Rejected*)base;assert(!r->file&&!r->close);++*r->drops;}
static bool rejectedFinish(XL6_ExecutionResourceV1* base){(void)base;return false;}
static XL6_ModulePrepareResult rejectedRetire(XL6_ExecutionResourceV1* base)
{
    Rejected* r=(Rejected*)base;if(!r->file)return XL6_MODULE_PREPARE_READY;
    if(!r->close){r->close=xrtNetFileCloseFuture(r->file);assert(r->close);}
    if(xrtFutureState(r->close)==XFUTURE_PENDING)return XL6_MODULE_PREPARE_BUSY;
    assert(xrtFutureState(r->close)==XFUTURE_RESOLVED);xrtFutureDestroy(r->close);r->close=NULL;
    xrtNetFileDestroy(r->file);r->file=NULL;
    if(r->fail){xrtSetErrorKind(XERR_IO);return XL6_MODULE_PREPARE_FAILED;}
    return XL6_MODULE_PREPARE_READY;
}
static const XL6_ExecutionFamilyV1 rejectedFamily={sizeof(rejectedFamily),XL6_EXECUTION_FAMILY_TAG_V1,XL6_EXECUTION_NET_FILE_V1,
    rejectedCreate,rejectedRollback,rejectedRetire,rejectedFinish,rejectedDrop,rejectedTrace,rejectedClose,rejectedClose,sizeof(Rejected),rejectedPresent};
static void rejectedPublication(unsigned mode)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    XL6_ExecutionSourceV1* source=xl6ExecutionSourceCreateV1();assert(source);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend();config.Workers=1;config.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&config);assert(engine&&xrtNetEngineStart(engine));unsigned drops=0;
    RejectedInput file={source,engine,mode==1,&drops};rejectedInput=&file;
    XL6_ExecutionCreateOptionsV1 options={.kind=XL6_EXECUTION_NET_FILE_V1,.deferRollback=mode!=2};
    assert(!xl6ExecutionCreateInternalV1(source,&rejectedFamily,&options)&&xrtErrorKind(xrtGetError())==XERR_STATE);xrtClearError();
    rejectedInput=NULL;
    if(mode){
        double deadline=xrtTimer()+10;
        while(xl6ExecutionSourceCheckV1(source)){assert(xrtTimer()<deadline);xrtThreadYield();}
        if(mode==1)assert(xrtErrorKind(xrtGetError())==XERR_IO);
        else assert(!xrtGetError());
        xrtClearError();assert(xl6ExecutionSourceRetryV1(source));++failures;
    }
    destroySource(&source,mode!=0);assert(drops==1);xrtClearError();
    assert(xrtNetEngineStop(engine)&&xrtNetEngineDestroy(engine));assert(remove("net-file-rejected.bin")==0);
    balanced(before);++rollbacks;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();assert(xrtTemp(8)&&xrtTempClear());
    const XL6_OwnershipDomainV1 rootDomain={sizeof(rootDomain),NULL,resolveRoot,NULL};
    const XL6_OwnershipDomainV1* domains[]={&rootDomain,xl6OwnershipDomainExecutionSourceV1Get(),
        xl6OwnershipDomainExecutionNetEngineV1Get(),xl6OwnershipDomainNetEngineNativeV1Get(),
        xl6OwnershipDomainNetBufferNativeV1Get(),xl6OwnershipDomainExecutionFailuresV1Get(),
        xl6OwnershipDomainNetFileNativeV1Get(),xl6OwnershipDomainExecutionNetFileV1Get(),xl6OwnershipDomainNativeFutureV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    for(unsigned i=0;i<4;++i)for(unsigned mode=0;mode<3;++mode){cycle(catalog,mode,false);cycle(catalog,mode,true);}
    xl6OwnershipCatalogDestroyV1(catalog);
    for(unsigned i=0;i<8;++i){automaticClose();for(unsigned mode=0;mode<3;++mode)rejectedPublication(mode);}
    assert(cycles==24&&finalizers==24&&freed==24&&resurrections==12&&automatic==8&&rollbacks==24&&failures==16);
    assert(xrtMemDebugReset());
    printf("NetFile execution: XLIR cycles=%u finalized=%u freed=%u resurrections=%u auto=%u rejected-publication=%u checked-failures=%u; exact memory\n",
        cycles,finalizers,freed,resurrections,automatic,rollbacks,failures);return 0;
}
