#define main xl6PriorUnarySnapshotMain
#define __wrap_xrtTypedArrayPush xl6UnaryWrapPush
#define __wrap_xrtSlotMapIterNext xl6UnaryWrapNext
#include "stdlib_slot_map_snapshot_host.c"
#undef main
#undef __wrap_xrtTypedArrayPush
#undef __wrap_xrtSlotMapIterNext
#include "src/runtime/xl6_slot_map_iterator_internal.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_array_native_value_admission.h"

static const XL6_ValueIteratorRuntimeV2* api;
static int keyFailure,valueFailure; static unsigned clones,keyBoxes;
static bool pairedCapture; static unsigned begins,pushes;
extern bool __real_xrtSlotMapIterBegin(const xslotmap*,xslotmapiter*);
bool __wrap_xrtSlotMapIterBegin(const xslotmap* source,xslotmapiter* iterator)
{ if(pairedCapture) ++begins; return __real_xrtSlotMapIterBegin(source,iterator); }
bool __wrap_xrtTypedArrayPush(xtypedarray* output,const void* input)
{ if(pairedCapture) ++pushes; return xl6UnaryWrapPush(output,input); }
ptr __wrap_xrtSlotMapIterNext(xslotmapiter* iterator,xslot* slot)
{ return xl6UnaryWrapNext(iterator,slot); }
static void observe(void) { begins=pushes=0; pairedCapture=true; }
extern bool __real_xrtTypeCopyValue(const xrttype*,ptr,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,ptr target,const void* source)
{
    bool ok=__real_xrtTypeCopyValue(type,target,source);
    if(ok && keyFailure && type==xl6SlotMapKeyTypeV1()) {
        int failure=keyFailure; keyFailure=0;
        if(failure>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        return false; /* Owns a partial copy: the boundary must roll it back. */
    }
    return ok;
}
extern xvalue* __real_xrtTypedArrayGetValue(const xtypedarray*,size_t,const xvalueconverter*);
xvalue* __wrap_xrtTypedArrayGetValue(const xtypedarray* array,size_t index,const xvalueconverter* converter)
{
    xvalue* result=__real_xrtTypedArrayGetValue(array,index,converter);
    if(valueFailure) {
        int failure=valueFailure; valueFailure=0; xrtValueRelease(result);
        if(failure>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        return NULL;
    }
    return result;
}
extern xtypedarray* __real_xrtTypedArrayClone(const xtypedarray*);
xtypedarray* __wrap_xrtTypedArrayClone(const xtypedarray* input)
{ ++clones; return __real_xrtTypedArrayClone(input); }
extern xvalue* __real_xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1);
xvalue* __wrap_xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1 key)
{ ++keyBoxes; return __real_xl6SlotMapKeyBoxV1(key); }

static XL6_ValueIterator* createMode(unsigned mode,const xvalue* map,const XL6_SlotMapLayoutV1* layout,
    const xvalueconverter* converter)
{
    switch(mode) {
    case 0: return xl6SlotMapCountIteratorCreateV1(map,layout);
    case 1: return xl6SlotMapKeyIteratorCreateV1(map,layout,0xCA14);
    case 2: return xl6SlotMapItemIteratorCreateV1(map,layout,converter);
    case 3: return xl6SlotMapPairIteratorCreateV1(map,layout,0xCA14,converter);
    default: assert(false); return NULL;
    }
}
static bool countEdge(xrtownershipref ref,void* data)
{ assert(ref.Data && ref.Ops); ++*(unsigned*)data; return true; }
static unsigned edges(XL6_ValueIterator* cursor)
{
    xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref root=api->Ownership(cursor); unsigned count=0;
    assert(root.Ops->Trace(root.Data,countEdge,&count)); assert(xrtOwnershipScopeEnd(&freeze)); return count;
}
static void modes(void)
{
    for(unsigned mode=0;mode<4;++mode) {
        XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
        XL6_SlotMapKeyV1 a={0},b={0},c={0}; bool changed=false,found=false;
        const xstrview text={"a\0你",5},other={"b\0好",5},replacement={"replacement",11};
        assert(xl6SlotMapInsertV1(map,&layout,&text,&a) && xl6SlotMapInsertV1(map,&layout,&other,&b));
        assert(xl6SlotMapKeyV1(map,&layout,a,true,&changed) && changed);
        assert(xl6SlotMapInsertV1(map,&layout,&text,&c) && !xl6SlotMapKeyEqualV1(a,c));
        clones=keyBoxes=0; observe(); XL6_ValueIterator* cursor=createMode(mode,map,&layout,xl6StringConverterV1());
        pairedCapture=false; assert(cursor && !clones && !keyBoxes);
        assert(begins==(mode ? 1u : 0u) && pushes==(mode==3 ? 4u : mode ? 2u : 0u));
        assert(edges(cursor)==((mode&1) ? 1u : 0u)+((mode&2) ? 1u : 0u));
        XL6_SlotMapKeyV1 key={0}; assert(!xl6ValueIteratorKeyCopyV3(cursor,&key)); xrtClearError();
        assert(xl6SlotMapSetV1(map,&layout,c,&replacement,&changed) && changed && xl6SlotMapClearV1(map,&layout));
        xrtValueRelease(map); /* Snapshots and copied keys survive source destruction. */
        assert(api->Advance(cursor,&found) && found);
        const XL6_ValueIteratorView* view=api->View(cursor); assert(view && view->ordinal==0);
        if(mode&1) assert(xl6ValueIteratorKeyCopyV3(cursor,&key) && xl6SlotMapKeyEqualV1(key,c));
        else { assert(!xl6ValueIteratorKeyCopyV3(cursor,&key)); xrtClearError(); }
        if(mode&2) {
            xstrview value={0}; assert(view->item && xrtValueGetString(view->item,&value));
            assert(value.Size==5 && !memcmp(value.Data,text.Data,5));
        } else assert(!view->item);
        assert(edges(cursor)==((mode&1) ? 1u : 0u)+((mode&2) ? 2u : 0u));
        assert(!clones && !keyBoxes); dropKey(&key);
        assert(api->Advance(cursor,&found) && found && api->View(cursor)->ordinal==1);
        if(mode&1) assert(xl6ValueIteratorKeyCopyV3(cursor,&key) && xl6SlotMapKeyEqualV1(key,b));
        assert(api->Advance(cursor,&found) && !found && api->Advance(cursor,&found) && !found);
        XL6_SlotMapKeyV1 absent={0}; assert(!xl6ValueIteratorKeyCopyV3(cursor,&absent)); xrtClearError();
        api->Destroy(cursor);
        if(mode&1) assert(xl6SlotMapKeyEqualV1(key,b)); /* An independent owner, not a cursor borrow. */
        dropKey(&key); dropKey(&a); dropKey(&b); dropKey(&c);
    }
}
static void domain(XL6_SlotMapLayoutV1 layout,unsigned rule,const void* input,const xvalueconverter* converter)
{
    xvalue* map=make(&layout,rule); assert(map); XL6_SlotMapKeyV1 original={0},key={0}; bool found=false;
    assert(xl6SlotMapInsertV1(map,&layout,input,&original));
    XL6_ValueIterator* cursor=createMode(3,map,&layout,converter); assert(cursor); xrtValueRelease(map);
    assert(api->Advance(cursor,&found) && found && xl6ValueIteratorKeyCopyV3(cursor,&key));
    assert(xl6SlotMapKeyEqualV1(key,original)); xvalue* value=api->View(cursor)->item; assert(value);
    if(layout.value.transfer==1) { int64 number=0; assert(xrtValueGetInt(value,&number) && number==*(const int64*)input); }
    if(layout.value.transfer==3) assert(value==*(xvalue* const*)input);
    if(layout.value.transfer==4) assert(xrtValueGetFuture(value)==*(xfuture* const*)input);
    if(layout.value.transfer==5) {
        XL6_SlotMapKeyV1 record={0}; assert(xl6SlotMapKeyUnboxV1(value,&record));
        assert(xl6SlotMapKeyEqualV1(record,*(const XL6_SlotMapKeyV1*)input)); dropKey(&record);
    }
    api->Destroy(cursor); assert(xl6SlotMapKeyEqualV1(key,original)); dropKey(&key); dropKey(&original);
}
static void domains(void)
{
    domain((XL6_SlotMapLayoutV1){0xD011,{0xD012,0xD013,1,sizeof(int64_t),xrtTypeInt64()}},XL6_VALUE_ADMIT_I64,&(int64_t){42},NULL);
    xvalue* reference=xrtValueArray(); assert(reference);
    domain((XL6_SlotMapLayoutV1){0xD021,{0xD022,0xD023,3,sizeof(xvalue*),xrtTypeValue()}},XL6_VALUE_ADMIT_ANY,&reference,xl6ArrayReferenceConverterV1());
    xrtValueRelease(reference);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    domain((XL6_SlotMapLayoutV1){0xD031,{0xD032,0xD033,4,sizeof(xfuture*),xrtTypeFuture()}},0,&future,NULL);
    xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    XL6_SlotMapKeyV1 key={0}; assert(xl6SlotMapInsertV1(map,&layout,&(xstrview){"key",3},&key));
    domain((XL6_SlotMapLayoutV1){0xD041,{0xD042,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,5,sizeof(key),xl6SlotMapKeyTypeV1()}},0,&key,xl6SlotMapKeyConverterV1());
    xrtValueRelease(map); dropKey(&key);
}
static void rejected(void)
{
    XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    XL6_SlotMapKeyV1 original={0}; xstrview text={"a\0你",5}; assert(xl6SlotMapInsertV1(map,&layout,&text,&original));
    for(int failure=-1;failure<=1;failure+=2) for(unsigned kind=0;kind<2;++kind) {
        XL6_ValueIterator* cursor=createMode(3,map,&layout,xl6StringConverterV1()); assert(cursor);
        bool found=false; XL6_SlotMapKeyV1 key={0}; xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if(kind) {
            assert(api->Advance(cursor,&found) && found); keyFailure=failure;
            assert(!xl6ValueIteratorKeyCopyV3(cursor,&key) && !key.instance && !key.slot && !keyFailure);
        } else { valueFailure=failure; assert(!api->Advance(cursor,&found) && !found && !valueFailure); }
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(failure<0 ? !outcome.error : xrtErrorKind(outcome.error)==XERR_STATE); xrtErrorFree(outcome.error);
        assert(!api->View(cursor)); xrtClearError(); assert(!api->Advance(cursor,&found) && !found); xrtClearError();
        api->Destroy(cursor); assert(xl6SlotMapCountV1(map,&layout)==1);
    }
    XL6_SlotMapLayoutV1 wrong=layout; ++wrong.value.elementIdentity;
    assert(!createMode(3,map,&wrong,xl6StringConverterV1())); xrtClearError();
    assert(!xl6SlotMapPairIteratorCreateV1(map,&layout,0,xl6StringConverterV1())); xrtClearError();
    assert(!xl6SlotMapKeyIteratorCreateV1(map,&layout,layout.identity)); xrtClearError();
    XL6_SlotMapSnapshotPairV1 pair={0};
    assert(xl6SlotMapPairsV1(map,&layout,0xCA14,xl6StringConverterV1(),&pair));
    XL6_SequenceLayoutV1 keyLayout=xl6SlotMapIteratorKeyLayoutV1(0xCA14),wrongValue=layout.value;
    ++wrongValue.elementIdentity;
    assert(!xl6SlotMapIteratorWrapTakeV1(&pair,&keyLayout,&wrongValue,SIZE_MAX,xl6SlotMapKeyIteratorOpsV1Get()));
    assert(!pair.keys && !pair.values && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    XL6_ValueIterator* cursor=createMode(3,map,&layout,xl6StringConverterV1()); bool found=false; assert(cursor);
    assert(api->Advance(cursor,&found) && found);
    XL6_SlotMapKeyV1 prior={0}; assert(xl6ValueIteratorKeyCopyV3(cursor,&prior));
    assert(!xl6ValueIteratorKeyCopyV3(cursor,&prior) && xl6SlotMapKeyEqualV1(prior,original)); xrtClearError(); dropKey(&prior);
    xrtownershipref root=api->Ownership(cursor); const xrtownershipadapterv1* adapter=xl6ValueIteratorOwnershipAdapterV1(root);
    int token=0; xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter && adapter->Hold(root.Data) && adapter->Claim(root.Data,&token));
    adapter->Restore(root.Data,&token); assert(adapter->Claim(root.Data,&token)); adapter->Clear(root.Data,&token);
    assert(xrtOwnershipScopeEnd(&freeze)); assert(!api->View(cursor)); xrtClearError();
    assert(adapter->Finish(root.Data,&token) && adapter->Finish(root.Data,&token)); adapter->Drop(root.Data); api->Destroy(cursor);
    /* Empty captures have a normal EOF and never manufacture an absent key. */
    assert(xl6SlotMapClearV1(map,&layout));
    for(unsigned mode=0;mode<4;++mode) {
        cursor=createMode(mode,map,&layout,xl6StringConverterV1()); assert(cursor);
        assert(api->Advance(cursor,&found) && !found); api->Destroy(cursor);
    }
    dropKey(&original); xrtValueRelease(map);
}
static bool cursorFault(unsigned mode,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_SlotMapLayoutV1 layout=textLayout();
    xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map); XL6_SlotMapKeyV1 original={0},key={0};
    assert(xl6SlotMapInsertV1(map,&layout,&(xstrview){"a\0你",5},&original));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    XL6_ValueIterator* cursor=createMode(mode,map,&layout,xl6StringConverterV1()); bool found=false;
    bool success=cursor && api->Advance(cursor,&found) && found && (!(mode&1) || xl6ValueIteratorKeyCopyV3(cursor,&key));
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if(hit) assert(xrtErrorKind(outcome.error)==XERR_MEMORY);
    api->Destroy(cursor); dropKey(&key); xrtErrorFree(outcome.error);
    assert(xl6SlotMapCountV1(map,&layout)==1);
    cursor=createMode(mode,map,&layout,xl6StringConverterV1()); assert(cursor && api->Advance(cursor,&found) && found);
    if(mode&1) assert(xl6ValueIteratorKeyCopyV3(cursor,&key));
    api->Destroy(cursor); dropKey(&key); dropKey(&original); xrtValueRelease(map); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
typedef struct CycleContext {
    XL6_ValueIterator* cursor;
    const xvalue* valueSnapshot;
    xvalue* extra;
    unsigned mode,finalized,dropped;
} CycleContext;
static bool emptyTrace(const void* data,xrtownershipvisitor visit,void* context)
{ (void)data; (void)visit; (void)context; return true; }
static void emptyDrop(void* data) { assert(!data); }
static bool cycleTrace(const void* data,xrtownershipvisitor visit,void* context)
{
    const CycleContext* owner=data;
    return !owner->cursor || visit(api->Ownership(owner->cursor),context);
}
static void cycleDrop(void* data)
{
    CycleContext* owner=data; assert(owner->finalized==1 && !owner->dropped && !owner->extra);
    xrtownershipscope mutation={0}; assert(xrtOwnershipMutationBegin(&mutation));
    XL6_ValueIterator* cursor=owner->cursor; owner->cursor=NULL; owner->dropped=1;
    assert(xrtOwnershipScopeEnd(&mutation)); api->Destroy(cursor);
}
static bool cycleFinalize(xvalue* object,void* data)
{
    CycleContext* owner=data; assert(!owner->finalized && !owner->dropped); ++owner->finalized;
    const XL6_ValueIteratorView* view=api->View(owner->cursor);
    assert(view && view->item==object && view->ordinal==0);
    if(owner->mode==1) { xrtSetErrorKind(XERR_STATE); return false; }
    if(owner->mode==2) {
        owner->extra=xrtValueRetain(owner->valueSnapshot); assert(owner->extra==owner->valueSnapshot);
    }
    return true;
}
static void cycleTerminal(xvalue* object,void* data) { (void)cycleFinalize(object,data); }
static const xvalueobjectownershipv1 cyclePolicy={sizeof(cyclePolicy),cycleTerminal,cycleTrace,cycleDrop,
    emptyTrace,emptyDrop,cycleFinalize};
static bool cycleResolve(xrtownershipref ref,void* denied,const xrtownershipadapterv1** adapter,
    const xrtownershippreparationv1** preparation)
{
    *preparation=NULL; *adapter=xl6ValueIteratorOwnershipAdapterV1(ref);
    if(*adapter) return true;
    const XL6_SequenceAdmissionOpsV2* admitted[]={xl6ArraySlotMapKeyAdmissionOpsV2Get()};
    *adapter=xl6SequenceOwnershipAdapterV2(ref,admitted,1);
    if(*adapter) return denied==NULL;
    *adapter=xrtValueObjectOwnershipAdapterV1(ref,&cyclePolicy);
    if(!*adapter) *adapter=xrtValueOwnershipAdapterV1(ref);
    return *adapter!=NULL;
}
static bool findSnapshot(xrtownershipref ref,void* data)
{
    CycleContext* owner=data;
    if(xrtValueTypeId(ref.Data)==0xDE02) owner->valueSnapshot=ref.Data;
    return true;
}
static void cycles(void)
{
    for(unsigned mode=0;mode<3;++mode) {
        CycleContext context={.mode=mode};
        xvalue* object=xrtValueObject(); assert(object &&
            xrtValueObjectLifetimeBindOwned(object,NULL,emptyTrace,emptyDrop) &&
            xrtValueObjectFinalizerPrepareOwned(object,cycleTerminal,&context,cycleTrace,cycleDrop) &&
            xrtValueObjectOwnershipBindV1(object,&cyclePolicy) && xrtValueObjectFinalizerCommit(object));
        XL6_SlotMapLayoutV1 layout={0xDE01,{0xDE02,0xDE03,3,sizeof(xvalue*),xrtTypeValue()}};
        xvalue* map=make(&layout,XL6_VALUE_ADMIT_ANY); XL6_SlotMapKeyV1 a={0},b={0};
        assert(map && xl6SlotMapInsertV1(map,&layout,&object,&a) && xl6SlotMapInsertV1(map,&layout,&object,&b));
        XL6_ValueIterator* cursor=createMode(3,map,&layout,xl6ArrayReferenceConverterV1()); bool found=false;
        assert(cursor && api->Advance(cursor,&found) && found && api->View(cursor)->item==object);
        xrtownershipscope mutation={0}; assert(xrtOwnershipMutationBegin(&mutation));
        context.cursor=cursor; assert(xrtOwnershipScopeEnd(&mutation));
        xvalue* weak=xrtValueWeakRef(object); assert(weak); xrtValueRelease(map); dropKey(&a); dropKey(&b);
        xrtownershipref root=api->Ownership(cursor); XL6_OwnershipPlan* plan=NULL;
        xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
        assert(root.Ops->Trace(root.Data,findSnapshot,&context) && context.valueSnapshot);
        assert(xrtOwnershipScopeEnd(&freeze));
        assert(xl6OwnershipPlanTryCreateV2(root,cycleResolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_ROOTED && plan);
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL; xrtValueRelease(object);
        assert(xl6OwnershipPlanTryCreateV2(root,cycleResolve,&context,&plan)==XL6_OWNERSHIP_PLAN_ERROR && !plan);
        xrtClearError(); assert(!context.finalized && !context.dropped);
        assert(xl6OwnershipPlanTryCreateV2(root,cycleResolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);
        assert(xl6OwnershipPlanPrepare(plan)==XL6_OWNERSHIP_STEP_READY);
        XL6_OwnershipStepResult status=xl6OwnershipPlanFinalizeV2(plan);
        assert(context.finalized==1 && !context.dropped);
        if(mode==1) {
            assert(status==XL6_OWNERSHIP_STEP_FAILED && xrtErrorKind(xrtGetError())==XERR_STATE);
            assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL; xrtClearError();
            assert(xl6OwnershipPlanTryCreateV2(root,cycleResolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);
            assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && context.finalized==1);
        } else assert(status==XL6_OWNERSHIP_STEP_READY);
        if(mode==2) {
            assert(context.extra && xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_ROOTED);
            assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY); plan=NULL;
            xrtValueRelease(context.extra); context.extra=NULL;
            assert(xl6OwnershipPlanTryCreateV2(root,cycleResolve,NULL,&plan)==XL6_OWNERSHIP_PLAN_FINALIZERS && plan);
            assert(xl6OwnershipPlanFinalizeV2(plan)==XL6_OWNERSHIP_STEP_READY && context.finalized==1);
        }
        assert(xl6OwnershipPlanTryCommit(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(xl6OwnershipPlanFinish(plan) && xl6OwnershipPlanFinish(plan));
        assert(xl6OwnershipPlanTryDestroy(plan)==XL6_OWNERSHIP_PLAN_READY);
        assert(context.dropped==1 && !context.cursor && xrtValueWeakRefExpired(weak)); xrtValueRelease(weak);
    }
}
int main(int argc,char** argv)
{
    assert(argc==2); api=xl6ValueIteratorRuntimeV2Get();
    if(!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); modes(); domains(); rejected(); cycles();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("SlotMap native cursor: exact keys, five domains, four minimal lanes, phased graph edges, lifetimes balanced");
    } else {
        assert(!strcmp(argv[1],"prefixes"));
        for(unsigned mode=0;mode<4;++mode) {
            size_t at=0; while(at<4096 && cursorFault(mode,at)) ++at; assert(at<4096);
            printf("cursor mode=%u prefixes=%zu first-no-hit=%zu balanced\n",mode,at+1,at);
        }
    }
    return 0;
}
