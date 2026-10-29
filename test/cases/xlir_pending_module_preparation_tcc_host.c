#define JOINT_TCC_ASYNC
#define JOINT_TCC_SOURCE_FILE "test/cases/xlir_pending_module_preparation_artifact.c"
#define main unused_pending_lifetime_main
#include "xlir_one_shot_tcc_host.c"
#undef main

typedef struct PendingModuleHooks {
    void (*observe)(int64_t,const char*);
    void (*capture)(xvalue*);
} PendingModuleHooks;
static _Atomic(xvalue*) pending_alias;
static unsigned choice;
static _Atomic unsigned events,notifications,destructors,boundary_probes,deferrals;
static _Atomic unsigned graph_notifications,graph_finalizers,inline_notifications,queued_notifications;
static DWORD WINAPI pending_boundary(void* unused)
{
    xrtownershipscope freeze={0};MEMORY_BASIC_INFORMATION memory;(void)unused;
    xlApiLock();assert(!atomic_load(&deleted)&&module->state==compiled_state&&module->codeLeases);
    xlApiUnlock();
    ULONGLONG deadline=GetTickCount64()+5000;
    while(!xrtOwnershipFreezeTryBegin(&freeze)) {
        assert(!xrtGetError()&&GetTickCount64()<deadline);SwitchToThread();
    }
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(VirtualQuery(relocated_entry,&memory,sizeof(memory))==sizeof(memory)&&memory.State==MEM_COMMIT);
    collect();assert(!atomic_load(&deleted));return 0;
}
static void pending_observe(int64_t event,const char* text)
{
    assert(!atomic_load(&deleted)&&text&&!strcmp(text,"pending owner"));
    if(event!=3&&choice<4){
        /* Prove the real host's V4 graph preparation, not eager module Unit or
         * an explicit host PromiseClose, delivers these callbacks. A finally
         * notification may run on the ready queue after Prepare returned BUSY;
         * graph phase and code ownership must remain valid on either thread. */
        xlApiLock();
        bool certified=module->ownershipPlan&&module->ownershipPhase==6&&
            module->retirementPrepared&&!module->ownershipCleared&&module->codeLeases;
        bool inlineDelivery=xl__ownershipDriverRunning&&xl__ownershipDriverThread==xrtThreadCurrentId();
        int driver=xl__ownershipDriverRunning,phase=module->ownershipPhase,
            prepared=module->retirementPrepared,cleared=module->ownershipCleared;
        void* plan=module->ownershipPlan;
        xlApiUnlock();
        if(!certified)
            fprintf(stderr,"pending callback mode=%u event=%lld driver=%d plan=%p phase=%d prepared=%d cleared=%d\n",
                choice,(long long)event,driver,plan,phase,prepared,cleared);
        assert(certified);++graph_notifications;
        if(inlineDelivery)++inline_notifications;else ++queued_notifications;
    }
    if(event==3&&(choice==0||choice==1||choice==3)){
        xlApiLock();bool certified=xl__ownershipDriverRunning&&module->ownershipPlan&&
            module->ownershipPhase==1&&!module->ownershipCleared;xlApiUnlock();
        assert(certified);++graph_finalizers;
    }
    unsigned before=atomic_load(&events);
    assert(event==(before==0?1:before==1?2:3));
    assert(atomic_compare_exchange_strong(&events,&before,before*10+(unsigned)event));
    HANDLE worker=CreateThread(NULL,0,pending_boundary,NULL,0,NULL);assert(worker);
    assert(WaitForSingleObject(worker,5000)==WAIT_OBJECT_0&&CloseHandle(worker));++boundary_probes;
    if(event==3)++destructors;else ++notifications;
}
static void pending_capture(xvalue* value)
{
    assert(!pending_alias&&!atomic_load(&deleted));
    if(choice==2){assert(events==1&&module->destroyRequested&&!module->ownershipCleared);}
    else assert(!events&&module->activeCalls==1&&xrtValueIsFuture(value));
    pending_alias=xrtValueRetain(value);assert(pending_alias);
}
static void pending_delete(TCCState* state)
{
    assert(events==123);delete_state(state);
}
static void pending_transaction(void)
{
    typedef void (*Bind)(const PendingModuleHooks*);
    typedef void (*Run)(int64_t);
    typedef xpromise* (*SourceRef)(void);
    Bind bind;Run run;SourceRef source_ref;xpromise* producer=NULL;int64_t number=79;
    PendingModuleHooks hooks={pending_observe,pending_capture};xmemdebugsnapshot before,after;
    xrtClearError();xrtMemDebugSnapshot(&before);memset(&context,0,sizeof(context));
    pending_alias=NULL;events=0;atomic_store(&deleted,0);atomic_store(&cleanups,0);
    load_artifact();void* symbol=lookup(compiled_state,"pending_module_bind");memcpy(&bind,&symbol,sizeof(bind));
    symbol=lookup(compiled_state,"pending_module_keep");memcpy(&run,&symbol,sizeof(run));
    symbol=lookup(compiled_state,"pending_module_source_ref");memcpy(&source_ref,&symbol,sizeof(source_ref));
    assert(bind&&run&&source_ref);bind(&hooks);
    xl__tcc.tcc_get_symbol=lookup;xl__tcc.tcc_delete=pending_delete;
    xlApiLock();module=xlCreateModuleFromState(&context,compiled_state);xlApiUnlock();assert(module);
    assert(module->retirementContract&&module->retirementStep&&!module->retirementPreparation&&xlActivateModule(&context,module,NULL));
    xlApiLock();assert(xlModuleBeginCallUnlocked(module));xlApiUnlock();
    run(choice);assert(!events&&!xrtGetError());if(choice>=3){producer=source_ref();assert(producer);}
    xlApiLock();xlModuleEndCallUnlocked(module);xlApiUnlock();
    xlDestroyModule(module); /* Consume the real public module handle once. */
    if(choice==1||choice>=3){
        collect();assert(!atomic_load(&deleted)&&!events&&!module->retirementFailed);++deferrals;
        if(choice==1){xrtValueRelease(pending_alias);pending_alias=NULL;}
        else if(choice==3){xrtPromiseDestroy(producer);producer=NULL;}
        else{
            if(choice==4)assert(xrtPromiseResolve(producer,&number));
            else if(choice==5){xerror* error=xrtErrorCreate(XERR_STATE,"pending",91,"native source failed");
                assert(error&&xrtPromiseReject(producer,error));xrtErrorFree(error);}
            else if(choice==6)assert(xrtPromiseCancel(producer));
            else assert(choice==7&&xrtPromiseClose(producer));
        }
    }
    if(choice==2){
        xdeadline deadline=xrtDeadlineAfter(5000000);
        while(events!=12){collect();assert(!atomic_load(&deleted)&&!xrtDeadlineExpired(deadline));xrtThreadYield();}
        collect();assert(pending_alias&&!atomic_load(&deleted)&&!module->retirementFailed);++deferrals;
        xrtValueRelease(pending_alias);pending_alias=NULL;
    }
    finish();assert(events==123&&atomic_load(&deleted)==1&&!compiled_state);
    if(choice>=4){
        xfutureresult result={0};assert(pending_alias&&xrtFutureResult(xrtValueGetFuture(pending_alias),&result));
        assert(result.State==(choice==4?XFUTURE_RESOLVED:choice==5?XFUTURE_FAILED:choice==6?XFUTURE_CANCELLED:XFUTURE_CLOSED));
        if(choice==4)assert(result.Value&&*(int64_t*)result.Value==79);
        if(choice==5)assert(result.Error&&xrtErrorKind(result.Error)==XERR_STATE);
        xrtValueRelease(pending_alias);pending_alias=NULL;xrtPromiseDestroy(producer);producer=NULL;
    }
    assert(!pending_alias&&!producer);xrtClearError();xrtFree(context.lastError);xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
}
int main(int argc,char** argv)
{
    assert(argc==2);choice=(unsigned)strtoul(argv[1],NULL,10);assert(choice<8);
    expected_cleanups=0;assert(xrtMemDebugEnable(true));provider_init();
    for(unsigned n=0;n<25;++n)pending_transaction();
    assert(notifications==50&&destructors==25&&boundary_probes==75&&deferrals==(choice?25u:0u));
    assert(graph_notifications==(choice<4?50u:0u)&&graph_finalizers==((choice==0||choice==1||choice==3)?25u:0u));
    assert(inline_notifications+queued_notifications==graph_notifications);
    printf("Pending module mode=%u: 25 real TCC lifetimes, 50 ordered finally callbacks before 25 class finalizers, 75 unlocked/unfrozen code-residency probes, %u real-reference deferrals, MEM_FREE and zero live delta; no guard leases or fake roots\n",choice,deferrals);
    printf("Pending module phases mode=%u: %u certified preparation notifications, %u graph finalizers\n",choice,graph_notifications,graph_finalizers);return 0;
}
