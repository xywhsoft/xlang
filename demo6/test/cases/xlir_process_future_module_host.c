/* Source API admission/retirement with a genuinely unmapped code provider.
 * A single host-resident XRT backs all values, native children and dispatchers.
 * No public Future<class> C ABI or Process.wait native-thread join is assumed. */
#ifndef XL_STATIC_EMBED
#define XL_STATIC_EMBED
#endif
#define XL_ENABLE_EXE_COMPILER 0
#include "src/api/xl_api.c"
#include "src/api/xl_api_path.c"
#include "src/common/xl6_console.c"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_process_live.h"
#include <assert.h>
#include <stdatomic.h>

extern size_t testProcessRetireObserved(void);
extern int64_t xl6ProcessStatusCodeV1(void*);
extern int64_t xl6ProcessStatusKindV1(void*);
typedef struct ProcessLifetimeHooks { void (*finally)(void); } ProcessLifetimeHooks;
static XL_Context context;
static XL_Module* module;
static TCCState* state;
static atomic_uint cleanups, deleted;
static void cleanup(void)
{ assert(!atomic_load(&deleted)); atomic_fetch_add(&cleanups,1); }
#ifdef PROCESS_TCC_PROVIDER
#include "test/support/process_module_tcc_provider.h"
#else
#if defined(_WIN32)
static HMODULE artifact;
#else
static void* artifact;
#endif

static void load_artifact(void)
{
#if defined(_WIN32)
    artifact=LoadLibraryA(PROCESS_DLL_PATH);
#else
    artifact=dlopen(PROCESS_DLL_PATH,RTLD_NOW|RTLD_LOCAL);
    if(!artifact) fprintf(stderr,"dlopen: %s\n",dlerror());
#endif
    assert(artifact); state=(TCCState*)artifact;
}
static void* lookup(TCCState* input,const char* name)
{
    assert(input==state);
#if defined(_WIN32)
    return (void*)GetProcAddress(artifact,name);
#else
    return dlsym(artifact,name);
#endif
}
static void delete_state(TCCState* input)
{
    assert(input==state && !module->codeLeases && !module->activeCalls);
    assert(atomic_load(&cleanups)==2 && !atomic_load(&deleted));
#if defined(_WIN32)
    assert(FreeLibrary(artifact));
    assert(GetModuleHandleA(PROCESS_DLL_NAME)==NULL);
#else
    assert(dlclose(artifact)==0);
    void* still=dlopen(PROCESS_DLL_PATH,RTLD_NOW|RTLD_NOLOAD);
    assert(!still);
#endif
    atomic_fetch_add(&deleted,1);
}
#endif
static void collect(void)
{ xlApiLock(); xlCollectRetiringModulesUnlocked(); xlApiUnlock(); }
static void await_cleanups(unsigned count)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(atomic_load(&cleanups)<count) {
        assert(!xrtDeadlineExpired(deadline)); xrtThreadYield();
    }
}
static void finish(void)
{
    xdeadline deadline=xrtDeadlineAfter(5000000);
    while(!atomic_load(&deleted)) {
        collect();
        if(xrtDeadlineExpired(deadline)) {
            xlApiLock();
            for(XL_Module* item=xl__retiringModules;item;item=item->retirementNext) {
                if(item!=module) continue;
                fprintf(stderr,"retirement stuck: leases=%zu active=%u phase=%d cleared=%d prepared=%d "
                    "plan=%p failed=%d error=%s\n",item->codeLeases,item->activeCalls,item->ownershipPhase,
                    item->ownershipCleared,item->retirementPrepared,(void*)item->ownershipPlan,
                    item->retirementFailed,item->retirementError ? xrtErrorMessage(item->retirementError) : "none");
            }
            xlApiUnlock(); assert(0);
        }
        xrtThreadYield();
    }
    /* The provider delete hook publishes physical unmapping while still under
     * the API mutex. Wait for that transaction to finish before reading its
     * intrusive list or allocator ledger; the atomic witness alone is not a
     * module-shell/runtime teardown barrier. */
    xlApiLock(); assert(xl__retiringModules==NULL); xlApiUnlock();
}
static void check_status(xvalue* value)
{
    xvalue* field=xrtValueObjectGet(value,(xstrview){"handle",6}); void* native=NULL;
    assert(field && xrtValueGetHandle(field,&native,NULL,NULL) && native);
    assert(xl6ProcessStatusKindV1(native)==1 && xl6ProcessStatusCodeV1(native)==0);
    assert(!xrtGetError());
}
static bool report_live(const xmemdebugallocation* allocation,void* unused)
{
    (void)unused;
    fprintf(stderr,"live %p size=%zu at %s:%u\n",allocation->Address,allocation->Size,
        allocation->File ? allocation->File : "unknown",allocation->Line);
    return true;
}
static unsigned rejectedTraceCalls, droppedHandles;
static bool rejected_trace(const xvalue* value,xrtownershipvisitor visitor,void* data)
{ (void)value; (void)visitor; (void)data; ++rejectedTraceCalls; return true; }
static void unknown_drop(void* value,void* data)
{ (void)data; ++droppedHandles; xrtFree(value); }
static void native_admission_contract(void)
{
    xmemdebugsnapshot before,after;
    const XL6_NativeHandlePolicyV1* policy=xl6ProcessStatusHandlePolicyV1Get();
    static const xvaluehandleops unknown={NULL,unknown_drop,NULL,NULL};
    assert(policy->size==sizeof(*policy) && policy->abiTag==XL6_NATIVE_HANDLE_POLICY_V1_TAG);
    xrtMemDebugSnapshot(&before);
    /* Exact Ops without the declared Trace, exact Trace with unknown Ops and
     * an entirely unknown callback all refuse BEFORE running that callback. */
    for(unsigned variant=0;variant<3;++variant) {
        void* payload=xrtCalloc(1,sizeof(xprocessstatus)); assert(payload);
        xvalue* handle=xrtValueHandleTake(&payload,variant==0 ? policy->Ops : &unknown,NULL);
        assert(handle && !payload);
        assert(xrtValueHandleOwnershipBindPhased(handle,variant==1 ? policy->Trace : rejected_trace));
        xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(xlModuleResolveOwnership(xrtValueOwnership(handle),NULL)==NULL);
        assert(xrtOwnershipScopeEnd(&freeze)); xrtValueRelease(handle);
    }
    assert(rejectedTraceCalls==0 && droppedHandles==2);
    void* payload=xrtCalloc(1,sizeof(xprocessstatus)); assert(payload);
    XL6_NativeHandlePolicyV1 invalid=*policy; invalid.abiTag=0;
    assert(!xl6NativeHandleTakeV1(&payload,&invalid) && payload);
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    policy->Ops->Drop(payload,NULL);
    unsigned failures=0;
    for(uint64 point=0;point<64;++point) {
        payload=xrtCalloc(1,sizeof(xprocessstatus)); assert(payload);
        xl6_error_scope boundary={0}; assert(xl6ErrorScopeBegin(&boundary));
        assert(xrtMemDebugFailAfter(point));
        xvalue* handle=xl6NativeHandleTakeV1(&payload,policy);
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(!payload); xl6_error_result outcome=xl6ErrorScopeEnd(&boundary);
        if(hit) {
            ++failures; assert(!handle && !outcome.ok);
            assert(!outcome.error || xrtErrorKind(outcome.error)==XERR_MEMORY);
        } else {
            assert(handle && outcome.ok && !outcome.error);
            xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
            assert(xlModuleResolveOwnership(xrtValueOwnership(handle),NULL)!=NULL);
            assert(xrtOwnershipScopeEnd(&freeze)); xrtValueRelease(handle);
        }
        xrtErrorFree(outcome.error); xrtClearError();
        if(!hit) break;
        assert(point<63);
    }
    assert(failures);
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
    printf("Resident native handle: 3 pre-trace refusals, invalid contract keeps slot, %u OOM prefix failures + first no-hit, exact ledger\n",failures);
}
int main(int argc,char** argv)
{
    assert(argc==2); unsigned mode=(unsigned)strtoul(argv[1],NULL,10); assert(mode<2);
    assert(xrtMemDebugEnable(true));
    native_admission_contract();
#ifdef PROCESS_TCC_PROVIDER
    provider_init();
#endif
    for(unsigned round=0;round<25;++round) {
        typedef void (*Bind)(const ProcessLifetimeHooks*);
        typedef xvalue* (*Create)(void);
        typedef xfuture* (*Start)(xvalue*);
        typedef XL6_FuturePayloadDrop (*Drop)(void);
        typedef const xrtownershipops* (*Identity)(void);
        Bind bind; Create create; Start start; Drop drop; Identity identity;
        xmemdebugsnapshot before,after; xfutureresult result={0}; xvalue* retained=NULL;
        atomic_store(&cleanups,0); atomic_store(&deleted,0); memset(&context,0,sizeof(context));
        xrtClearError(); xrtMemDebugSnapshot(&before); load_artifact();
        void* symbol=lookup(state,"process_lifetime_bind");
        _Static_assert(sizeof(symbol)==sizeof(bind),"provider function pointer ABI");
        memcpy(&bind,&symbol,sizeof(bind));
        symbol=lookup(state,"process_lifetime_start"); memcpy(&start,&symbol,sizeof(start));
        symbol=lookup(state,"process_lifetime_create"); memcpy(&create,&symbol,sizeof(create));
        symbol=lookup(state,"process_lifetime_drop_address"); memcpy(&drop,&symbol,sizeof(drop));
        symbol=lookup(state,"process_lifetime_callable_identity"); memcpy(&identity,&symbol,sizeof(identity));
        assert(bind && create && start && drop && identity);
        assert(drop()==xl6PayloadDropGet(XL6_FUTURE_PAYLOAD_XVALUE));
        assert(identity()==xrtCallableOwnership(NULL).Ops);
        ProcessLifetimeHooks hooks={cleanup}; bind(&hooks);
        xl__tcc.tcc_get_symbol=lookup; xl__tcc.tcc_delete=delete_state;
        xlApiLock(); module=xlCreateModuleFromState(&context,state); xlApiUnlock(); assert(module);
        assert(xlActivateModule(&context,module,NULL));
        xlApiLock(); assert(xlModuleBeginCallUnlocked(module)); xlApiUnlock();
        xvalue* process=create(); assert(process && !xrtGetError());
        xfuture* first=start(process); xfuture* second=start(process);
        assert(first && second && !xrtGetError());
        xrtValueRelease(process);
        xlApiLock(); xlModuleEndCallUnlocked(module); xlApiUnlock();
        assert(xrtFutureState(first)==XFUTURE_PENDING && xrtFutureState(second)==XFUTURE_PENDING);
        assert(!xlDeactivateModule(&context,module)); xrtClearError();
        xlDestroyModule(module); /* Handle consumed exactly once. */
        collect(); assert(!atomic_load(&deleted) && !atomic_load(&cleanups));
        if(mode) {
            assert(xrtFutureCancel(first));
            assert(xrtFutureWaitFor(first,5000000)==XWAIT_OK && xrtFutureResult(first,&result));
            assert(result.State==XFUTURE_CANCELLED);
            await_cleanups(1); collect();
            assert(!atomic_load(&deleted) && xrtFutureState(second)==XFUTURE_PENDING);
        }
        /* Real stdin EOF and private native waiter join; no kill/fake result. */
        assert(testProcessRetireObserved()==1);
        assert(xrtFutureWaitFor(second,5000000)==XWAIT_OK && xrtFutureResult(second,&result));
        assert(result.State==XFUTURE_RESOLVED && result.Value);
        retained=xrtValueRetain(*(xvalue**)result.Value); assert(retained); check_status(retained);
        if(!mode) {
            assert(xrtFutureWaitFor(first,5000000)==XWAIT_OK && xrtFutureResult(first,&result));
            assert(result.State==XFUTURE_RESOLVED && result.Value);
            assert(*(xvalue**)result.Value!=retained); check_status(*(xvalue**)result.Value);
        }
        xrtFutureDestroy(first); xrtFutureDestroy(second); await_cleanups(2);
        collect(); assert(!atomic_load(&deleted));
        /* The escaped class still pins its generated type/lifecycle. Its native
         * field alone has resident Ops+Trace: after releasing the class, it can
         * outlive actual provider unmapping and retire safely in resident code. */
        check_status(retained);
        xvalue* field=xrtValueRetain(xrtValueObjectGet(retained,(xstrview){"handle",6})); assert(field);
        xrtValueRelease(retained); finish();
        void* native=NULL; const xvaluehandleops* ops=NULL; void* data=(void*)1;
        assert(xrtValueGetHandle(field,&native,&ops,&data) && native && !data);
        assert(ops==xl6ProcessStatusHandlePolicyV1Get()->Ops && xl6ProcessStatusCodeV1(native)==0);
        xrtValueRelease(field);
        xrtFree(context.lastError); xrtClearError(); xrtMemDebugSnapshot(&after);
        if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
            xrtMemDebugVisitLive(report_live,NULL);
        assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
        assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
        assert(before.UseAfterFreeCount==after.UseAfterFreeCount);
#ifdef PROCESS_TCC_PROVIDER
        provider_check_balanced();
#endif
    }
    printf("Process Future provider %s mode=%u: 25 real unloads; pending/cancel isolation, independent statuses, escaped class pins, finally twice, native waiter and heap balanced\n",
#ifdef PROCESS_TCC_PROVIDER
        "TCC/MEM_FREE",
#elif defined(_WIN32)
        "DLL/FreeLibrary",
#else
        "ELF/dlclose",
#endif
        mode);
    return 0;
}
