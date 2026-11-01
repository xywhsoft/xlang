#include "foreign_view_native_fixture.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_native_callback.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "foreign_native_userdata_module.h"
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef struct NativePayload { Native native;XL6_NativeCallback* callback; } NativePayload;
static NativePayload* activeNative;
static const XL6_NativeCallbackRuntimeV1* api;
static bool thunkReturned,requireReturn;
static unsigned adapterReferences=1,bodyReferences=1,bodyCalls;
static int adapter_acquire(void* unused){(void)unused;++adapterReferences;return 1;}
static void adapter_release(void* unused)
{(void)unused;assert(adapterReferences>1);if(requireReturn)assert(thunkReturned);--adapterReferences;}
static int body_acquire(void* unused){(void)unused;++bodyReferences;return 1;}
static void body_release(void* unused){(void)unused;assert(bodyReferences>1);--bodyReferences;}
static const XL6_CodeOwner adapterOwner={sizeof(adapterOwner),NULL,adapter_acquire,adapter_release};
static const XL6_CodeOwner bodyOwner={sizeof(bodyOwner),NULL,body_acquire,body_release};

static void slot_set(NativePayload* native,XL6_NativeCallback* record)
{
    if(record)assert(api->HoldNative(record));
    xrtownershipscope mutation={0};assert(xrtOwnershipMutationBegin(&mutation));
    XL6_NativeCallback* old=native->callback;native->callback=record;
    assert(xrtOwnershipScopeEnd(&mutation));assert(api->DropNative(old));
}
static bool callback_destroy(void* raw)
{
    NativePayload* native=raw;
    if(!native->native.depth && !native->native.failures)slot_set(native,NULL);
    return destroy_native(raw);
}
static bool callback_end(void* raw)
{
    NativeScope* scope=raw;NativePayload* native=(NativePayload*)scope->root;
    if(native->native.closing && native->native.depth==1)slot_set(native,NULL);
    return scope_end(raw);
}
static bool callback_trace(const void* raw,xrtownershipvisitor visit,void* user)
{
    const NativePayload* native=raw;
    return !native->callback || visit(api->Ownership(native->callback),user);
}
static const XL6_ForeignViewDescriptorV1 callbackDescriptor={
    sizeof(callbackDescriptor),XL6_FOREIGN_VIEW_V1_TAG,"native.test.root",
    sizeof(Watch),_Alignof(Watch),watch,unwatch,callback_destroy,
    sizeof(NativeScope),_Alignof(NativeScope),scope_begin,callback_end,callback_trace};
static XL6_ForeignView* context(NativePayload** output)
{
    NativePayload* native=xrtCalloc(1,sizeof(*native));assert(native);native->native.root=&native->native;
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(native,&callbackDescriptor,NULL);assert(view);*output=native;return view;
}
enum {READ,REMOVE,NEST};
typedef struct Environment {XL6_ForeignView* capture;int mode;} Environment;
static const XL6_CaptureSlotV2 environmentSlots[]={
    {{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Environment,capture),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get},
    {{XL6_CAPTURE_TRIVIAL,offsetof(Environment,mode),sizeof(int)},NULL}};
static const XL6_CaptureLayoutV2 environmentLayout={sizeof(environmentLayout),sizeof(Environment),2,environmentSlots};
static int invoke_native(NativePayload*);
static int body(void* raw,Native* native)
{
    const Environment* environment=raw;assert(xl6ForeignCallbackActiveV1() && native->depth>=2);++bodyCalls;
    if(environment->mode==REMOVE)slot_set((NativePayload*)native,NULL);
    if(environment->mode==NEST && native->depth==2)assert(invoke_native((NativePayload*)native)==42);
    return 42;
}
static XL6_Closure* closure(XL6_ForeignView* captured,int mode,const XL6_CodeOwner* code)
{
    Environment* environment=xrtCalloc(1,sizeof(*environment));assert(environment);environment->mode=mode;
    if(captured){environment->capture=xl6ForeignViewRetainV1(captured);assert(environment->capture);}
    XL6_Closure* value=xl6ClosureRuntimeV4Get()->Create(code,(void(*)(void))body,NULL,
        environment,sizeof(*environment),&environmentLayout,NULL,NULL,0);assert(value);return value;
}
/* Same ordering required of a generated typed C thunk. The native publisher's
 * independent hold spans this entire function, including code after Leave. */
static int thunk(XL6_NativeCallback* record)
{
    XL6_NativeCallbackScopeV1 scope={0};NativeScope native={0};
    assert(api->Enter(record,123,&scope,&native,sizeof(native)));
    int answer=((int(*)(void*,Native*))scope.closure.closure->invoke)(scope.closure.closure->environment,scope.owner.raw);
    xl6_error_result result=api->Leave(&scope,(xl6_error_result){true,NULL});assert(result.ok && !result.error);
    assert(adapterReferences>1 || !requireReturn);thunkReturned=true;return answer;
}
static int invoke_native(NativePayload* native)
{
    NativeScope operation={0};assert(scope_begin(native,&operation));
    XL6_NativeCallback* record=native->callback;assert(record && api->HoldNative(record));
    int answer=thunk(record);assert(api->DropNative(record));assert(callback_end(&operation));
    assert(xl6ForeignViewDrainV1());return answer;
}
static void replacement(void)
{
    XL6_ForeignView* root=context(&activeNative);
    XL6_Closure* function=closure(NULL,REMOVE,&bodyOwner);
    XL6_NativeCallback* record=api->Create(&adapterOwner,function,root,123);assert(record);
    xl6ClosureRuntimeV1Get()->Release(function);slot_set(activeNative,record);assert(api->Release(record));
    assert(adapterReferences==2 && bodyReferences==2);thunkReturned=false;requireReturn=true;
    assert(invoke_native(activeNative)==42 && thunkReturned);
    requireReturn=false;assert(!activeNative->callback && adapterReferences==1 && bodyReferences==1);
    assert(xl6ForeignViewReleaseV1(root));activeNative=NULL;
}
static void nested_and_allocations(void)
{
    NativePayload* native;XL6_ForeignView* root=context(&native);XL6_Closure* function=closure(NULL,NEST,NULL);
    XL6_NativeCallback* record=api->Create(NULL,function,root,123);assert(record);
    xl6ClosureRuntimeV1Get()->Release(function);slot_set(native,record);assert(api->Release(record));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);unsigned count=bodyCalls;
    for(unsigned i=0;i<100;++i)assert(invoke_native(native)==42);
    xrtMemDebugSnapshot(&after);
    assert(bodyCalls-count==200 && before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    assert(xl6ForeignViewReleaseV1(root));
}
static void native_binding(void)
{
    NativePayload* native;XL6_ForeignView* root=context(&native);XL6_Closure* function=closure(NULL,READ,NULL);
    XL6_NativeCallback* record=api->Create(NULL,function,root,123);assert(record);
    xl6ClosureRuntimeV1Get()->Release(function);slot_set(native,record);unsigned before=nativeDestroyed;
    /* Keeping native userdata alive does not create a language self-capture. */
    assert(xl6ForeignViewReleaseV1(root) && nativeDestroyed==before+1);
    XL6_NativeCallbackScopeV1 scope={0};NativeScope storage={0};unsigned calls=bodyCalls;
    assert(!api->Enter(record,123,&scope,&storage,sizeof(storage)) && bodyCalls==calls);xrtClearError();
    assert(!scope.self && !scope.record && !xl6ForeignCallbackActiveV1());assert(api->Release(record));
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* user,void* catalog)
{(void)catalog;return xl6ForeignViewDiscoverV1(NULL,visit,user);}
static void cycle(void)
{
    NativePayload* native;XL6_ForeignView* root=context(&native);XL6_Closure* function=closure(root,READ,NULL);
    XL6_NativeCallback* record=api->Create(NULL,function,root,123);assert(record);
    xl6ClosureRuntimeV1Get()->Release(function);slot_set(native,record);assert(api->Release(record));assert(xl6ForeignViewReleaseV1(root));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainNativeCallbackV1Get(),xl6OwnershipDomainClosureV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(root),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
    XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanPrepare(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY);
    step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanFinalizeV2(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY && xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)>0);xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1());
}
static void errors(void)
{
    NativePayload* native;XL6_ForeignView* root=context(&native);XL6_Closure* function=closure(NULL,READ,NULL);
    XL6_NativeCallback* record=api->Create(NULL,function,root,123);assert(record);xl6ClosureRuntimeV1Get()->Release(function);
    XL6_NativeCallbackScopeV1 scope={0};NativeScope storage={0};
    assert(!api->Enter(record,124,&scope,&storage,sizeof(storage)));xrtClearError();
    assert(api->Enter(record,123,&scope,&storage,sizeof(storage)));
    assert(!api->Enter(record,123,&scope,&storage,sizeof(storage)));xrtClearError();
    native->native.failEnd=true;
    xl6_error_result result=api->Leave(&scope,(xl6_error_result){false,NULL});assert(!result.ok && !result.error);
    assert(api->Enter(record,123,&scope,&storage,sizeof(storage)));
    xerror* error=xrtErrorCreate(XERR_VALUE,"test",0,"primary");assert(error);
    result=api->Leave(&scope,(xl6_error_result){false,error});assert(!result.ok && result.error==error);xrtErrorFree(result.error);
    assert(api->Release(record) && xl6ForeignViewReleaseV1(root));
    function=closure(NULL,READ,NULL);record=api->Create(NULL,function,NULL,123);assert(record);
    xl6ClosureRuntimeV1Get()->Release(function);
    assert(!api->Enter(record,123,&scope,&storage,sizeof(storage)) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);
    xrtClearError();assert(api->Enter(record,123,&scope,NULL,0));
    result=api->Leave(&scope,(xl6_error_result){true,NULL});assert(result.ok && !result.error && api->Release(record));
}
static const unsigned failingLayout=1;
static bool failing_validate(const void* layout,const void* environment,size_t bytes)
{return layout==&failingLayout && environment && bytes==1;}
static bool failing_trace(const void* layout,const void* environment,size_t bytes,XL6_ModuleOwnershipVisit visit,void* user)
{(void)visit;(void)user;return failing_validate(layout,environment,bytes);}
static bool failing_drop(const void* layout,void* environment,size_t bytes)
{assert(failing_validate(layout,environment,bytes));xrtFree(environment);return false;}
static const XL6_ClosureEnvironmentOpsV1 failingOps={sizeof(failingOps),failing_validate,failing_trace,failing_drop};
static void cleanup_failure(void)
{
    for(unsigned native=0;native<2;++native){
        void* environment=xrtCalloc(1,1);assert(environment);
        XL6_Closure* function=xl6ClosureCreateEnvironmentV1(NULL,(void(*)(void))body,NULL,
            environment,1,&failingLayout,&failingOps,NULL,NULL,0);assert(function);
        XL6_NativeCallback* record=api->Create(NULL,function,NULL,123);assert(record);
        xl6ClosureRuntimeV1Get()->Release(function);
        xl6_error_scope errors={0};assert(xl6ErrorScopeBegin(&errors));
        bool ok=native?api->DropNative(record):api->Release(record);assert(!ok && !xrtGetError());
        if(!native)xl6ErrorScopeCaptureFailure(&errors);
        xrtSetErrorKind(XERR_RANGE);xl6ErrorScopeCaptureFailure(&errors);
        xl6_error_result result=xl6ErrorScopeEnd(&errors);assert(!result.ok && !result.error);
        assert(!xl6ForeignCallbackActiveV1());
    }
}
static int32 worker_drop(void* raw)
{assert(api->DropNative(raw));assert(!xl6ForeignCallbackActiveV1());return 0;}
static void affinity(void)
{
    NativePayload* native;XL6_ForeignView* root=context(&native);
    XL6_Closure* function=closure(root,READ,NULL);
    XL6_NativeCallback* record=api->Create(NULL,function,root,123);assert(record);
    xl6ClosureRuntimeV1Get()->Release(function);assert(xl6ForeignViewReleaseV1(root));
    unsigned before=nativeDestroyed;xthread* thread=xrtThreadCreate(worker_drop,record,0);assert(thread);
    assert(xrtThreadWait(thread)==XWAIT_OK);xrtThreadDestroy(thread);
    assert(nativeDestroyed==before && xl6ForeignViewPendingV1(xrtThreadCurrentId())==1);
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1);
}
static void allocation(void)
{
    NativePayload* native;XL6_ForeignView* root=context(&native);XL6_Closure* function=closure(NULL,READ,NULL);
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);unsigned failures=0;
    for(int64_t point=0;point<8;++point){
        assert(xrtMemDebugFailAfter(point));XL6_NativeCallback* record=api->Create(&adapterOwner,function,root,123);
        assert(xrtMemDebugFailAfter(-1));if(!record)++failures;xrtClearError();assert(api->Release(record));
        xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && adapterReferences==1);
        if(record)break;
    }
    assert(failures==2);xl6ClosureRuntimeV1Get()->Release(function);assert(xl6ForeignViewReleaseV1(root));
}
typedef struct LoadedModule {void* library;unsigned references;const bool* returned;} LoadedModule;
static void* module_load(const char* path)
{
#if defined(_WIN32)
    return LoadLibraryA(path);
#else
    return dlopen(path,RTLD_NOW|RTLD_LOCAL);
#endif
}
static void module_symbol(void* library,const char* name,void* result,size_t bytes)
{
#if defined(_WIN32)
    FARPROC address=GetProcAddress(library,name);
#else
    void* address=dlsym(library,name);
#endif
    assert(address && bytes==sizeof(address));memcpy(result,&address,bytes);
}
static int module_acquire(void* data)
{LoadedModule* owner=data;assert(owner->library && owner->references);++owner->references;return 1;}
static void module_release(void* data)
{
    LoadedModule* owner=data;assert(owner->references);
    if(--owner->references)return;
    assert(*owner->returned);
#if defined(_WIN32)
    assert(FreeLibrary(owner->library));
#else
    assert(!dlclose(owner->library));
#endif
    owner->library=NULL;
}
static void modules(const char* bodyPath,const char* adapterPath)
{
    static const XL6_ClosureCaptureSlot slot={XL6_CLOSURE_CAPTURE_TRIVIAL,0,sizeof(UserdataModuleHost*)};
    static const XL6_ClosureEnvironmentV1 layout={sizeof(layout),sizeof(UserdataModuleHost*),1,&slot};
    for(unsigned i=0;i<50;++i){
        UserdataModuleHost host={.api=api};
        LoadedModule bodyModule={module_load(bodyPath),1,&host.bodyReturned};
        LoadedModule adapterModule={module_load(adapterPath),1,&host.adapterReturned};
        assert(bodyModule.library && adapterModule.library);
        UserdataModuleBody body;UserdataModuleAdapter adapter;
        module_symbol(bodyModule.library,"moduleBody",&body,sizeof(body));
        module_symbol(adapterModule.library,"moduleAdapter",&adapter,sizeof(adapter));
        XL6_CodeOwner bodyCode={sizeof(bodyCode),&bodyModule,module_acquire,module_release};
        XL6_CodeOwner adapterCode={sizeof(adapterCode),&adapterModule,module_acquire,module_release};
        UserdataModuleHost** environment=xrtMalloc(sizeof(*environment));assert(environment);*environment=&host;
        XL6_Closure* function=xl6ClosureRuntimeV2Get()->Create(&bodyCode,(void(*)(void))body,NULL,environment,&layout,NULL,NULL,0);assert(function);
        host.slot=api->Create(&adapterCode,function,NULL,789);assert(host.slot);
        xl6ClosureRuntimeV1Get()->Release(function);
        /* Transfer the final two OS handles exclusively to resident leases. */
        module_release(&bodyModule);module_release(&adapterModule);
        assert(bodyModule.references==1 && adapterModule.references==1);
        XL6_NativeCallback* record=host.slot;assert(api->HoldNative(record));
        assert(adapter(&host,record)==73 && host.calls==1 && !host.slot);
        assert(host.bodyReturned && host.adapterReturned && bodyModule.library && adapterModule.library);
        assert(api->DropNative(record));
        assert(!bodyModule.library && !adapterModule.library && !bodyModule.references && !adapterModule.references);
    }
    puts("50 independent body/adapter shared-library lifetimes released after the typed thunk returned");
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));api=xl6NativeCallbackRuntimeV1Get();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct{const char* name;void(*run)(void);} cases[]={
        {"replacement",replacement},{"nested",nested_and_allocations},{"binding",native_binding},
        {"cycle",cycle},{"errors",errors},{"cleanup",cleanup_failure},{"affinity",affinity},{"allocation",allocation}};
    if(argc==4 && !strcmp(argv[1],"modules")){
        modules(argv[2],argv[3]);argc=2;argv[1]="modules-complete";
    }
    bool found=argc==1 || (argc==2 && !strcmp(argv[1],"modules-complete"));
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2 && adapterReferences==1 && bodyReferences==1 && !xl6ForeignCallbackActiveV1());
    assert(xl6ForeignViewDrainV1());xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && !xl6ForeignViewPendingV1(0));
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("native userdata: distinct code owners, return-tail hold, nested typed calls, binding pins, cycle and checked rollback passed");return 0;
}
