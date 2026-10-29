#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_string_type.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int entryFailure,cleanupFailure,copyFailure;
static bool generationLimit;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
extern bool __real_xrtTypeCopyValue(const xrttype*,ptr,const void*);
extern xslot __real_xrtSlotMapInsert(xslotmap*,ptr);
extern xslot xl6TestSlotMapGenerationLimit(xslotmap*,xslot);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (!entryFailure) return __real_xl6ErrorScopeBegin(scope);
    int selected=entryFailure; entryFailure=0;
    if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError(); return false;
}
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failure)
{
    if (!cleanupFailure) return __real_xl6ErrorScopeBeginCleanup(scope,failure);
    int selected=cleanupFailure; cleanupFailure=0;
    if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError(); return false;
}
bool __wrap_xrtTypeCopyValue(const xrttype* type,ptr target,const void* source)
{
    int selected=copyFailure; copyFailure=0;
    bool success=__real_xrtTypeCopyValue(type,target,source);
    if (success && selected) {
        if (selected>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    }
    return selected==2 || selected==-2 ? false : success;
}
xslot __wrap_xrtSlotMapInsert(xslotmap* map,ptr item)
{
    xslot result=__real_xrtSlotMapInsert(map,item);
    if (result && generationLimit) { generationLimit=false; result=xl6TestSlotMapGenerationLimit(map,result); }
    return result;
}
static XL6_SlotMapLayoutV1 layout(unsigned transfer,const xrttype* type)
{ return (XL6_SlotMapLayoutV1){0xC701,{0xC702,0xC703,transfer,type->Size,type}}; }
static xvalue* seed(const XL6_SlotMapLayoutV1* t,unsigned rule)
{
    const XL6_SequenceLayoutV1* v=&t->value;
    if (v->transfer==3) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,v->identity,v->elementIdentity,rule,false,v->elementIdentity);
    if (v->transfer==4) return xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,v->identity,v->elementIdentity,v->elementIdentity,false);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,v->identity,v->elementIdentity,v->transfer,v->size,v->type,rule,false,v->elementIdentity);
}
static xvalue* create(const XL6_SlotMapLayoutV1* t,unsigned rule)
{ return xl6SlotMapCreateTakeV1(t,seed(t,rule)); }
static void keyDrop(XL6_SlotMapKeyV1* key) { xrtTypeDropValue(xl6SlotMapKeyTypeV1(),key); }
static bool foreignRetain(ptr raw,ptr* output,ptr context)
{ (void)context; *output=raw; return true; }
static void foreignDrop(ptr raw,ptr context) { (void)raw; (void)context; }
static const xvaluehandleops foreignOps={foreignRetain,foreignDrop,NULL,NULL};
static void vectors(void)
{
    assert(xrtTypeValidate(xl6SlotMapKeyTypeV1()));
    XL6_SlotMapLayoutV1 t=layout(1,xrtTypeInt64()); xvalue* map=create(&t,XL6_VALUE_ADMIT_I64); assert(map);
    XL6_SlotMapKeyV1 first={0},second={0},copy={0}; bool found=true; int64_t output=73;
    assert(xl6SlotMapReadV1(map,&t,first,&output,&found) && !found && output==73);
    assert(xl6SlotMapInsertV1(map,&t,&(int64_t){42},&first));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SlotMapReadV1(map,&t,first,&output,&found) && found && output==42);
    assert(xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&copy,&first) && xl6SlotMapKeyEqualV1(first,copy));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&copy,&first) && xl6SlotMapKeyEqualV1(first,copy));
    assert(!xrtGetError());
    assert(xl6SlotMapKeyV1(map,&t,first,true,&found) && found);
    assert(xl6SlotMapInsertV1(map,&t,&(int64_t){17},&second));
    assert(xrtSlotIndex(first.slot)==xrtSlotIndex(second.slot) && first.slot!=second.slot);
    assert(xl6SlotMapKeyV1(map,&t,first,false,&found) && !found);
    assert(xl6SlotMapReadV1(map,&t,second,&output,&found) && found && output==17);
    xvalue* other=create(&t,XL6_VALUE_ADMIT_I64); XL6_SlotMapKeyV1 foreign={0}; assert(other);
    assert(xl6SlotMapInsertV1(other,&t,&(int64_t){33},&foreign));
    assert(xl6SlotMapKeyV1(other,&t,first,false,&found) && !found);
    assert(xl6SlotMapKeyV1(map,&t,foreign,false,&found) && !found);
    assert(xl6SlotMapClearV1(map,&t)); assert(xl6SlotMapKeyV1(map,&t,second,false,&found) && !found);
    XL6_SlotMapKeyV1 exhausted={0},after={0}; generationLimit=true;
    assert(xl6SlotMapInsertV1(map,&t,&(int64_t){1},&exhausted) && !generationLimit);
    assert(xrtSlotGeneration(exhausted.slot)==UINT32_MAX);
    assert(xl6SlotMapKeyV1(map,&t,exhausted,true,&found) && found);
    assert(xl6SlotMapInsertV1(map,&t,&(int64_t){2},&after));
    assert(xrtSlotIndex(after.slot)!=xrtSlotIndex(exhausted.slot));
    XL6_SlotMapLayoutV1 wrong=t; ++wrong.value.elementIdentity;
    assert(xl6SlotMapCountV1(map,&wrong)==-1); xrtClearError();
    ptr opaque=(ptr)(uintptr_t)17; xvalue* forged=xrtValueHandleTake(&opaque,&foreignOps,NULL); assert(forged);
    assert(xrtValueTypeIdBind(forged,t.identity));
    assert(xl6SlotMapCountV1(forged,&t)==-1); xrtClearError(); xrtValueRelease(forged);
    assert(!xl6SlotMapReserveV1(map,&t,-1)); xrtClearError();
    assert(!xl6SlotMapReserveV1(map,&t,(int64_t)UINT32_MAX+1)); xrtClearError();
    assert(xl6SlotMapReserveV1(map,&t,128) && xl6SlotMapCapacityV1(map,&t)>=128);
    keyDrop(&foreign); xrtValueRelease(other); xrtValueRelease(map);
    /* A retained key record, not the destroyed map address, blocks ABA. */
    for (unsigned i=0;i<64;++i) {
        map=create(&t,XL6_VALUE_ADMIT_I64); assert(map); XL6_SlotMapKeyV1 fresh={0};
        assert(xl6SlotMapInsertV1(map,&t,&(int64_t){1},&fresh));
        assert(!xl6SlotMapKeyEqualV1(first,fresh));
        assert(xl6SlotMapKeyV1(map,&t,first,false,&found) && !found); keyDrop(&fresh); xrtValueRelease(map);
    }
    keyDrop(&first); keyDrop(&copy); keyDrop(&second); keyDrop(&exhausted); keyDrop(&after);
    t=layout(1,xrtTypeInt64()); map=create(&t,XL6_VALUE_ADMIT_U8); assert(map);
    first=(XL6_SlotMapKeyV1){0}; assert(!xl6SlotMapInsertV1(map,&t,&(int64_t){256},&first) && !first.instance);
    xrtClearError(); assert(!xl6SlotMapCountV1(map,&t)); xrtValueRelease(map);
}
static void textBoundaries(void)
{
    XL6_SlotMapLayoutV1 t=layout(2,xl6StringTypeV1()); xvalue* map=create(&t,XL6_VALUE_ADMIT_STRING); assert(map);
    xstrview input={"a\0你",5},output={0}; XL6_SlotMapKeyV1 key={0}; bool found=false;
    assert(xl6SlotMapInsertV1(map,&t,&input,&key));
    assert(xl6SlotMapReadV1(map,&t,key,&output,&found) && found && output.Size==5 && output.Data!=input.Data);
    assert(!memcmp(output.Data,input.Data,5)); xrtFree((ptr)output.Data); output=(xstrview){0};
    const int failures[]={-2,-1,1,2};
    for (size_t i=0;i<sizeof(failures)/sizeof(*failures);++i) for (unsigned cleanup=0;cleanup<2;++cleanup) {
        int failure=failures[i];
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure; cleanupFailure=cleanup ? -1 : 0;
        assert(!xl6SlotMapReadV1(map,&t,key,&output,&found) && !output.Data && !output.Size);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !copyFailure && !cleanupFailure);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    for (size_t i=0;i<sizeof(failures)/sizeof(*failures);++i) {
        XL6_SlotMapKeyV1 attempted={0}; bool updated=false;
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); copyFailure=failures[i];
        assert(!xl6SlotMapInsertV1(map,&t,&input,&attempted) && !attempted.instance);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !copyFailure);
        xrtErrorFree(result.error); assert(xl6SlotMapCountV1(map,&t)==1);
        assert(xl6ErrorScopeBegin(&parent)); copyFailure=failures[i];
        assert(!xl6SlotMapSetV1(map,&t,key,&input,&updated) && !updated);
        result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !copyFailure); xrtErrorFree(result.error);
        assert(xl6SlotMapReadV1(map,&t,key,&output,&found) && found && output.Size==5);
        xrtFree((ptr)output.Data); output=(xstrview){0};
    }
    for (int failure=-1;failure<=1;failure+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6SlotMapReadV1(map,&t,key,&output,&found));
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !entryFailure);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_MEMORY); xrtErrorFree(result.error);
    }
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* incoming=xrtGetError();
    assert(xl6SlotMapReadV1(map,&t,key,&output,&found) && xrtGetError()==incoming); xrtClearError();
    xrtFree((ptr)output.Data); keyDrop(&key); xrtValueRelease(map);
}
typedef struct Edges { size_t count; xrtownershipref first; bool same; } Edges;
static bool edge(xrtownershipref ref,ptr opaque)
{
    Edges* e=opaque; if (!e->count) { e->first=ref; e->same=true; }
    else e->same=e->same && ref.Data==e->first.Data && ref.Ops==e->first.Ops;
    ++e->count; return true;
}
static xrtownershipref ownerOf(xvalue* value)
{ xrtownershipref shell=xrtValueOwnership(value); Edges e={0}; assert(shell.Ops->Trace(shell.Data,edge,&e) && e.count==1); return e.first; }
static void graph(void)
{
    XL6_SlotMapLayoutV1 t=layout(3,xrtTypeValue()); xvalue* map=create(&t,XL6_VALUE_ADMIT_ANY); assert(map);
    XL6_SlotMapKeyV1 a={0},b={0}; assert(xl6SlotMapInsertV1(map,&t,&map,&a) && xl6SlotMapInsertV1(map,&t,&map,&b));
    xrtownershipref owner=ownerOf(map),expected=xrtValueOwnership(map); Edges e={0};
    assert(owner.Ops->Trace(owner.Data,edge,&e) && e.count==2 && e.same && e.first.Data==expected.Data && e.first.Ops==expected.Ops);
    const xrtownershipadapterv1* adapter=xl6SlotMapOwnershipAdapterV1(owner); assert(adapter);
    unsigned token=0; xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token)); adapter->Restore(owner.Data,&token);
    assert(adapter->Claim(owner.Data,&token)); adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); cleanupFailure=-1;
    assert(!adapter->Finish(owner.Data,&token)); xl6ErrorScopeCaptureFailure(&parent);
    assert(adapter->Finish(owner.Data,&token) && adapter->Finish(owner.Data,&token));
    xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !result.error);
    adapter->Drop(owner.Data); keyDrop(&a); keyDrop(&b); xrtValueRelease(map);
    t=layout(4,xrtTypeFuture()); map=create(&t,0); assert(map);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    a=(XL6_SlotMapKeyV1){0}; assert(xl6SlotMapInsertV1(map,&t,&future,&a)); owner=ownerOf(map);
    const XL6_SequenceAdmissionOpsV2* allowed[]={xl6ArrayFutureAdmissionOpsV2Get()};
    assert(!xl6SlotMapOwnershipAdapterV1(owner) && xl6SlotMapOwnershipAdapterV2(owner,allowed,1));
    e=(Edges){0}; expected=xrtFutureOwnership(future);
    assert(owner.Ops->Trace(owner.Data,edge,&e) && e.count==1 && e.first.Data==expected.Data && e.first.Ops==expected.Ops);
    keyDrop(&a); xrtValueRelease(map); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
}
typedef struct DropState { xvalue* map; XL6_SlotMapLayoutV1 layout; unsigned count; bool absent; } DropState;
static void reentrantDrop(ptr raw,ptr context)
{
    DropState* state=raw; assert(state==context); ++state->count;
    if (state->absent) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    assert(xl6SlotMapCountV1(state->map,&state->layout)==-1);
}
static const xvaluehandleops reentrantOps={foreignRetain,reentrantDrop,NULL,NULL};
static void cleanupBoundaries(void)
{
    for (unsigned absent=0;absent<2;++absent) {
        XL6_SlotMapLayoutV1 t=layout(3,xrtTypeValue()); xvalue* map=create(&t,XL6_VALUE_ADMIT_ANY); assert(map);
        DropState state={map,t,0,absent!=0}; XL6_SlotMapKeyV1 keys[2]={{0},{0}};
        for (unsigned i=0;i<2;++i) {
            ptr raw=&state; xvalue* item=xrtValueHandleTake(&raw,&reentrantOps,&state); assert(item);
            assert(xl6SlotMapInsertV1(map,&t,&item,&keys[i])); xrtValueRelease(item);
        }
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
        assert(!xl6SlotMapClearV1(map,&t) && state.count==2);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(absent ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
        assert(!xl6SlotMapCountV1(map,&t)); keyDrop(&keys[0]); keyDrop(&keys[1]); xrtValueRelease(map);
    }
    for (int failure=-1;failure<=1;failure+=2) {
        XL6_SlotMapLayoutV1 t=layout(2,xl6StringTypeV1()); xvalue* map=create(&t,XL6_VALUE_ADMIT_STRING); assert(map);
        XL6_SlotMapKeyV1 key={0}; assert(xl6SlotMapInsertV1(map,&t,&(xstrview){"a\0b",3},&key));
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); cleanupFailure=failure;
        xrtValueRelease(map); assert(!cleanupFailure); xl6_error_result result=xl6ErrorScopeEnd(&parent);
        assert(!result.ok && (failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_MEMORY));
        xrtErrorFree(result.error); keyDrop(&key);
    }
}
static bool fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_SlotMapLayoutV1 t=layout(2,xl6StringTypeV1());
    xvalue* map=create(&t,XL6_VALUE_ADMIT_STRING); assert(map);
    XL6_SlotMapKeyV1 key={0},next={0}; xstrview old={"a\0你",5},text={"next\0",5},output={0}; bool found=false;
    assert(xl6SlotMapInsertV1(map,&t,&old,&key)); xvalue* witness=NULL,*created=NULL;
    if (!strcmp(operation,"create")) { witness=seed(&t,XL6_VALUE_ADMIT_STRING); assert(witness); }
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); assert(xrtMemDebugFailAfter(point));
    bool success=false;
    if (!strcmp(operation,"create")) { created=xl6SlotMapCreateTakeV1(&t,witness); witness=NULL; success=created!=NULL; }
    else if (!strcmp(operation,"insert")) success=xl6SlotMapInsertV1(map,&t,&text,&next);
    else if (!strcmp(operation,"replace")) success=xl6SlotMapSetV1(map,&t,key,&text,&found);
    else if (!strcmp(operation,"read")) success=xl6SlotMapReadV1(map,&t,key,&output,&found);
    else if (!strcmp(operation,"reserve")) success=xl6SlotMapReserveV1(map,&t,512);
    else if (!strcmp(operation,"clear")) success=xl6SlotMapClearV1(map,&t);
    else if (!strcmp(operation,"remove")) success=xl6SlotMapKeyV1(map,&t,key,true,&found);
    else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result result=xl6ErrorScopeEnd(&parent);
    assert(hit ? !success && !result.ok : success && result.ok);
    if (hit) assert(!next.instance && !created && !output.Data && xrtErrorKind(result.error)==XERR_MEMORY);
    xrtFree((ptr)output.Data); output=(xstrview){0};
    if (hit || (strcmp(operation,"remove") && strcmp(operation,"clear"))) {
        assert(xl6SlotMapReadV1(map,&t,key,&output,&found) && found);
        xstrview expected=!strcmp(operation,"replace") && !hit ? text : old;
        assert(output.Size==expected.Size && !memcmp(output.Data,expected.Data,expected.Size)); xrtFree((ptr)output.Data);
    }
    keyDrop(&key); keyDrop(&next); xrtErrorFree(result.error); xrtValueRelease(witness); xrtValueRelease(created); xrtValueRelease(map);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); vectors(); textBoundaries(); graph(); cleanupBoundaries();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); puts("[PASS] native owned SlotMap, real generations, instance records and phased edges"); return 0;
    }
    assert(!strcmp(argv[1],"prefixes"));
    const char* operations[]={"create","insert","replace","read","reserve","clear","remove"};
    for (size_t i=0;i<sizeof(operations)/sizeof(*operations);++i) {
        size_t point=0; while (point<4096 && fault(operations[i],point)) ++point; assert(point<4096);
        printf("slot-map %s runs=%zu first-no-hit=%zu\n",operations[i],point+1,point);
    }
    return 0;
}
