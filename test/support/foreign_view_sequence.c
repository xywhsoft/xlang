#include "foreign_view_native_fixture.h"
#include "src/runtime/xl6_foreign_view_sequence.h"
#include "src/runtime/xl6_foreign_view_value_storage.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_stack_erased.h"
#include "src/runtime/xl6_ownership_domains.h"
enum{OWNER=4321,ELEMENT=4322,BOXED=4323,OTHER=4324};
#define LAYOUT OWNER,ELEMENT,XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1,sizeof(XL6_ForeignView*),xl6ForeignViewStorageTypeV1()
static XL6_ForeignView* make(void)
{XL6_ForeignView* view=xl6ForeignViewCreateRootV1(native_create(),&rootDescriptor,NULL);assert(view);return view;}
static xvalue* create(bool nullable)
{xvalue* value=xl6ForeignViewSequenceCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,BOXED,nullable);assert(value);return value;}
static void typed(void)
{
    assert(xl6ForeignViewStorageTypeV1()->Size==sizeof(void*) &&
        xl6ForeignViewStorageTypeV1()->Id==xrtTypeId((xstrview){"xl6.foreign-view-slot.v1",sizeof("xl6.foreign-view-slot.v1")-1}));
    XL6_ForeignView* view=make();xvalue* array=create(false);assert(xl6ArrayReserveV1(array,LAYOUT,32));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(0));
    for(unsigned i=0;i<200;++i){
        assert(xl6ArrayPushV1(array,LAYOUT,&view));XL6_ForeignView* out=NULL;
        assert(xl6ArrayReadV1(array,LAYOUT,0,&out) && out==view && xl6ForeignViewReleaseV1(out));out=NULL;
        assert(xl6ArrayTakeV1(array,LAYOUT,0,&out) && out==view && xl6ForeignViewReleaseV1(out));
    }
    assert(xrtMemDebugFailAfter(-1));xrtMemDebugSnapshot(&after);
    assert(after.AllocCount==before.AllocCount && after.ReallocCount==before.ReallocCount);
    assert(xl6ArrayPushV1(array,LAYOUT,&view));xvalue* clone=xl6ArrayCloneV1(array,LAYOUT);assert(clone);
    assert(xl6ArrayEqualsV1(array,clone,LAYOUT));
    XL6_ForeignView* occupied=xl6ForeignViewRetainV1(view);assert(occupied);
    assert(!xl6ArrayReadV1(array,LAYOUT,0,&occupied) && occupied==view);xrtClearError();assert(xl6ForeignViewReleaseV1(occupied));
    assert(xl6ForeignViewCloseV1(view));
    XL6_ForeignView* out=NULL;assert(xl6ArrayReadV1(clone,LAYOUT,0,&out) && out==view && xl6ForeignViewReleaseV1(out));
    assert(xl6ArrayEqualsV1(array,clone,LAYOUT));
    xrtValueRelease(array);xrtValueRelease(clone);assert(xl6ForeignViewReleaseV1(view) && xl6ForeignViewDrainV1());
}
static void nullable(void)
{
    XL6_ForeignView* none=NULL;xvalue* required=create(false);xvalue* optional=create(true);
    assert(!xl6ArrayPushV1(required,LAYOUT,&none));xrtClearError();
    assert(!xl6ArrayResizeV1(required,LAYOUT,2));xrtClearError();assert(xl6ArraySizeV1(required,LAYOUT,false)==0);
    assert(xl6ArrayResizeV1(optional,LAYOUT,2) && xl6ArrayPushV1(optional,LAYOUT,&none));
    assert(xl6ArrayReadV1(optional,LAYOUT,0,&none) && !none);
    xvalue* boxed=xl6ArrayErasedReadV1(optional,1);assert(boxed && xrtValueType(boxed)==XVALUE_NULL);xrtValueRelease(boxed);
    assert(!xl6ArrayErasedPushV1(required,xrtValueNull()));xrtClearError();
    assert(xl6ArrayErasedPushV1(optional,xrtValueNull()));
    assert(!xl6ArrayAppendV1(required,optional,LAYOUT));xrtClearError();
    xrtValueRelease(required);xrtValueRelease(optional);
}
static unsigned foreignCalls;
static void foreignDrop(void* raw,void* user){(void)raw;(void)user;++foreignCalls;}
static bool foreignTrace(const xvalue* value,xrtownershipvisitor visit,void* user)
{(void)value;(void)visit;(void)user;++foreignCalls;return true;}
static void erased(void)
{
    XL6_ForeignView* view=make();xvalue* array=create(false);assert(xl6ArrayPushV1(array,LAYOUT,&view));
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xvalue* alias=xl6ArrayEraseV1(array,LAYOUT,xl6ForeignViewSequenceConverterV1());assert(alias==array);
    xrtMemDebugSnapshot(&after);assert(before.AllocCount==after.AllocCount && before.ReallocCount==after.ReallocCount);
    xvalue* item=xl6ArrayErasedReadV1(alias,0);assert(item && xrtValueTypeId(item)==BOXED);
    xrtMemDebugSnapshot(&after);assert(after.AllocCount-before.AllocCount==1);
    assert(xl6ArrayErasedPushV1(alias,item));
    XL6_ForeignView* out=NULL;assert(xl6ForeignViewUnboxV1(item,BOXED,false,&out) && out==view && xl6ForeignViewReleaseV1(out));
    xvalue* wrong=xl6ForeignViewBoxV1(view,OTHER);assert(wrong && !xl6ArrayErasedPushV1(alias,wrong));xrtClearError();xrtValueRelease(wrong);
    static const xvaluehandleops ops={NULL,foreignDrop,NULL,NULL};void* raw=view;
    wrong=xrtValueHandleTake(&raw,&ops,NULL);assert(wrong && !raw && xrtValueTypeIdBind(wrong,BOXED));
    assert(xrtValueHandleOwnershipBindPhased(wrong,foreignTrace));
    xerror* prior=xrtErrorCreate(XERR_RANGE,"test",0,"prior");assert(prior);xrtSetErrorTake(prior);
    assert(!xl6ForeignViewValueAdmitsV1(wrong,BOXED,false) && xrtGetError()==prior && !foreignCalls);xrtClearError();
    assert(!xl6ArrayErasedPushV1(alias,wrong) && !foreignCalls);xrtClearError();xrtValueRelease(wrong);assert(foreignCalls==1);
    assert(xrtMemDebugFailAfter(0));assert(!xl6ArrayErasedTakeV1(alias,0));assert(xrtMemDebugFailAfter(-1));xrtClearError();
    assert(xl6ArrayErasedSizeV1(alias)==2);
    xvalue* unbound=xl6ForeignViewStorageBoxV1(view);assert(unbound && !xrtValueTypeId(unbound));
    assert(!xl6ArrayErasedPushV1(alias,unbound));xrtClearError();xrtValueRelease(unbound);
    xvalue* clone=xl6ArrayErasedCloneV1(alias);assert(clone && xl6ArrayEqualsV1(array,clone,LAYOUT));
    xrtValueRelease(item);xrtValueRelease(clone);xrtValueRelease(alias);xrtValueRelease(array);
    assert(xl6ForeignViewReleaseV1(view) && xl6ForeignViewDrainV1());
}
static void stack(void)
{
    XL6_ForeignView* view=make();xvalue* value=xl6ForeignViewSequenceCreateV1(XL6_SEQUENCE_STACK_V1,OWNER,ELEMENT,BOXED,false);assert(value);
    assert(xl6StackReserveV1(value,LAYOUT,8) && xl6StackPushV1(value,LAYOUT,&view));
    XL6_ForeignView* out=NULL;bool found=false;
    assert(xl6StackTryTakeV2(value,LAYOUT,0,&out,false,&found) && found && out==view && xl6ForeignViewReleaseV1(out));
    xvalue* projected=xl6StackErasedReadV1(value,0,true);assert(projected && xrtValueTypeId(projected)==BOXED);
    assert(xl6StackSizeV1(value,LAYOUT,false)==0 && xl6StackErasedPushV1(value,projected));
    xrtValueRelease(projected);xrtValueRelease(value);assert(xl6ForeignViewReleaseV1(view) && xl6ForeignViewDrainV1());
}
static bool discover(XL6_ModuleOwnershipVisit visit,void* user,void* catalog)
{(void)catalog;return xl6ForeignViewDiscoverV1(NULL,visit,user);}
static void cycle(void)
{
    XL6_ForeignView* view=make();XL6_ForeignViewCallV1 call={0};NativeScope scope={0};
    assert(xl6ForeignViewCallBeginV1(view,&call,&scope,sizeof(scope)));
    Native* native=call.raw;native->object=create(false);
    for(unsigned i=0;i<8;++i)assert(xl6ArrayPushV1(native->object,LAYOUT,&view));
    assert(xl6ForeignViewCallEndV1(&call) && xl6ForeignViewReleaseV1(view));
    const XL6_OwnershipDomainV1* domains[]={xl6OwnershipDomainForeignViewV1Get(),
        xl6OwnershipDomainSequenceV1Get(),xl6OwnershipDomainSequenceForeignViewV1Get()};
    XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,3);assert(catalog);XL6_OwnershipPlan* plan=NULL;
    assert(xl6OwnershipPlanTryCreateV6(xl6ForeignViewOwnershipV1(view),xl6OwnershipCatalogProbeV1,discover,true,catalog,&plan)>0 && plan);
    assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
    XL6_OwnershipStepResult step=XL6_OWNERSHIP_STEP_BUSY;
    for(unsigned i=0;i<8 && step==XL6_OWNERSHIP_STEP_BUSY;++i)step=xl6OwnershipPlanFinalizeV2(plan);
    assert(step==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
    assert(xl6OwnershipPlanFinishV2(plan)==XL6_OWNERSHIP_STEP_READY && xl6OwnershipPlanTryDestroy(plan)>0);
    xl6OwnershipCatalogDestroyV1(catalog);assert(xl6ForeignViewDrainV1());
}
static void allocation(void)
{
    bool succeeded=false;
    for(int prefix=0;prefix<16 && !succeeded;++prefix){
        xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(prefix));
        xvalue* array=xl6ForeignViewSequenceCreateV1(XL6_SEQUENCE_ARRAY_V1,OWNER,ELEMENT,BOXED,false);
        succeeded=array!=NULL;assert(xrtMemDebugFailAfter(-1));xrtClearError();
        if(array){xrtMemDebugSnapshot(&after);printf("empty sequence: %llu allocations, %llu requested live bytes\n",
            (unsigned long long)(after.AllocCount-before.AllocCount),
            (unsigned long long)(after.LiveBytes-before.LiveBytes));}
        xrtValueRelease(array);
        xrtMemDebugSnapshot(&after);assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
    }
    assert(succeeded);
    assert(!xl6ForeignViewSequenceCreateV1(XL6_SEQUENCE_CURSOR_V1,OWNER,ELEMENT,BOXED,false));xrtClearError();
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnabled() || xrtMemDebugEnable(true));xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    const struct{const char* name;void(*run)(void);} cases[]={{"typed",typed},{"nullable",nullable},{"erased",erased},{"stack",stack},{"cycle",cycle},{"allocation",allocation}};
    bool found=argc==1;for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);++i)if(argc==1 || !strcmp(argv[1],cases[i].name)){found=true;cases[i].run();}
    assert(found && argc<=2 && xl6ForeignViewDrainV1() && !xl6ForeignViewPendingV1(0));xrtClearError();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount);
    puts("foreign view sequence: pointer slots, no-allocation typed operations, nullable, exact dynamic projection, stack, cycle and OOM rollback passed");return 0;
}
