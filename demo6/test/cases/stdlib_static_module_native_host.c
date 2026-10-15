#include "src/runtime/xl6_static_module.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_stack.h"
#include "lib/xruntime/xruntime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REF_ARGS UINT64_C(102),UINT64_C(202),3,sizeof(xvalue*),xrtTypeValue()
static XL6_StaticModule* module;
static XL6_OwnershipCatalog* catalog;
static xvalue* root;
static unsigned finalized,finished,preflightCalls,finishBusy;
static bool noDiagnostic,failFinalizer,failPreflight,failFinish,reenter,detached,environmentAlive;
static bool spawn,resurrect;
static bool laterDiagnostic;
static XL6_CodeLease* resurrected;
static bool trace(XL6_ModuleOwnershipVisit visit,void* context)
{ return visit(xrtValueOwnership(root),context); }
static int finish(void)
{
    ++finished;assert(finalized==1 && (root || detached));
    xrtValueRelease(root);root=NULL;environmentAlive=false;return 1;
}
static XL6_ModulePrepareResult prepare(void)
{
    ++preflightCalls;
    if(failPreflight){failPreflight=false;if(!noDiagnostic)xrtSetErrorKind(XERR_RANGE);return XL6_MODULE_PREPARE_FAILED;}
    return XL6_MODULE_PREPARE_READY;
}
static XL6_ModuleRetireResult step(void)
{
    if(finishBusy){--finishBusy;return XL6_MODULE_RETIRE_BUSY;}
    if(failFinish){failFinish=false;if(!noDiagnostic || laterDiagnostic)xrtSetErrorKind(XERR_ARGUMENT);return XL6_MODULE_RETIRE_FAILED;}
    return finish() ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_FAILED;
}
static bool finalize(xvalue* object,void* context)
{
    (void)context;int64_t id=0;
    assert(environmentAlive && (root || detached) && xrtValueGetInt(xrtValueObjectGet(object,XRT_STR_LITERAL("id")),&id) && id==7);
    xvalue* links=xrtValueObjectGet(object,XRT_STR_LITERAL("links"));
    assert(links && xl6StackSizeV1(links,REF_ARGS,false)==1 && xrtValueCount(object)==2);
    xrtownershipscope freeze={0};assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    assert(++finalized==1);
    if(reenter){XL6_StaticModuleResultV1 busy=xl6StaticModuleRetireV1(module);assert(!busy.ready && busy.ok);}
    if(spawn){
        const XL6_ClassRuntimeV2* api=xl6ClassRuntimeV2Get();
        xvalue* child=api->Create(xl6StaticModuleOwnerV1(module),NULL);assert(child);
        assert(api->CommitTake(child)==child && xl6StackPushV1(links,REF_ARGS,&child));xrtValueRelease(child);
    }
    if(resurrect){assert(xl6CodeLeaseAcquireScoped(xl6StaticModuleOwnerV1(module),&resurrected)==XL6_CODE_LEASE_ACQUIRED && resurrected);}
    if(failFinalizer){if(!noDiagnostic)xrtSetErrorKind(XERR_RANGE);return false;}
    return true;
}
static void setup(void)
{
    static const XL6_ModuleRetirementV1 graph={sizeof(graph),0x584C364D525431ULL,trace,finish};
    const XL6_OwnershipDomainV1* domains[]={
#if defined(XL6_TEST_SELECTED_OWNERSHIP_DOMAINS)
#include "selected.domains.h"
#else
        xl6OwnershipDomainClassV1Get(),xl6OwnershipDomainCodeLeaseV1Get(),xl6OwnershipDomainSequenceV1Get()
#endif
    };
    catalog=xl6OwnershipCatalogCreateV1(domains,sizeof(domains)/sizeof(*domains));assert(catalog);
    XL6_StaticModuleContractV1 contract={sizeof(contract),&graph,prepare,step,xl6OwnershipCatalogResolveV1,catalog};
    module=xl6StaticModuleCreateV1(&contract);assert(module);environmentAlive=true;
    const XL6_ClassRuntimeV2* api=xl6ClassRuntimeV2Get();
    root=api->Create(xl6StaticModuleOwnerV1(module),finalize);assert(root);
    assert(xrtValueObjectSetNew(root,XRT_STR_LITERAL("id"),xrtValueInt(7)));
    xvalue* links=xl6StackCreateV1(REF_ARGS);assert(links);
    assert(xrtValueObjectSetNew(root,XRT_STR_LITERAL("links"),links));
    assert(api->CommitTake(root)==root && xl6StackPushV1(links,REF_ARGS,&root));
}
static void run(unsigned mode)
{
    finalized=finished=preflightCalls=finishBusy=0;
    noDiagnostic=mode==4 || mode==6 || mode==8 || mode==14;
    failFinalizer=mode==3 || mode==4;failPreflight=mode==5 || mode==6 || mode==14 || mode==15;
    failFinish=mode==7 || mode==8 || mode==14 || mode==15;reenter=mode==9;
    detached=mode==10;
    spawn=mode==12;resurrect=mode==13;resurrected=NULL;
    laterDiagnostic=mode==14;
    if(mode==2)finishBusy=2;
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);setup();
    xvalue* escaped=mode==1 ? xrtValueRetain(root) : NULL;
    if(detached){xrtValueRelease(root);root=NULL;}
    if(mode==11){XL6_StaticModule* beforePointer=module;assert(!xl6StaticModuleDestroyV1(&module) && module==beforePointer && root);xrtClearError();}
    xerror* incoming=NULL;
    if(mode==16){xrtSetErrorKind(XERR_STATE);incoming=xrtErrorRef(xrtGetError());assert(incoming);}
    XL6_StaticModuleResultV1 result={false,true};
    if(escaped){result=xl6StaticModuleRetireV1(module);assert(!result.ready && result.ok && !finalized && root);xrtValueRelease(escaped);}
    bool observedFailure=false;
    for(unsigned turn=0;turn<64 && !result.ready;++turn){
        result=xl6StaticModuleRetireV1(module);
        if(resurrected){assert(!result.ready && !finished && root && environmentAlive);xl6CodeLeaseRelease(resurrected);resurrected=NULL;resurrect=false;}
        if(!result.ok){
            observedFailure=true;
            assert(noDiagnostic ? xrtGetError()==NULL : xrtGetError()!=NULL);
            if(mode==15)assert(xrtErrorKind(xrtGetError())==XERR_RANGE);
            xrtClearError();
        }
    }
    assert(result.ready && finalized==1 && finished==1 && !root);
    assert(observedFailure==((mode>=3 && mode<=8) || mode==14 || mode==15));
    assert(result.ok==!observedFailure);
    XL6_CodeLease* closed=NULL;assert(xl6CodeLeaseAcquire(xl6StaticModuleOwnerV1(module),&closed)==XL6_CODE_LEASE_CLOSED && !closed);
    assert(xl6StaticModuleDestroyV1(&module) && !module);
    xl6OwnershipCatalogDestroyV1(catalog);catalog=NULL;
    if(incoming){assert(xrtGetError()==incoming);xrtErrorFree(incoming);}
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
        fprintf(stderr,"mode=%u live=%zu/%zu bytes=%zu/%zu\n",mode,before.LiveCount,after.LiveCount,before.LiveBytes,after.LiveBytes);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("static module mode=%u actual class/Stack cycle, fields intact, once-only destructor, checked recovery balanced\n",mode);
}
static bool oom(size_t point)
{
    finalized=finished=preflightCalls=finishBusy=0;
    noDiagnostic=failFinalizer=failPreflight=failFinish=reenter=detached=spawn=resurrect=false;
    laterDiagnostic=false;
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);setup();
    assert(xrtMemDebugFailAfter(point));
    XL6_StaticModuleResultV1 result={false,true};bool hit=false;
    for(unsigned turn=0;turn<64 && !result.ready && !hit;++turn){
        result=xl6StaticModuleRetireV1(module);hit=xrtMemDebugFailTriggered();
    }
    xrtMemDebugFailClear();
    assert(hit ? !result.ok : result.ok);xrtClearError();
    for(unsigned turn=0;turn<64 && !result.ready;++turn){
        result=xl6StaticModuleRetireV1(module);assert(hit ? !result.ok : result.ok);xrtClearError();
    }
    assert(result.ready && finalized==1 && finished==1 && !root);
    assert(xl6StaticModuleDestroyV1(&module) && !module);
    xl6OwnershipCatalogDestroyV1(catalog);catalog=NULL;xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
    return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    if(!strcmp(argv[1],"--oom")){
        size_t point=0;while(point<1000 && oom(point))++point;assert(point<1000);
        printf("static module retirement full prefixes=%zu first-no-hit=%zu balanced\n",point+1,point);return 0;
    }
    unsigned mode=(unsigned)strtoul(argv[1],NULL,10);assert(mode<17);
    run(mode);return 0;
}
