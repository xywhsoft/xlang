#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_foreign_view_value.h"
#include "src/runtime/xl6_ownership_domains.h"
enum {IDENTITY=1234,OTHER_IDENTITY=1235};
static xstrview key(const char* text){return (xstrview){text,strlen(text)};}
static unsigned foreignCallbacks;
static void foreign_drop(void* raw,void* user){(void)raw;(void)user;++foreignCallbacks;}
static bool foreign_trace(const xvalue* value,xrtownershipvisitor visit,void* user)
{(void)value;(void)visit;(void)user;++foreignCallbacks;return true;}
static const xvaluehandleops foreignOperations={NULL,foreign_drop,NULL,NULL};
static void values(void)
{
    Native* native=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(native,&rootDescriptor,NULL);assert(root);
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xvalue* boxed=xl6ForeignViewBoxV1(root,IDENTITY);assert(boxed);xrtMemDebugSnapshot(&after);
    assert(after.AllocCount-before.AllocCount==1 && after.ReallocCount==before.ReallocCount);
    xvalue* cloned=xrtValueClone(boxed);assert(cloned && xrtValueTypeId(cloned)==IDENTITY && xrtValueEqual(boxed,cloned));
    XL6_ForeignView* copy=NULL;
    assert(xl6ForeignViewUnboxV1(cloned,IDENTITY,false,&copy) && copy==root && xl6ForeignViewReleaseV1(copy));copy=NULL;
    assert(!xl6ForeignViewUnboxV1(cloned,OTHER_IDENTITY,false,&copy) && !copy);xrtClearError();
    copy=xl6ForeignViewRetainV1(root);assert(copy);
    assert(!xl6ForeignViewUnboxV1(boxed,IDENTITY,false,&copy) && copy==root);xrtClearError();assert(xl6ForeignViewReleaseV1(copy));copy=NULL;
    xvalue* wrong=xrtValuePointer(root);assert(wrong && !xl6ForeignViewUnboxV1(wrong,IDENTITY,false,&copy) && !copy);xrtClearError();xrtValueRelease(wrong);
    void* foreign=root;wrong=xrtValueHandleTake(&foreign,&foreignOperations,NULL);assert(wrong && !foreign);
    assert(xrtValueTypeIdBind(wrong,IDENTITY) && xrtValueHandleOwnershipBindPhased(wrong,foreign_trace));
    assert(!xl6ForeignViewUnboxV1(wrong,IDENTITY,false,&copy) && !copy && !foreignCallbacks);
    xrtClearError();xrtValueRelease(wrong);assert(foreignCallbacks==1);foreignCallbacks=0;
    xerror* prior=xrtErrorCreate(XERR_RANGE,"test",0,"prior");assert(prior);xrtSetErrorTake(prior);
    xvalue* protected=xl6ForeignViewBoxV1(root,IDENTITY);assert(protected && xrtGetError()==prior);xrtClearError();xrtValueRelease(protected);
    xvalue* null=xl6ForeignViewBoxV1(NULL,IDENTITY);assert(null && xrtValueType(null)==XVALUE_NULL);
    assert(xl6ForeignViewUnboxV1(null,IDENTITY,true,&copy) && !copy);
    assert(!xl6ForeignViewUnboxV1(null,IDENTITY,false,&copy) && !copy);xrtClearError();xrtValueRelease(null);
    assert(xl6ForeignViewCloseV1(root));
    /* Copying a closed value is valid; accessing its native payload is not. */
    assert(xl6ForeignViewUnboxV1(boxed,IDENTITY,false,&copy) && copy==root);
    XL6_ForeignViewCallV1 call={0};NativeScope storage={0};assert(!xl6ForeignViewCallBeginV1(copy,&call,&storage,sizeof(storage)));xrtClearError();
    assert(xl6ForeignViewReleaseV1(copy));xrtValueRelease(boxed);xrtValueRelease(cloned);assert(xl6ForeignViewReleaseV1(root) && xl6ForeignViewDrainV1());
}
static xvalue* class_with_view(XL6_ForeignView* view)
{
    const XL6_ClassRuntimeV1* api=xl6ClassRuntimeV1Get();xvalue* object=api->Create(NULL,NULL);assert(object);
    xvalue* value=xl6ForeignViewBoxV1(view,IDENTITY);assert(value);
    assert(xrtValueObjectSetTake(object,key("view"),&value) && !value);
    object=api->CommitTake(object);assert(object);return object;
}
static void class_field(void)
{
    Native* native=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(native,&rootDescriptor,NULL);assert(root);
    xvalue* object=class_with_view(root);xvalue* clone=xrtValueClone(object);assert(clone);
    xvalue* marker=xrtValueInt(7);assert(marker && xrtValueObjectSetTake(clone,key("marker"),&marker) && !marker);
    xvalue* deep=xrtValueDeepClone(object);assert(deep);
    assert(xl6ForeignViewReleaseV1(root));xrtValueRelease(object);
    XL6_ForeignView* view=NULL;assert(xl6ForeignViewUnboxV1(xrtValueObjectGet(clone,key("view")),IDENTITY,false,&view));
    XL6_ForeignViewCallV1 call={0};NativeScope scope={0};
    assert(xl6ForeignViewCallBeginV1(view,&call,&scope,sizeof(scope)) && call.raw==native && xl6ForeignViewCallEndV1(&call));
    assert(xl6ForeignViewReleaseV1(view));xrtValueRelease(clone);
    view=NULL;assert(xl6ForeignViewUnboxV1(xrtValueObjectGet(deep,key("view")),IDENTITY,false,&view));assert(xl6ForeignViewReleaseV1(view));
    unsigned destroyed=nativeDestroyed;xrtValueRelease(deep);
    assert(nativeDestroyed==destroyed && xl6ForeignViewPendingV1(0));
    assert(xl6ForeignViewDrainV1() && nativeDestroyed==destroyed+1);
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* user,void* catalog)
{(void)catalog;return xl6ForeignViewDiscoverV1(NULL,visit,user);}
static void cycle(void)
{
    Native* native=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(native,&rootDescriptor,NULL);assert(root);
    native->object=class_with_view(root);assert(xl6ForeignViewReleaseV1(root));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),xl6OwnershipDomainForeignViewValueV1Get(),xl6OwnershipDomainClassV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(root),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanFinalizeV2(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryDestroy(plan)>0);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1());
}
static void cleanup(void)
{
    Native* native=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(native,&rootDescriptor,NULL);assert(root);
    native->failures=1;native->failWithoutDiagnostic=true;
    xvalue* value=xl6ForeignViewBoxV1(root,IDENTITY);assert(value && xl6ForeignViewReleaseV1(root));
    xrtValueRelease(value);assert(xl6ForeignViewPendingV1(0));
    assert(!xl6ForeignViewDrainV1() && !xrtGetError());assert(xl6ForeignViewDrainV1());
}
static void allocation(void)
{
    Native* native=native_create();XL6_ForeignView* root=xl6ForeignViewCreateRootV1(native,&rootDescriptor,NULL);assert(root);
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));assert(!xl6ForeignViewBoxV1(root,IDENTITY));assert(xrtMemDebugFailAfter(-1));xrtClearError();
    xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    assert(xl6ForeignViewReleaseV1(root));
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct{const char* name;void(*run)(void);} cases[]={{"values",values},{"class-field",class_field},{"cycle",cycle},{"cleanup",cleanup},{"allocation",allocation}};
    bool found=argc==1;for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2 && xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0));xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("foreign view value: exact storage projection, clone/COW/deep class fields, closed values, graph cycle and rollback passed");return 0;
}
