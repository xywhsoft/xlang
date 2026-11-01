#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/api/xl_foreign_error.h"

static XL6_ForeignView* finalizedView;
static Native* finalizedNative;
static unsigned finalized;
static bool class_finalizer(xvalue* value,void* unused)
{
    (void)value;(void)unused;assert(xl6ForeignCallbackActiveV1());
    assert(xl6ForeignViewCloseV1(finalizedView));
    assert(!finalizedNative->closing);++finalized;return false; /* authoritative NULL diagnostic */
}
static void finalizer(void)
{
    finalizedNative=native_create();
    finalizedView=xl6ForeignViewCreateRootV1(finalizedNative,&rootDescriptor,&codeOwner);assert(finalizedView);
    xvalue* object=xl6ClassRuntimeV2Get()->Create(&codeOwner,class_finalizer);assert(object);
    object=xl6ClassRuntimeV2Get()->CommitTake(object);assert(object);
    unsigned before=nativeDestroyed;xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));
    xrtValueRelease(object);xl6_error_result result=xl6ErrorScopeEnd(&scope);
    assert(!result.ok && !result.error && finalized==1 && nativeDestroyed==before);
    assert(!xl6ForeignCallbackActiveV1() && xl6ForeignViewPendingV1(0));
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1);
    assert(xl6ForeignViewReleaseV1(finalizedView));finalizedView=NULL;finalizedNative=NULL;
}
typedef struct Context { XL6_ForeignView* view; } Context;
static bool trace_context(const void* data,XL6_ModuleOwnershipVisit visit,void* user)
{ return visit(xl6ForeignViewOwnershipV1(((const Context*)data)->view),user); }
static void drop_context(void* data)
{
    Context* context=data;assert(xl6ForeignCallbackActiveV1());
    assert(xl6ForeignViewReleaseV1(context->view));xrtFree(context);
}
static void no_op_finalize(xvalue* value,void* data)
{ (void)value;(void)data;assert(xl6ForeignCallbackActiveV1()); }
static void context_release(void)
{
    Context* context=xrtCalloc(1,sizeof(*context));assert(context);
    Native* raw=native_create();context->view=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner);assert(context->view);
    xvalue* object=xrtValueObjectLifo();assert(object);
    XL6_CodeLease* lease=NULL;assert(xl6CodeLeaseAcquire(&codeOwner,&lease)==XL6_CODE_LEASE_ACQUIRED);
    assert(xl6CodeFinalizerBind(object,lease,no_op_finalize,context,trace_context,drop_context));xl6CodeLeaseRelease(lease);
    unsigned before=nativeDestroyed;xrtValueRelease(object);
    assert(nativeDestroyed==before && codeReferences==2 && xl6ForeignViewPendingV1(0));
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1 && codeReferences==1);
}
static const XL6_CaptureSlotV2 environmentSlot={
    {XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Context,view),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get};
static const XL6_CaptureLayoutV2 environmentLayout={sizeof(environmentLayout),sizeof(Context),1,&environmentSlot};
static void typed_body(void) { }
static XL6_Closure* make_closure(void)
{
    Context* context=xrtCalloc(1,sizeof(*context));assert(context);
    Native* raw=native_create();context->view=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner);assert(context->view);
    XL6_Closure* closure=xl6ClosureRuntimeV4Get()->Create(NULL,typed_body,NULL,
        context,sizeof(*context),&environmentLayout,NULL,NULL,0);assert(closure);return closure;
}
static bool entry(void* data,const xrtcallframe* frame,xrtcallresult* result)
{
    (void)frame;(void)result;assert(xl6ForeignCallbackActiveV1());
    XL6_Closure* closure=data;Context* context=closure->environment;
    unsigned before=nativeDestroyed;assert(xl6ForeignViewCloseV1(context->view));
    assert(nativeDestroyed==before);return true;
}
static void box_invoke(void)
{
    XL6_Closure* closure=make_closure();
    xvalue* box=xl6ClosureRuntimeV1Get()->Box(&codeOwner,closure,entry,123);assert(box);
    xl6ClosureRuntimeV1Get()->Release(closure);
    xrtcallresult result=XRT_CALL_RESULT_INIT;unsigned before=nativeDestroyed;
    assert(xrtValueInvoke(box,NULL,&result));xrtCallResultUnit(&result);
    assert(nativeDestroyed==before && xl6ForeignViewPendingV1(0) && !xl6ForeignCallbackActiveV1());
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1);
    xrtValueRelease(box);assert(codeReferences==1);
}
static void box_drop(void)
{
    XL6_Closure* closure=make_closure();
    xvalue* box=xl6ClosureRuntimeV1Get()->Box(&codeOwner,closure,entry,123);assert(box);
    xl6ClosureRuntimeV1Get()->Release(closure);unsigned before=nativeDestroyed;
    xrtValueRelease(box);
    assert(nativeDestroyed==before && codeReferences==2 && xl6ForeignViewPendingV1(0));
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==before+1 && codeReferences==1);
    /* The other two public native cleanup callbacks have the same boundary. */
    closure=make_closure();before=nativeDestroyed;
    xl6ClosureRuntimeV1Get()->ReleaseEnvironment(closure);
    assert(nativeDestroyed==before && xl6ForeignViewPendingV1(0));assert(xl6ForeignViewDrainV1());
    closure=make_closure();before=nativeDestroyed;
    XL6_Closure** payload=xrtMalloc(sizeof(*payload));assert(payload);*payload=closure;
    xl6ClosureRuntimeV1Get()->PayloadDrop(payload,NULL);
    assert(nativeDestroyed==before && xl6ForeignViewPendingV1(0));assert(xl6ForeignViewDrainV1());
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct{const char* name;void(*run)(void);} cases[]={
        {"finalizer",finalizer},{"context-release",context_release},{"box-invoke",box_invoke},{"box-drop",box_drop}};
    bool found=argc==1;
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2 && !xl6ForeignCallbackActiveV1());assert(xl6ForeignViewDrainV1());xrtClearError();xrtMemDebugSnapshot(&after);
    assert(codeReferences==1 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("resident native callback boundaries: class finalizers, context release, boxed invoke/drop and closure cleanup passed");return 0;
}
