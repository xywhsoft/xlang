/* Actual host Module/entry/read code and resident Class lifetime. The image
 * token below is not TCC code: this is NOT generated binding/JIT/teardown. */
#include "src/api/xl_module_state.h"
#include "src/api/xl_api_lock.h"
#include "src/api/xl_module_read.inc"
#include "src/runtime/xl6_persistence_restore_plan.h"
#include "src/runtime/xl6_restore_class.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define READ_ID UINT64_C(0xB160)
#define READ_TAG INT64_C(160)
static XL_Module readModule;
static XL6_CodeOwnerGraph readOwner;
static xvalue* readGlobal;
static unsigned readTraces,readDrops,readClones,readFactories,unknownReads;
static bool readShared;
static int readAcquire(void* data)
{ XL_Module* m=data; xlApiLock(); assert(m->codeLeases<SIZE_MAX); ++m->codeLeases; xlApiUnlock(); return 1; }
static void readRelease(void* data)
{ XL_Module* m=data; xlApiLock(); assert(m->codeLeases); --m->codeLeases; xlApiUnlock(); }
static bool readTrace(XL6_ModuleOwnershipVisit visit,void* context)
{ ++readTraces; return visit(xrtValueOwnership(readGlobal),context); }
static int readFinish(void) { assert(0); return 0; }
static const XL6_ModuleRetirementV1 readContract={sizeof(readContract),UINT64_C(0x584C364D525431),readTrace,readFinish};
static const XL6_ModuleReadContractV1 readCertificate={sizeof(readCertificate),UINT64_C(0x584C364D524431),readTrace};
static bool readFinalize(xvalue* value,void* context)
{ (void)value; (void)context; assert(!xl__moduleRead); ++readDrops; return true; }
static xvalue* readObject(void)
{
    xvalue* value=xl6ClassRuntimeV2Get()->Create(&readOwner.base,readFinalize);
    if (!value) return NULL;
    if (!xrtValueTypeIdBind(value,READ_ID) ||
        !xrtValueObjectSetNew(value,XRT_STR_LITERAL("$xlir.class.runtime-type"),xrtValueInt(READ_TAG)) ||
        !xrtValueObjectSetNew(value,XRT_STR_LITERAL("n"),xrtValueInt(7))) {
        xerror* error=xrtTakeError(); xrtValueRelease(value); xrtClearError(); xrtSetErrorTake(error); return NULL;
    }
    return xl6ClassRuntimeV2Get()->CommitTake(value);
}
static xvalue* readFactory(void)
{ assert(!xl__moduleRead); ++readFactories; return readShared ? xrtValueRetain(readGlobal) : readObject(); }
static xvalue* readClone(const xvalue* value)
{
    xrtownershipscope freeze={0};
    assert(!xl__moduleRead && value==readGlobal && xrtOwnershipFreezeTryBegin(&freeze));
    assert(xrtOwnershipScopeEnd(&freeze)); ++readClones; return readObject();
}
static bool unknownCount(const void* data,size_t* count)
{ (void)data; (void)count; ++unknownReads; return false; }
static bool unknownTrace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; ++unknownReads; return false; }
static const xrtownershipops unknownOps={unknownCount,unknownTrace};
static void readInit(void)
{
    assert(!xl__moduleRead && !xl__moduleCalls);
    readModule=(XL_Module){0}; readModule.state=(TCCState*)&readModule;
    readModule.codeOwnerReady=1; readModule.activation=XL_MODULE_ACTIVE;
    readModule.ownershipTrace=readTrace; readModule.retirementContract=&readContract;
    readModule.ownershipReadContract=&readCertificate;
    readModule.codeOwnerGraph=(XL6_CodeOwnerGraph){{sizeof(XL6_CodeOwnerGraph),&readModule,readAcquire,readRelease},
        UINT64_C(0x584C36434F574E31),&xlModuleOwnershipOps};
    readOwner=readModule.codeOwnerGraph; readShared=false;
}
static void readEnter(XL6_ModuleCall* call)
{ xlApiLock(); assert(xlModuleBeginCallUnlocked(&readModule)); xlModuleCallAttachUnlocked(&readModule,call); xlApiUnlock(); }
static void readLeave(XL6_ModuleCall* call)
{
    xlApiLock(); xlModuleCallDetachUnlocked(&readModule,call);
    /* External image token, no TCC deletion tail. */
    assert(readModule.activeCalls); --readModule.activeCalls; xlApiUnlock();
}
static int64_t readNumber(const xvalue* value)
{ int64_t n=0; assert(xrtValueGetInt(xrtValueObjectGet(value,XRT_STR_LITERAL("n")),&n)); return n; }
static XL6_RestorePlanV1 readPlan(XL6_RestoreNodeV1* nodes,XL6_RestoreFieldV1* field,bool boundary,bool clone)
{
    nodes[0]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_OBJECT_V1,.identity=READ_ID,
        .as.object={readFactory,READ_TAG,0,1,xl6RestoreClassProviderV1()},.clone=clone ? readClone : NULL};
    nodes[1]=(XL6_RestoreNodeV1){.kind=XL6_RESTORE_SCALAR_V1,.identity=UINT64_C(0xB161),.as.scalar=XL6_VALUE_ADMIT_I64};
    *field=(XL6_RestoreFieldV1){XRT_STR_INIT("n"),XRT_STR_INIT("n"),1,XL6_RESTORE_REQUIRED_V1};
    return (XL6_RestorePlanV1){sizeof(XL6_RestorePlanV1),2,1,0,nodes,field,false,
        boundary ? xlModuleOwnershipReadBoundaryV1() : NULL};
}
static xvalue* readWire(void)
{ xvalue* wire=xrtValueObject(); assert(wire && xrtValueObjectSetNew(wire,XRT_STR_LITERAL("n"),xrtValueInt(42))); return wire; }
static bool readRun(unsigned mode,size_t point,bool fault)
{
    readInit(); readShared=mode==2 || mode==3;
    readGlobal=readShared ? readObject() : NULL; assert(!readShared || readGlobal);
    XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field;
    XL6_RestorePlanV1 plan=readPlan(nodes,&field,mode!=1,mode==3);
    xvalue* wire=readWire(); XL6_ModuleCall call={0}; readEnter(&call);
    XL6_CodeLease* lease=NULL; XL6_CodeScope execution={0};
    assert(xl6CodeLeaseAcquire(&readOwner.base,&lease)==XL6_CODE_LEASE_ACQUIRED && lease);
    xl6CodeScopeEnter(&execution,lease);
    unsigned clones=readClones,factories=readFactories,traces=readTraces;
    xl6_error_scope error={0}; assert(xl6ErrorScopeBegin(&error));
    if (fault) assert(xrtMemDebugFailAfter(point));
    xvalue* result=xl6PersistenceRestorePlanTakeV1(NULL,0,NULL,wire,&plan);
    bool hit=fault && xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&error);
    bool success=(mode==0 || mode==3) && !hit;
    assert(success ? result && outcome.ok && result!=readGlobal && readNumber(result)==42 : !result && !outcome.ok);
    assert(!xl__moduleRead && readModule.activeCalls==1 && readFactories<=factories+1);
    if (!fault) { assert(readFactories==factories+1); assert(readClones==clones+(mode==3 ? 1u : 0u)); }
    if (readGlobal) assert(readNumber(readGlobal)==7);
    if (mode==1) assert(readTraces==traces);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(wire);
    xl6CodeScopeLeave(&execution); xl6CodeLeaseRelease(lease); readLeave(&call);
    xrtValueRelease(readGlobal); readGlobal=NULL;
    assert(!readModule.activeCalls && !readModule.codeLeases && !xl__moduleCalls && !xl__moduleRead);
    xrtClearError(); return hit;
}
typedef struct ReadWorker { unsigned mode; xatomic64 ready,release; XL6_CodeLease* lease; } ReadWorker;
static int32 readWorker(void* data)
{
    ReadWorker* w=data; XL6_ModuleCall call={0}; XL6_CodeScope code={0};
    if (w->mode==0) readEnter(&call);
    else if (w->mode==1) xl6CodeScopeEnter(&code,w->lease);
    else xlApiLock();
    xrtAtomic64Store(&w->ready,1,XMEMORY_RELEASE);
    while (!xrtAtomic64Load(&w->release,XMEMORY_ACQUIRE)) xrtThreadYield();
    if (w->mode==0) readLeave(&call);
    else if (w->mode==1) xl6CodeScopeLeave(&code);
    else xlApiUnlock();
    return 0;
}
static void readThreads(unsigned mode)
{
    readInit(); XL6_ModuleCall call={0}; readEnter(&call);
    ReadWorker w={.mode=mode}; xrtAtomic64Init(&w.ready,0); xrtAtomic64Init(&w.release,0);
    assert(xl6CodeLeaseAcquire(&readOwner.base,&w.lease)==XL6_CODE_LEASE_ACQUIRED);
    xthread* thread=xrtThreadCreate(readWorker,&w,0); assert(thread);
    while (!xrtAtomic64Load(&w.ready,XMEMORY_ACQUIRE)) xrtThreadYield();
    unsigned traces=readTraces;
    if (mode==2) {
        XL6_OwnershipReadScopeV1 scope={0};
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* diagnostic=xrtGetError();
        assert(xlModuleReadBegin(&scope)==XL6_OWNERSHIP_READ_BUSY_V1 && !scope.self && !scope.thread &&
            xrtGetError()==diagnostic); xrtClearError();
    } else {
        XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field; XL6_RestorePlanV1 plan=readPlan(nodes,&field,true,false);
        xvalue* wire=readWire(); xl6_error_scope error={0}; assert(xl6ErrorScopeBegin(&error));
        xvalue* result=xl6PersistenceRestorePlanTakeV1(NULL,0,NULL,wire,&plan);
        xl6_error_result outcome=xl6ErrorScopeEnd(&error);
        assert(!result && !outcome.ok && !xl__moduleRead); xrtErrorFree(outcome.error); xrtValueRelease(wire); xrtClearError();
    }
    assert(readTraces==traces);
    xrtAtomic64Store(&w.release,1,XMEMORY_RELEASE);
    assert(xrtThreadWait(thread)==XWAIT_OK); xrtThreadDestroy(thread);
    xl6CodeLeaseRelease(w.lease); readLeave(&call);
    assert(!readModule.codeLeases && !readModule.activeCalls);
}
static void foreignFieldsDrop(ptr raw,ptr user) { (void)raw; (void)user; }
static void readRefusals(void)
{
    readInit(); XL6_ModuleCall outer={0},inner={0}; readEnter(&outer); readEnter(&inner);
    void* raw=xl6_xrt_dynamic_fields_create(); assert(raw);
    void* fields=raw;
    xvalue* boxed=xl6NativeHandleTakeV1(&raw,xl6DynamicFieldsHandlePolicyV1Get()); assert(boxed && !raw);
    static const xvaluehandleops foreignFields={.Drop=foreignFieldsDrop}; raw=(void*)1;
    xvalue* foreign=xrtValueHandleTake(&raw,&foreignFields,NULL); assert(foreign);
    XL6_OwnershipReadScopeV1 read={0},nested={0}; xrtownershipscope freeze={0}; size_t count=0;
    assert(xlModuleReadBegin(&read)==XL6_OWNERSHIP_READ_READY_V1);
    assert(xlModuleReadBegin(&nested)==XL6_OWNERSHIP_READ_BUSY_V1 && !nested.self && !nested.thread);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(xlModuleReadAdmits(&read,(xrtownershipref){&readModule,&xlModuleOwnershipOps}));
    assert(xlModuleOwnershipCount(&readModule,&count) && count==3); /* handle + TWO real entries */
    XL6_OwnershipReadScopeV1 copied=read;
    assert(xlModuleReadAdmits(&read,xrtDynamicFieldsOwnership(fields)));
    assert(xlModuleReadAdmits(&read,xrtValueOwnership(boxed)));
    assert(!xlModuleReadAdmits(&copied,xrtDynamicFieldsOwnership(fields)));
    assert(!xlModuleReadAdmits(&read,xrtValueOwnership(foreign)) && !unknownReads);
    assert(!xlModuleReadAdmits(&copied,(xrtownershipref){&readModule,&xlModuleOwnershipOps}));
    assert(!xlModuleReadAdmits(&read,(xrtownershipref){&readModule,&unknownOps}) && !unknownReads);
    readModule.ownershipReadContract=NULL;
    assert(!xlModuleReadAdmits(&read,(xrtownershipref){&readModule,&xlModuleOwnershipOps}));
    readModule.ownershipReadContract=&readCertificate;
    ++readModule.activeCalls; /* Unregistered/opaque entry cannot claim pause. */
    assert(!xlModuleReadAdmits(&read,(xrtownershipref){&readModule,&xlModuleOwnershipOps})); --readModule.activeCalls;
    readModule.retirementRunning=1;
    assert(!xlModuleReadAdmits(&read,(xrtownershipref){&readModule,&xlModuleOwnershipOps})); readModule.retirementRunning=0;
    assert(xrtOwnershipScopeEnd(&freeze)); xlModuleReadEnd(&read);
    assert(!read.self && !read.thread && !xl__moduleRead && !xlModuleOwnershipCount(&readModule,&count));
    xrtValueRelease(boxed); xrtValueRelease(foreign);
    readLeave(&inner); readLeave(&outer);
    readOwner.ops=&unknownOps; XL6_ModuleCall call={0}; readEnter(&call);
    XL6_RestoreNodeV1 nodes[2]; XL6_RestoreFieldV1 field; XL6_RestorePlanV1 plan=readPlan(nodes,&field,true,false);
    xvalue* wire=readWire(); xl6_error_scope error={0}; assert(xl6ErrorScopeBegin(&error));
    xvalue* result=xl6PersistenceRestorePlanTakeV1(NULL,0,NULL,wire,&plan);
    xl6_error_result outcome=xl6ErrorScopeEnd(&error);
    assert(!result && !outcome.ok && !unknownReads); xrtErrorFree(outcome.error); xrtValueRelease(wire);
    readLeave(&call); assert(!readModule.codeLeases); xrtClearError();
}
void xl6TestRestoreModuleReadV1(void)
{
    for (unsigned mode=0;mode<4;++mode) assert(!readRun(mode,0,false));
    readRefusals(); for (unsigned mode=0;mode<3;++mode) readThreads(mode);
    printf("actual host read: paused current entries/scopes remain roots; foreign entry/scope/host-lock and unknown policy refuse; clone outside fence\n");
}
bool xl6TestRestoreModuleReadFaultV1(const char* name,size_t point)
{
    unsigned mode=!strcmp(name,"plan-module-clone") ? 3u : 0u;
    assert(mode==3 || !strcmp(name,"plan-module-read"));
    bool hit=readRun(mode,point,true);
    printf("native plan %s point=%zu hit=%d complete-prefix balanced\n",name,point,hit); return hit;
}
