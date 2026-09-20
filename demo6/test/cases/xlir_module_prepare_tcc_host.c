#define JOINT_TCC_ASYNC
#ifdef MODULE_PREPARE_LEGACY_V2
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_prepare_legacy_artifact.c"
#else
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_module_prepare_artifact.c"
#endif
#define main unused_async_main
#include "xlir_one_shot_tcc_host.c"
#undef main

typedef struct FutureEscapeHooks { void (*capture)(xvalue*); void (*dropped)(int64_t); } FutureEscapeHooks;
static xvalue* alias;
static unsigned choice, internal_drops, external_drops, prepares, preparation_probes, forbidden_calls;
static unsigned preparation_ready, preparation_busy, preparation_failed;
#ifdef MODULE_PREPARE_LEGACY_V2
typedef XL6_ModuleRetirementV2 Preparation;
typedef XL6_ModuleRetirementGetV2 PreparationGet;
#define PREPARE_SYMBOL "xl_script_module_retirement_v2"
#define PREPARE_FIELD prepare
#define PREPARE_VERSION 2
#else
typedef XL6_ModuleRetirementV3 Preparation;
typedef XL6_ModuleRetirementGetV3 PreparationGet;
#define PREPARE_SYMBOL "xl_script_module_retirement_v3"
#define PREPARE_FIELD preflight
#define PREPARE_VERSION 3
#endif
static const Preparation* original_preparation;
static Preparation preparation;
static HANDLE producer_entered, producer_release;
static XL6_TaskOutcome blocked_run(xcancel* cancel,void* data,XL6_TaskResult* result)
{
    (void)cancel; (void)result;
    if(choice==17) {
        xlDestroyModule(module);
        assert(!atomic_load(&deleted) && !module->retirementFailed);
        if(PREPARE_VERSION==2)assert(!module->retirementPrepared);
        /* The frame owns its actual pool reference through this callback.
         * A self-call must close admission and report BUSY, never self-join or
         * turn ordinary deferred retirement into a language failure. */
        assert(xl6TaskPoolTryQuiesceV1(*(XL6_TaskPool**)data)==XL6_MODULE_PREPARE_BUSY && !xrtGetError());
    }
    assert(SetEvent(producer_entered));
    assert(WaitForSingleObject(producer_release,5000)==WAIT_OBJECT_0);
    return XL6_TASK_SUCCESS;
}
static void blocked_drop(void* data,void* context) { (void)data; (void)context; }
static bool blocked_trace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static DWORD WINAPI destroy_worker(void* unused) { (void)unused; xlDestroyModule(module); return 0; }
static void capture_future(xvalue* value)
{
    assert(value && !alias && module->activeCalls == 1 && xrtValueIsFuture(value));
    alias = xrtValueClone(value); assert(alias && !xrtGetError());
}
static void dropped_future(int64_t id)
{
    assert(!atomic_load(&deleted));
    if(id == 42) ++external_drops;
    else { assert(id >= 7 && id <= 9); ++internal_drops; }
}
static DWORD WINAPI probe_boundary(void* unused)
{
    xrtownershipscope freeze = {0}; ULONGLONG deadline = GetTickCount64() + 5000; (void)unused;
    xlApiLock();
    assert(module->preparationRefs == 1 && module->retirementRunning && xl__ownershipDriverRunning);
    assert(module->state && !module->ownershipPlan && !module->ownershipCleared);
    xlApiUnlock();
    while(!xrtOwnershipFreezeTryBegin(&freeze)) { assert(GetTickCount64() < deadline); xrtThreadYield(); }
    assert(xrtOwnershipScopeEnd(&freeze)); return 0;
}
static XL6_ModulePrepareResult prepare(void)
{
    HANDLE worker; XL6_ModulePrepareResult result;
    assert(!xrtGetError() && module->preparationRefs == 1 && module->retirementRunning &&
        xl__ownershipDriverRunning && !internal_drops && !external_drops);
    worker = CreateThread(NULL, 0, probe_boundary, NULL, 0, NULL); assert(worker);
    assert(WaitForSingleObject(worker, 6000) == WAIT_OBJECT_0 && CloseHandle(worker)); ++preparation_probes;
    collect(); assert(!atomic_load(&deleted)); /* Competing/reentrant driver stays excluded. */
    ++prepares;
    if(prepares==1 && choice==6)result=XL6_MODULE_PREPARE_BUSY;
    else if(prepares==1 && choice==7)result=XL6_MODULE_PREPARE_FAILED; /* No diagnostic allocation. */
    else if(prepares==1 && choice==8) { xrtSetErrorKind(XERR_STATE); result=XL6_MODULE_PREPARE_FAILED; }
    else result=original_preparation->PREPARE_FIELD();
    if(result==XL6_MODULE_PREPARE_READY)++preparation_ready;
    else if(result==XL6_MODULE_PREPARE_BUSY)++preparation_busy;
    else { assert(result==XL6_MODULE_PREPARE_FAILED); ++preparation_failed; }
    return result;
}
static XL6_ModulePrepareResult forbidden_prepare(void) { ++forbidden_calls; abort(); }
static const Preparation* altered_get(void) { return choice == 13 ? NULL : &preparation; }
static void* altered_lookup(TCCState* state, const char* name)
{
    if(!strcmp(name,PREPARE_SYMBOL))
        return choice == 15 ? NULL : (void*)altered_get;
    return lookup(state,name);
}
static HANDLE mutation_entered, mutation_release;
static DWORD WINAPI contending_mutation(void* unused)
{
    xrtownershipscope scope={0}; (void)unused;
    assert(xrtOwnershipMutationBegin(&scope) && SetEvent(mutation_entered));
    assert(WaitForSingleObject(mutation_release,5000)==WAIT_OBJECT_0);
    assert(xrtOwnershipScopeEnd(&scope)); return 0;
}
static void transact(void)
{
    typedef void (*Bind)(const FutureEscapeHooks*); typedef int64_t (*Run)(int64_t); typedef int64_t (*Settle)(void);
    Bind bind; Run run; Settle settle; PreparationGet get;
    FutureEscapeHooks hooks = {capture_future, dropped_future};
    XL6_CodeLease* compatibility_guard = NULL; xrtownershipscope mutation = {0};
    xmemdebugsnapshot before, after; xerror* prior = NULL;
    xrtClearError(); xrtMemDebugSnapshot(&before);
    memset(&context,0,sizeof(context)); alias=NULL; internal_drops=external_drops=prepares=0;
    preparation_ready=preparation_busy=preparation_failed=0;
    atomic_store(&deleted,0); atomic_store(&cleanups,0); load_artifact();
    void* symbol=lookup(compiled_state,"future_escape_bind"); memcpy(&bind,&symbol,sizeof(bind));
    symbol=lookup(compiled_state,"future_escape_run"); memcpy(&run,&symbol,sizeof(run));
    symbol=lookup(compiled_state,"future_prepare_settle"); memcpy(&settle,&symbol,sizeof(settle));
    symbol=lookup(compiled_state,PREPARE_SYMBOL); memcpy(&get,&symbol,sizeof(get));
    assert(bind && run && settle && get); bind(&hooks); original_preparation=get();
    assert(original_preparation && original_preparation->size == sizeof(preparation));
    preparation=*original_preparation; preparation.PREPARE_FIELD=prepare;
    if(choice >= 9 && choice <= 13) {
        preparation.PREPARE_FIELD=forbidden_prepare;
        if(choice==9)--preparation.size;
        if(choice==10)++preparation.abiTag;
        if(choice==11)preparation.graph=NULL;
        if(choice==12)preparation.PREPARE_FIELD=NULL;
    }
    xl__tcc.tcc_get_symbol=altered_lookup; xl__tcc.tcc_delete=delete_state;
    xlApiLock(); module=xlCreateModuleFromState(&context,compiled_state); xlApiUnlock(); assert(module);
    xl__tcc.tcc_get_symbol=lookup;
    assert(!!module->retirementContract == !(choice>=9 && choice<=13));
    assert(!!module->retirementPreparation == (PREPARE_VERSION==2&&!(choice>=9 && choice<=13)&&choice!=15));
    assert(!!module->retirementPreflight == (PREPARE_VERSION==3&&!(choice>=9 && choice<=13)&&choice!=15));
    assert(xlActivateModule(&context,module,NULL));
    xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
    assert(settle()==1 && run(choice < 6 ? choice : 0)==42 && !xrtGetError());
    if(alias) {
        assert(xrtFutureWaitFor(xrtValueGetFuture(alias),5000000)==XWAIT_OK);
        if(choice==4) { xvalue* extracted=xrtValueRetain(*(xvalue**)xrtFutureValue(xrtValueGetFuture(alias)));
            assert(extracted); xrtValueRelease(alias); alias=extracted; }
    }
    xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
    if(choice>=16) {
        XL6_TaskPool* (*pool_get)(void); xfuture* pending; HANDLE destroyer=NULL; bool returned=true;
        symbol=lookup(compiled_state,"module_prepare_pool"); memcpy(&pool_get,&symbol,sizeof(pool_get)); assert(pool_get);
        XL6_TaskPool* pool=pool_get(); assert(pool);
        producer_entered=CreateEventA(NULL,TRUE,FALSE,NULL); producer_release=CreateEventA(NULL,TRUE,FALSE,NULL);
        assert(producer_entered && producer_release);
        void* frame=xl6TaskRuntimeV1Get()->Allocate(&module->codeOwnerGraph.base,sizeof(pool),blocked_run,blocked_drop,blocked_trace); assert(frame);
        *(XL6_TaskPool**)frame=pool;
        pending=xl6TaskSubmitPayloadV1(pool,frame,NULL,XL6_FUTURE_PAYLOAD_VALUE,false); assert(pending);
        assert(WaitForSingleObject(producer_entered,5000)==WAIT_OBJECT_0);
        if(choice==16) {
            destroyer=CreateThread(NULL,0,destroy_worker,NULL,0,NULL); assert(destroyer);
            returned=WaitForSingleObject(destroyer,1000)==WAIT_OBJECT_0;
        }
        if(returned){
            assert(!atomic_load(&deleted)&&!module->retirementFailed&&!internal_drops);
            if(PREPARE_VERSION==2)assert(!module->retirementPrepared&&xl6TaskRuntimeV1Get()->PoolIsClosing(pool));
            else if(choice==16)assert(!xl6TaskRuntimeV1Get()->PoolIsClosing(pool));
        }
        assert(SetEvent(producer_release));
        if(destroyer)assert(WaitForSingleObject(destroyer,5000)==WAIT_OBJECT_0 && CloseHandle(destroyer));
        assert(xrtFutureWaitFor(pending,5000000)==XWAIT_OK); xrtFutureDestroy(pending);
        assert(CloseHandle(producer_entered) && CloseHandle(producer_release));
        producer_entered=producer_release=NULL;
        finish();
        if(!returned)fprintf(stderr,"Nonblocking drain counterexample: Destroy waited for a producer requiring the caller's signal\n");
        assert(returned);
        goto balanced;
    }
    /* No Join/Quiesce: real parked workers and an open pool are now traceable.
     * Neither successful inspection nor V3 preflight is permission to drain. */
    { xrtownershipscope freeze={0}; xrtownershipresult shape={0}; xrtownershipref root={module,&xlModuleOwnershipOps};
      ULONGLONG deadline=GetTickCount64()+5000;
      for(;;) {
          xlApiLock();
          if(xrtOwnershipFreezeTryBegin(&freeze)){
              bool inspected=xrtOwnershipInspect(&root,1,NULL,0,&shape);
              assert(xrtOwnershipScopeEnd(&freeze));xlApiUnlock();
              if(inspected)break;
              xrtClearError();
          }else xlApiUnlock();
          assert(GetTickCount64()<deadline);xrtThreadYield();
      }
      assert(shape.NodeCount&&shape.ExternalRootCount); }
    xrtClearError();
    if((choice>=9 && choice<=13) || choice==15)
        assert(xl6CodeLeaseAcquire(&module->codeOwnerGraph.base,&compatibility_guard)==XL6_CODE_LEASE_ACQUIRED);
    if(choice==8) { xrtSetErrorKind(XERR_ARGUMENT); prior=xrtErrorRef(xrtGetError()); }
    if(choice==14) assert(xrtOwnershipMutationBegin(&mutation));
    if(choice>=6 && choice<=8) {
        /* Retirement admission may legitimately defer before calling Prepare.
         * Make that competing mutation deterministic, retaining every original
         * assertion about the FIRST actual BUSY/FAILED preparation outcome. */
        mutation_entered=CreateEventA(NULL,TRUE,FALSE,NULL); mutation_release=CreateEventA(NULL,TRUE,FALSE,NULL);
        assert(mutation_entered && mutation_release);
        HANDLE contender=CreateThread(NULL,0,contending_mutation,NULL,0,NULL); assert(contender);
        assert(WaitForSingleObject(mutation_entered,5000)==WAIT_OBJECT_0);
        xlDestroyModule(module);
        assert(!prepares && !atomic_load(&deleted) && !module->retirementPrepared &&
            !module->retirementFailed && !module->preparationRefs && !internal_drops);
        assert(SetEvent(mutation_release) && WaitForSingleObject(contender,5000)==WAIT_OBJECT_0);
        assert(CloseHandle(contender) && CloseHandle(mutation_entered) && CloseHandle(mutation_release));
        ULONGLONG deadline=GetTickCount64()+5000;
        while(!prepares) {
            assert(!atomic_load(&deleted) && !module->retirementPrepared && !module->retirementFailed &&
                !internal_drops && GetTickCount64()<deadline);
            collect(); if(!prepares)xrtThreadYield();
        }
    } else xlDestroyModule(module);
    if(choice==14) {
        assert(!prepares && !atomic_load(&deleted) && !module->preparationRefs);
        assert(xrtOwnershipScopeEnd(&mutation));
    }
    if(choice>=6 && choice<=8) {
        assert(prepares==1 && !module->retirementPrepared && !atomic_load(&deleted) && !internal_drops);
        assert(module->retirementFailed == (choice!=6));
        if(choice==7)assert(!module->retirementError);
        if(choice==8)assert(module->retirementError && xrtGetError()==prior && xrtErrorKind(module->retirementError)==XERR_STATE);
    }
    if(compatibility_guard) {
        XL_ScriptUnitCheckedProc unit;
        collect(); assert(!atomic_load(&deleted) && !prepares && !module->ownershipPlan && !forbidden_calls);
        symbol=lookup(compiled_state,"xl_script_module_unit_checked"); memcpy(&unit,&symbol,sizeof(unit));
        XL6_CodeScope scope={0}; xl6CodeScopeEnter(&scope,compatibility_guard); assert(unit && unit());
        xl6CodeScopeLeave(&scope); xl6CodeLeaseRelease(compatibility_guard);
    } else if(alias) {
        ULONGLONG deadline=GetTickCount64()+5000;
        while(!module->retirementPrepared) { collect(); assert(!atomic_load(&deleted) && GetTickCount64()<deadline); xrtThreadYield(); }
        collect(); assert(!atomic_load(&deleted) && !internal_drops && !external_drops && module->retirementPrepared);
        assert(module->preparationRefs==0 && preparation_ready==1 && !preparation_failed);
        xrtValueRelease(alias); alias=NULL;
    }
    finish();
balanced:
    if(prior) { assert(xrtGetError()==prior); xrtClearError(); xrtErrorFree(prior); }
    assert(internal_drops==3 && external_drops==(choice==5 ? 1u : 0u) && !xrtGetError() && !forbidden_calls);
    if(!(choice>=9 && choice<=13) && choice!=15) {
        assert(preparation_ready==1 && preparation_failed==(choice==7 || choice==8 ? 1u : 0u));
        assert(prepares==preparation_ready+preparation_busy+preparation_failed);
        if(choice==6 || (choice==16&&PREPARE_VERSION==2))assert(preparation_busy>=1);
    } else assert(!prepares && !preparation_ready);
    xrtFree(context.lastError); xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
}
int main(int argc,char** argv)
{
    assert(argc==2); choice=(unsigned)strtoul(argv[1],NULL,10); assert(choice<18);
    expected_cleanups=0; assert(xrtMemDebugEnable(true)); provider_init();
    for(unsigned round=0;round<25;++round)transact();
    if(PREPARE_VERSION==2)printf("Module prepare mode=%u: 25 real TCC lifetimes, 75 internal finalizers, %u unlocked/unfrozen probes; automatic resource drain, aliases and zero live delta\n",choice,preparation_probes);
    else printf("Module preflight V3 mode=%u: 25 real TCC lifetimes, 75 internal finalizers, %u unlocked/unfrozen probes; graph-owned pool, aliases and zero live delta\n",choice,preparation_probes);
    return 0;
}
