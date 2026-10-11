#include "src/runtime/xl6_array_native_value_admission.h"
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_dict.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include "src/ir/xl6_ir.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert((unsigned)XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY==5,"explicit IR/native transport");
static int copyFailure;
extern bool __real_xrtTypeCopyValue(const xrttype*,ptr,const void*);
bool __wrap_xrtTypeCopyValue(const xrttype* type,ptr output,const void* input)
{
    int selected=copyFailure; copyFailure=0;
    bool success=__real_xrtTypeCopyValue(type,output,input);
    if (success && selected) {
        if (selected>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    }
    return selected==2 || selected==-2 ? false : success;
}
static const uint64_t keyIdentity=XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID;
static XL6_SequenceLayoutV1 tuple(void)
{ return (XL6_SequenceLayoutV1){0xED01,keyIdentity,5,sizeof(XL6_SlotMapKeyV1),xl6SlotMapKeyTypeV1()}; }
static xvalue* witness(unsigned role) { return xl6SequenceSlotMapKeyCreateV1(role,tuple().identity); }
static void releaseKey(XL6_SlotMapKeyV1* key) { xrtTypeDropValue(xl6SlotMapKeyTypeV1(),key); }
static xvalue* sample(XL6_SlotMapKeyV1* output)
{
    const xrttype* type=xrtTypeInt64(); XL6_SlotMapLayoutV1 layout={0xEC01,{0xEC02,0xEC03,1,sizeof(int64_t),type}};
    xvalue* seed=xl6SequencePrimitiveCreateV1(1,layout.value.identity,layout.value.elementIdentity,1,sizeof(int64_t),type,
        XL6_VALUE_ADMIT_I64,false,layout.value.elementIdentity); assert(seed);
    xvalue* map=xl6SlotMapCreateTakeV1(&layout,seed); assert(map);
    assert(xl6SlotMapInsertV1(map,&layout,&(int64_t){42},output)); return map;
}
#define ARGS tuple().identity,keyIdentity,5,sizeof(XL6_SlotMapKeyV1),xl6SlotMapKeyTypeV1()
static bool edge(xrtownershipref ref,ptr context) { (void)ref; ++*(size_t*)context; return true; }
static bool capture(xrtownershipref ref,ptr context) { *(xrtownershipref*)context=ref; return true; }
static void graph(const xvalue* value,unsigned kind)
{
    xrtownershipref shell=xrtValueOwnership(value),owner={0};
    assert(shell.Ops->Trace(shell.Data,capture,&owner) && owner.Data);
    const XL6_SequenceAdmissionOpsV2* allowed[]={xl6ArraySlotMapKeyAdmissionOpsV2Get()};
    const xrtownershipadapterv1* absent=kind==0 ? xl6SequenceOwnershipAdapterV1(owner) : kind==1 ?
        xl6ListOwnershipAdapterV1(owner) : kind==2 ? xl6SetOwnershipAdapterV1(owner) : kind==3 ?
        xl6DictOwnershipAdapterV1(owner) : xl6SlotMapOwnershipAdapterV1(owner);
    assert(!absent);
    const xrtownershipadapterv1* actual=kind==0 ? xl6SequenceOwnershipAdapterV2(owner,allowed,1) : kind==1 ?
        xl6ListOwnershipAdapterV2(owner,allowed,1) : kind==2 ? xl6SetOwnershipAdapterV2(owner,allowed,1) :
        kind==3 ? xl6DictOwnershipAdapterV2(owner,allowed,1) : xl6SlotMapOwnershipAdapterV2(owner,allowed,1); assert(actual);
    size_t count=0; assert(owner.Ops->Trace(owner.Data,edge,&count) && !count);
}
static void vectors(void)
{
    XL6_SlotMapKeyV1 key={0},other={0},out={0}; xvalue* source=sample(&key),*second=sample(&other);
    XL6_ListLayoutV1 l={0xED02,tuple()}; XL6_SetLayoutV1 s={0xED03,tuple()}; XL6_DictLayoutV1 d={0xED04,tuple()};
    XL6_SlotMapLayoutV1 m={0xED05,tuple()}; XL6_SlotMapKeyV1 stored={0};
    xvalue* a=witness(1),*stack=witness(2),*list=xl6ListCreateTakeV1(&l,witness(1));
    xvalue* set=xl6SetCreateTakeV1(&s,witness(1)),*dict=xl6DictCreateTakeV1(&d,witness(1));
    assert(a && stack && list && set && dict);
    xvalue* nestedMap=xl6SlotMapCreateTakeV1(&m,witness(1)); assert(nestedMap);
    assert(xl6SlotMapInsertV1(nestedMap,&m,&key,&stored));
    assert(!xl6SequenceTupleV1(0xEE01,keyIdentity,1,sizeof(key),xl6SlotMapKeyTypeV1())); xrtClearError();
    assert(!xl6SequenceTupleV1(0xEE01,keyIdentity,5,sizeof(int64_t),xrtTypeInt64())); xrtClearError();
    assert(!xl6SequenceSlotMapKeyCreateV1(0,tuple().identity)); xrtClearError();
    xvalue* raw=xl6ArrayCreateV1(ARGS); assert(raw && xl6ArrayPushV1(raw,ARGS,&key));
    assert(!xl6ArrayReadV1(raw,ARGS,0,&out) && !out.instance && !out.slot); xrtClearError();
    assert(xl6ArrayFindV1(raw,ARGS,&key)==-1 && xrtGetError()); xrtClearError();
    xrtValueRelease(raw);
    assert(xl6ArrayPushV1(a,ARGS,&key) && xl6ArrayPushV1(a,ARGS,&other));
    assert(xl6ArrayReadV1(a,ARGS,0,&out) && xl6SlotMapKeyEqualV1(key,out));
    assert(!xl6ArrayReadV1(a,ARGS,0,&out) && xl6SlotMapKeyEqualV1(key,out)); xrtClearError(); releaseKey(&out);
    assert(xl6ArrayFindV1(a,ARGS,&key)==0 && xl6ArrayFindV1(a,ARGS,&other)==1);
    xvalue* clone=xl6ArrayCloneV1(a,ARGS); assert(clone && xl6ArrayEqualsV1(a,clone,ARGS));
    uint64 ah=0,bh=0; assert(xrtValueHash(a,&ah) && xrtValueHash(clone,&bh) && ah==bh);
    assert(xl6ArrayResizeV1(a,ARGS,3) && xl6ArrayReadV1(a,ARGS,2,&out) && !out.instance && !out.slot);
    assert(xl6ListSetV1(list,&l,INT64_MIN,&key) && xl6ListSetV1(list,&l,INT64_MAX,&other));
    bool found=false,added=false;
    assert(xl6ListGetV1(list,&l,INT64_MIN,&out,false,&found) && found && xl6SlotMapKeyEqualV1(out,key)); releaseKey(&out);
    xvalue* listClone=xl6ListCloneV1(list,&l); assert(listClone);
    assert(xl6ListEqualsV1(list,listClone,&l,&found) && found && xl6ListContainsV1(list,&l,&other,&found) && found);
    assert(xl6SetAddV1(set,&s,&key,&added) && added);
    assert(xl6SetAddV1(set,&s,&key,&added) && !added);
    assert(xl6SetAddV1(set,&s,&other,&added) && added && xl6SetCountV1(set,&s,false)==2);
    set=xl6SetProjectionTakeV1(set,&s,xl6SlotMapKeyConverterV1()); assert(set);
    xvalue* box=xl6SlotMapKeyBoxV1(key); assert(box);
    assert(xl6SetErasedKeyV1(set,box,XL6_SET_ADD_V1,&added) && !added);
    const xstrview name=XRT_STR_INIT("a\0b");
    assert(xl6DictSetV1(dict,&d,name,&key) && xl6DictReadSlotV1(dict,&d,name,&out,true));
    assert(xl6SlotMapKeyEqualV1(key,out));
    assert(!xl6DictReadSlotV1(dict,&d,name,&out,true) && xl6SlotMapKeyEqualV1(key,out)); xrtClearError(); releaseKey(&out);
    assert(xl6SlotMapReadV1(nestedMap,&m,stored,&out,&found) && found && xl6SlotMapKeyEqualV1(out,key));
    assert(!xl6SlotMapReadV1(nestedMap,&m,stored,&out,&found)); xrtClearError(); releaseKey(&out);
    for (int fail=-2;fail<=2;++fail) if (fail) for (unsigned kind=0;kind<4;++kind) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); copyFailure=fail;
        bool success=kind==0 ? xl6ArrayReadV1(a,ARGS,0,&out) : kind==1 ?
            xl6ListGetV1(list,&l,INT64_MIN,&out,false,&found) : kind==2 ? xl6DictReadSlotV1(dict,&d,name,&out,true) :
            xl6SlotMapReadV1(nestedMap,&m,stored,&out,&found);
        assert(!success && !copyFailure && !out.instance && !out.slot);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(fail<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    graph(a,0); graph(stack,0); graph(list,1); graph(set,2); graph(dict,3); graph(nestedMap,4);
    xrtValueRelease(source); xrtValueRelease(second); releaseKey(&key); releaseKey(&other);
    assert(xl6ArrayReadV1(a,ARGS,0,&out) && xl6SetKeyV1(set,&s,&out,true,&found) && found); releaseKey(&out);
    assert(xl6SlotMapReadV1(nestedMap,&m,stored,&out,&found) && found); releaseKey(&out);
    releaseKey(&stored); xrtValueRelease(nestedMap);
    xrtValueRelease(box); xrtValueRelease(dict); xrtValueRelease(set); xrtValueRelease(listClone);
    xrtValueRelease(list); xrtValueRelease(stack); xrtValueRelease(clone); xrtValueRelease(a);
}
static bool fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_SlotMapKeyV1 key={0},out={0}; xvalue* source=sample(&key);
    XL6_ListLayoutV1 l={0xED02,tuple()}; XL6_SetLayoutV1 s={0xED03,tuple()}; XL6_DictLayoutV1 d={0xED04,tuple()};
    XL6_SlotMapLayoutV1 m={0xED05,tuple()};
    xvalue* a=witness(1); assert(a); assert(xl6ArrayPushV1(a,ARGS,&key));
    xvalue* box=xl6SlotMapKeyBoxV1(key); assert(box); xvalue* result=NULL;
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); assert(xrtMemDebugFailAfter(point));
    bool success=false;
    if (!strcmp(operation,"factory")) { result=witness(1); success=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(a,ARGS); success=result!=NULL; }
    else if (!strcmp(operation,"read")) success=xl6ArrayReadV1(a,ARGS,0,&out);
    else if (!strcmp(operation,"list")) { result=xl6ListCreateTakeV1(&l,witness(1)); success=result!=NULL; }
    else if (!strcmp(operation,"set")) { result=xl6SetCreateTakeV1(&s,witness(1)); success=result!=NULL; }
    else if (!strcmp(operation,"dict")) { result=xl6DictCreateTakeV1(&d,witness(1)); success=result!=NULL; }
    else if (!strcmp(operation,"slot-map")) { result=xl6SlotMapCreateTakeV1(&m,witness(1)); success=result!=NULL; }
    else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if (hit) assert(!result && !out.instance && xrtErrorKind(outcome.error)==XERR_MEMORY);
    assert(xl6ArraySizeV1(a,ARGS,false)==1);
    releaseKey(&out); releaseKey(&key); xrtValueRelease(result); xrtErrorFree(outcome.error);
    xrtValueRelease(box); xrtValueRelease(a); xrtValueRelease(source); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); vectors(); assert(!xrtGetError() && !xl6ProductMemoryEnd());
        xl6ProductMemoryVerify(); puts("[PASS] owning native records across sealed containers"); return 0;
    }
    assert(!strcmp(argv[1],"prefixes")); const char* operations[]={"factory","clone","read","list","set","dict","slot-map"};
    for (size_t i=0;i<sizeof(operations)/sizeof(*operations);++i) {
        size_t point=0; while (point<4096 && fault(operations[i],point)) ++point; assert(point<4096);
        printf("native-value %s runs=%zu first-no-hit=%zu\n",operations[i],point+1,point);
    }
    return 0;
}
