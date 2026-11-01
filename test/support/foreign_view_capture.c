#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_capture_v3.h"
#include "src/runtime/xl6_foreign_view_capture.h"
#include "src/runtime/xl6_closure_capture_v4.h"
#include "src/runtime/xl6_cell_native.h"
#include "src/runtime/xl6_native_future_payload.h"
#include "src/runtime/xl6_ownership_domains.h"
#include "src/api/xl_foreign_error.h"

typedef struct Environment { XL6_ForeignView* view; xstrview text; } Environment;
static const XL6_CaptureSlotV2 environmentSlots[]={
    {{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Environment,view),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get},
    {{XL6_CAPTURE_STRING,offsetof(Environment,text),sizeof(xstrview)},NULL}};
static const XL6_CaptureLayoutV2 environmentLayout={sizeof(environmentLayout),sizeof(Environment),2,environmentSlots};
static unsigned visits;
static bool count_ref(xrtownershipref ref,void* context)
{ (void)context; assert(ref.Data && ref.Ops); ++visits; return true; }

static void layout_and_legacy_rejection(void)
{
    const XL6_NativeCaptureProfileV1* prefix=xl6ForeignViewCaptureProfileV1Get();
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(prefix); assert(profile);
    assert(!xl6NativeCaptureProfileValidV1(prefix));
    assert(!xl6CaptureRuntimeV2Get()->Validate(&environmentLayout,sizeof(Environment)));
    assert(!xl6CellCreateNativeV2(prefix)); xrtClearError();
    assert(!xl6NativeFuturePayloadRuntimeV1Get()->Allocate(prefix)); xrtClearError();
    Native* raw=native_create(); native_child(raw);
    XL6_ForeignView* root=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(root);
    Environment frame={0}; assert(profile->CopyFresh(&frame.view,&root));
    assert(!profile->CopyFresh(&frame.view,&root)); xrtClearError();
    assert(xl6ForeignViewReleaseV1(root));
    frame.text=(xstrview){xrtStrDupN("a\0b",3),3}; assert(frame.text.Data);
    assert(xl6CaptureRuntimeV3Get()->Validate(&environmentLayout,sizeof(frame)));
    visits=0; assert(xl6CaptureRuntimeV3Get()->Trace(&environmentLayout,&frame,sizeof(frame),count_ref,NULL) && visits==1);
    xl6_error_result cleaned=xl6CaptureRuntimeV3Get()->Cleanup(&environmentLayout,&frame,sizeof(frame),(xl6_error_result){true,NULL});
    assert(cleaned.ok && !cleaned.error && !frame.view && !frame.text.Data && codeReferences==1);
    cleaned=xl6CaptureRuntimeV3Get()->Cleanup(&environmentLayout,&frame,sizeof(frame),(xl6_error_result){true,NULL});
    assert(cleaned.ok);
}
static void sparse_frames(void)
{
    typedef struct Frame { XL6_ForeignView* a; XL6_ForeignView* b; unsigned char aLive,bLive; } Frame;
    const XL6_ActivationCaptureSlotV2 fields[]={
        {{{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Frame,a),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get},offsetof(Frame,aLive)},
        {{{XL6_CAPTURE_NATIVE_GRAPH_V3,offsetof(Frame,b),sizeof(XL6_ForeignView*)},xl6ForeignViewCaptureProfileV1Get},offsetof(Frame,bLive)}};
    const size_t order[]={1,0};
    XL6_ActivationCaptureLayoutV2 layout={sizeof(layout),sizeof(Frame),2,fields,2,order};
    Native* raw=native_create(); native_child(raw);
    Frame frame={.a=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner),.b=(XL6_ForeignView*)(uintptr_t)1,.aLive=1};
    assert(frame.a && xl6ActivationCaptureRuntimeV3Get()->Validate(&layout,sizeof(frame)));
    visits=0;
    assert(xl6ActivationCaptureRuntimeV3Get()->Trace(&layout,&frame,sizeof(frame),count_ref,NULL) && visits==1);
    frame.aLive=2;
    assert(!xl6ActivationCaptureRuntimeV3Get()->Trace(&layout,&frame,sizeof(frame),count_ref,NULL));
    xl6_error_result result=xl6ActivationCaptureRuntimeV3Get()->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL});
    assert(!result.ok && frame.a && frame.aLive==2); xrtErrorFree(result.error);
    frame.aLive=1;
    result=xl6ActivationCaptureRuntimeV3Get()->Cleanup(&layout,&frame,sizeof(frame),(xl6_error_result){true,NULL});
    assert(result.ok && !frame.a && !frame.aLive && !frame.bLive && codeReferences==1);
    const size_t repeated[]={0,0}; layout.cleanup=repeated;
    assert(!xl6ActivationCaptureRuntimeV3Get()->Validate(&layout,sizeof(frame)));
}
static const XL6_NativeCaptureProfileV1* overlappingProfile;
static const XL6_NativeCaptureProfileV1* overlapping_get(void) { return overlappingProfile; }
static void malformed_metadata(void)
{
    XL6_CaptureSlotV2 field=environmentSlots[0];
    XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(Environment),1,&field};
    field.value.kind=XL6_CAPTURE_NATIVE_LEAF_V2;
    assert(!xl6CaptureRuntimeV3Get()->Validate(&layout,sizeof(Environment)));
    field=environmentSlots[0]; field.value.offset=SIZE_MAX-3;
    assert(!xl6CaptureRuntimeV3Get()->Validate(&layout,sizeof(Environment)));
    _Alignas(max_align_t) unsigned char data[sizeof(XL6_NativeGraphCaptureProfileV1)+32];
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(xl6ForeignViewCaptureProfileV1Get());
    memcpy(data,profile,sizeof(*profile));
    unsigned char original[sizeof(data)]; memcpy(original,data,sizeof(data));
    overlappingProfile=(const XL6_NativeCaptureProfileV1*)data;
    field=environmentSlots[0]; field.native=overlapping_get; layout.bytes=sizeof(data);
    assert(xl6CaptureRuntimeV3Get()->Validate(&layout,sizeof(data)));
    assert(!xl6CaptureRuntimeV3Get()->Trace(&layout,data,sizeof(data),count_ref,NULL));
    xl6_error_result result=xl6CaptureRuntimeV3Get()->Cleanup(&layout,data,sizeof(data),(xl6_error_result){true,NULL});
    assert(!result.ok && !memcmp(data,original,sizeof(data))); xrtErrorFree(result.error);
    overlappingProfile=NULL;
}
typedef struct Marker { int id; bool fail,diagnostic; } Marker;
static int dropOrder[2]; static size_t dropped;
static bool marker_copy(void* output,const void* input) { *(Marker*)output=*(const Marker*)input; return true; }
static bool marker_trace(const void* value,XL6_ModuleOwnershipVisit visit,void* context)
{ (void)value; (void)context; return visit!=NULL; }
static bool marker_drop(void* raw,unsigned char* live)
{
    Marker* slot=raw; Marker value=*slot; *slot=(Marker){0}; if (live) *live=0;
    if (!value.id) return true;
    assert(dropped<2); dropOrder[dropped++]=value.id;
    if (value.diagnostic) xrtSetErrorKind(XERR_RANGE);
    return !value.fail;
}
static const XL6_NativeCaptureProfileV1* marker_profile(void)
{
    static const XL6_NativeGraphCaptureProfileV1 profile={
        {sizeof(profile),UINT64_C(0x5445535447524150),NULL,NULL},XL6_NATIVE_GRAPH_CAPTURE_V1_TAG,
        sizeof(Marker),_Alignof(Marker),marker_copy,marker_trace,marker_drop};
    return &profile.prefix;
}
static void checked_cleanup(void)
{
    Marker data[]={{1,true,true},{2,true,false}};
    const XL6_CaptureSlotV2 fields[]={
        {{XL6_CAPTURE_NATIVE_GRAPH_V3,0,sizeof(Marker)},marker_profile},
        {{XL6_CAPTURE_NATIVE_GRAPH_V3,sizeof(Marker),sizeof(Marker)},marker_profile}};
    const XL6_CaptureLayoutV2 layout={sizeof(layout),sizeof(data),2,fields};
    dropped=0;
    xl6_error_result result=xl6CaptureRuntimeV3Get()->Cleanup(&layout,data,sizeof(data),(xl6_error_result){true,NULL});
    assert(!result.ok && !result.error && dropped==2 && dropOrder[0]==2 && dropOrder[1]==1);
    assert(!data[0].id && !data[1].id);
    result=xl6CaptureRuntimeV3Get()->Cleanup(&layout,data,sizeof(data),(xl6_error_result){true,NULL});
    assert(result.ok && dropped==2);
    result=xl6CaptureRuntimeV3Get()->Cleanup(&layout,data,sizeof(data),(xl6_error_result){false,NULL});
    assert(!result.ok && !result.error && dropped==2);
}
static int invoke_environment(void* raw)
{
    const Environment* environment=raw; NativeScope native={0}; XL6_ForeignViewCallV1 call={0};
    assert(xl6ForeignViewCallBeginV1(environment->view,&call,&native,sizeof(native)));
    assert(call.raw && ((Native*)call.raw)->depth);
    int result=42+(int)environment->text.Size;
    assert(environment->text.Size==3 && !environment->text.Data[1]);
    assert(xl6ForeignViewCallEndV1(&call)); return result;
}
static XL6_Closure* create_capture(XL6_ForeignView* view)
{
    Environment* environment=xrtCalloc(1,sizeof(*environment)); assert(environment);
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(xl6ForeignViewCaptureProfileV1Get());
    assert(profile->CopyFresh(&environment->view,&view));
    environment->text=(xstrview){xrtStrDupN("a\0b",3),3}; assert(environment->text.Data);
    XL6_Closure* closure=xl6ClosureRuntimeV4Get()->Create(NULL,(void(*)(void))invoke_environment,NULL,
        environment,sizeof(*environment),&environmentLayout,NULL,NULL,0);
    assert(closure); return closure;
}
static void typed_closure(void)
{
    Native* raw=native_create(); native_child(raw);
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(view);
    XL6_Closure* closure=create_capture(view); assert(xl6ForeignViewReleaseV1(view));
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    XL6_ClosureScope scope={0}; xl6ClosureRuntimeV1Get()->Enter(&scope,closure);
    assert(((int(*)(void*))closure->invoke)(closure->environment)==45);
    xl6ClosureRuntimeV1Get()->Leave(&scope); xrtMemDebugSnapshot(&after);
    assert(before.AllocCount==after.AllocCount && before.FreeCount==after.FreeCount && before.ReallocCount==after.ReallocCount);
    xl6ClosureRuntimeV1Get()->Release(closure); assert(codeReferences==1);
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* context,void* catalog)
{ (void)catalog; return xl6ForeignViewDiscoverV1(NULL,visit,context); }
static void closure_cycle(void)
{
    Native* raw=native_create(); native_child(raw);
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,NULL); assert(view);
    XL6_Closure* closure=create_capture(view);
    raw->callback=xl6ClosureRuntimeV1Get()->Retain(closure);
    xl6ClosureRuntimeV1Get()->Release(closure); assert(xl6ForeignViewReleaseV1(view));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainClosureV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,2); assert(catalog);
    XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
    XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
    for (unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i) step=xl6OwnershipPlanPrepare(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY);
    step=XL6_OWNERSHIP_STEP_BUSY;
    for (unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i) step=xl6OwnershipPlanFinalizeV2(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY);
    assert(xl6OwnershipPlanTryDestroy(plan)>0); xl6OwnershipCatalogDestroyV1(catalog);
    assert(xl6ForeignViewPendingV1(0)==0);
}
static void allocation_rollback(void)
{
    Native* raw=native_create(); native_child(raw);
    XL6_ForeignView* view=xl6ForeignViewCreateRootV1(raw,&rootDescriptor,&codeOwner); assert(view);
    xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    const XL6_NativeGraphCaptureProfileV1* profile=xl6NativeGraphCaptureProfileV1Get(xl6ForeignViewCaptureProfileV1Get());
    unsigned failures=0;
    for (int64 fail=0;fail<8;++fail) {
        Environment* environment=xrtCalloc(1,sizeof(*environment)); assert(environment);
        assert(profile->CopyFresh(&environment->view,&view));
        environment->text=(xstrview){xrtStrDupN("a\0b",3),3}; assert(environment->text.Data);
        assert(xrtMemDebugFailAfter(fail));
        XL6_Closure* closure=xl6ClosureRuntimeV4Get()->Create(&codeOwner,(void(*)(void))invoke_environment,NULL,
            environment,sizeof(*environment),&environmentLayout,NULL,NULL,0);
        assert(xrtMemDebugFailAfter(-1)); xrtClearError();
        if (closure) xl6ClosureRuntimeV1Get()->Release(closure);
        else { ++failures; assert(environment->view && environment->text.Data);
            assert(xl6ClosureRuntimeV4Get()->DropEnvironment(&environmentLayout,environment,sizeof(*environment))); }
        xrtMemDebugSnapshot(&after);
        assert(codeReferences==2 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
        if (closure) break;
    }
    assert(failures==2); assert(xl6ForeignViewReleaseV1(view));
    printf("native graph closure rollback: %u failure prefixes\n",failures);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
    const struct { const char* name; void (*run)(void); } cases[]={
        {"layout",layout_and_legacy_rejection},{"sparse",sparse_frames},{"metadata",malformed_metadata},
        {"cleanup",checked_cleanup},{"invoke",typed_closure},{"cycle",closure_cycle},{"allocation",allocation_rollback}};
    bool found=argc==1;
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)
        if (argc==1 || !strcmp(argv[1],cases[i].name)) { found=true; cases[i].run(); }
    assert(found && argc<=2); xrtClearError(); xrtMemDebugSnapshot(&after);
    assert(codeReferences==1 && before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("native graph captures: explicit schema, sparse cleanup, checked failures, typed invocation and closure cycle passed");
    return 0;
}
