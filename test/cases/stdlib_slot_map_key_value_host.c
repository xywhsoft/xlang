#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_set_key_policy.h"
#include "src/runtime/xl6_native_leaf_value.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int copyFailure,entryFailure,bindFailure,releaseFailure;
extern bool __real_xrtTypeCopyValue(const xrttype*,ptr,const void*);
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xrtValueTypeIdBind(xvalue*,uint64);
extern void __real_xrtValueRelease(xvalue*);
void __wrap_xrtValueRelease(xvalue* value)
{
    __real_xrtValueRelease(value);
    int selected=releaseFailure; releaseFailure=0;
    if (selected>0) xrtSetErrorKind(XERR_STATE);
    else if (selected<0) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
}
bool __wrap_xrtTypeCopyValue(const xrttype* type,ptr output,const void* source)
{
    int selected=copyFailure; copyFailure=0;
    bool success=__real_xrtTypeCopyValue(type,output,source);
    if (success && selected) {
        if (selected>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    }
    return selected==2 || selected==-2 ? false : success;
}
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (!entryFailure) return __real_xl6ErrorScopeBegin(scope);
    int selected=entryFailure; entryFailure=0;
    if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError(); return false;
}
bool __wrap_xrtValueTypeIdBind(xvalue* value,uint64 identity)
{
    if (!bindFailure) return __real_xrtValueTypeIdBind(value,identity);
    int selected=bindFailure; bindFailure=0;
    if (selected>0) xrtSetErrorKind(XERR_STATE); else xrtClearError(); return false;
}
static XL6_SlotMapLayoutV1 layout(void)
{ return (XL6_SlotMapLayoutV1){0xDA01,{0xDA02,0xDA03,1,sizeof(int64_t),NULL}}; }
static xvalue* create(XL6_SlotMapKeyV1* key)
{
    XL6_SlotMapLayoutV1 t=layout(); t.value.type=xrtTypeInt64();
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,t.value.identity,t.value.elementIdentity,
        1,t.value.size,t.value.type,XL6_VALUE_ADMIT_I64,false,t.value.elementIdentity); assert(witness);
    xvalue* map=xl6SlotMapCreateTakeV1(&t,witness); assert(map);
    assert(xl6SlotMapInsertV1(map,&t,&(int64_t){42},key)); return map;
}
static void keyDrop(XL6_SlotMapKeyV1* key) { xrtTypeDropValue(xl6SlotMapKeyTypeV1(),key); }
static bool foreignRetain(ptr raw,ptr* output,ptr user)
{ (void)user; *output=raw; return true; }
static void foreignDrop(ptr raw,ptr user) { (void)raw; (void)user; }
static uint64 forbiddenHash(ptr raw,ptr user) { (void)raw; (void)user; assert(0); return 0; }
static bool forbiddenEqual(ptr a,ptr b,ptr user) { (void)a; (void)b; (void)user; assert(0); return false; }
static const xvaluehandleops foreignOps={foreignRetain,foreignDrop,forbiddenHash,forbiddenEqual};
static uint64 aliasHash(const void* raw,ptr user)
{ (void)user; return xl6SlotMapKeyHashV1(*(const XL6_SlotMapKeyV1*)raw); }
static bool aliasEqual(const void* a,const void* b,ptr user)
{ (void)user; return xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)a,*(const XL6_SlotMapKeyV1*)b); }
static bool aliasEmpty(const void* raw)
{ const XL6_SlotMapKeyV1* key=raw; return !key->instance && !key->slot; }
static const xtypedsetkeypolicy aliasContent={aliasHash,aliasEqual,NULL};
static const XL6_NativeLeafValueProfileV1 aliasProfile={
    xl6SlotMapKeyTypeV1,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,&aliasContent,aliasEmpty};
static bool edge(xrtownershipref ref,ptr count) { (void)ref; ++*(size_t*)count; return true; }
static xrttype rejectedType;
static const xrttype* rejectedTypeGet(void) { return &rejectedType; }
static xrttype emptyOutputType;
static const xrttype* emptyOutputTypeGet(void) { return &emptyOutputType; }
static bool copyIntoEmpty(ptr target,const void* source,const xrttype* type)
{
    (void)type; XL6_SlotMapKeyV1 temporary={0};
    if (!__real_xrtTypeCopyValue(xl6SlotMapKeyTypeV1(),&temporary,source)) return false;
    /* A legitimate Copy implementation may rely on its caller's documented
     * empty-output precondition instead of checking an occupied destination. */
    *(XL6_SlotMapKeyV1*)target=temporary; return true;
}
static void directCopyVectors(XL6_SlotMapKeyV1 key)
{
    const XL6_NativeLeafValueProfileV1* profile=xl6SlotMapKeyValueProfileV1Get();
    assert(profile && profile==xl6SlotMapKeyValueProfileV1Get() && xl6NativeLeafValueProfileValidV1(profile));
    assert(!xl6NativeLeafValueProfileValidV1(NULL));
    XL6_SlotMapKeyV1 output={0},empty={0};
    assert(xrtMemDebugFailAfter(0));
    for (size_t i=0;i<10000;++i) {
        assert(xl6NativeLeafValueCopyV1(profile,&key,&output));
        assert(xl6SlotMapKeyEqualV1(key,output)); keyDrop(&output);
    }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6NativeLeafValueCopyV1(profile,&key,&output));
    assert(!xl6NativeLeafValueCopyV1(profile,&key,&output) && xl6SlotMapKeyEqualV1(key,output));
    xrtClearError(); keyDrop(&output);
    static xrttypeops emptyOutputOps;
    static XL6_NativeLeafValueProfileV1 emptyOutputProfile;
    emptyOutputType=*profile->Type(); emptyOutputOps=*emptyOutputType.Ops;
    emptyOutputOps.Copy=copyIntoEmpty; emptyOutputType.Ops=&emptyOutputOps;
    emptyOutputProfile=*profile; emptyOutputProfile.Type=emptyOutputTypeGet;
    assert(xl6NativeLeafValueCopyV1(&emptyOutputProfile,&key,&output));
    assert(!xl6NativeLeafValueCopyV1(&emptyOutputProfile,&key,&output) && xl6SlotMapKeyEqualV1(key,output));
    xrtClearError(); keyDrop(&output);
    assert(!xl6NativeLeafValueCopyV1(profile,&empty,&empty) && !empty.instance && !empty.slot); xrtClearError();
    /* Alignment rejection must precede the profile's typed Empty/Copy loads. */
    _Alignas(max_align_t) unsigned char raw[sizeof(XL6_SlotMapKeyV1)+_Alignof(max_align_t)]={0};
    memcpy(raw+1,&key,sizeof(key));
    assert(!xl6NativeLeafValueCopyV1(profile,raw+1,&output) && !output.instance && !output.slot); xrtClearError();
    memset(raw,0,sizeof(raw));
    assert(!xl6NativeLeafValueCopyV1(profile,&key,raw+1)); xrtClearError();
    for (size_t i=0;i<sizeof(raw);++i) assert(!raw[i]);
    assert(!xl6NativeLeafValueBoxV1(profile,raw+1)); xrtClearError();
    XL6_NativeLeafValueProfileV1 bad=*profile; bad.Type=rejectedTypeGet;
    for (unsigned i=0;i<6;++i) {
        rejectedType=*profile->Type(); xrttypeops operations=*rejectedType.Ops;
        if (i==0) rejectedType.Align=3;
        if (i==1) ++rejectedType.Size;
        if (i==2) rejectedType.Flags|=XRT_TYPE_FLAG_REFERENCE;
        if (i==3) rejectedType.Id=0;
        if (i==4) { operations.Copy=NULL; rejectedType.Ops=&operations; }
        if (i==5) { operations.Drop=NULL; rejectedType.Ops=&operations; }
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* incoming=xrtGetError();
        assert(!xl6NativeLeafValueProfileValidV1(&bad) && xrtGetError()==incoming); xrtClearError();
        assert(!xl6NativeLeafValueCopyV1(&bad,&key,&output) && !output.instance && !output.slot); xrtClearError();
    }
    for (int failure=-2;failure<=2;++failure) if (failure) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure;
        assert(!xl6NativeLeafValueCopyV1(profile,&key,&output) && !output.instance && !output.slot && !copyFailure);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    for (int failure=-1;failure<=1;failure+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6NativeLeafValueCopyV1(profile,&key,&output) && !output.instance && !output.slot && !entryFailure);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_MEMORY); xrtErrorFree(result.error);
    }
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* incoming=xrtGetError();
    assert(xl6NativeLeafValueProfileValidV1(profile) && xrtGetError()==incoming);
    assert(xl6NativeLeafValueCopyV1(profile,&key,&output) && xrtGetError()==incoming);
    keyDrop(&output); xrtClearError();
}
static void vectors(void)
{
    XL6_SlotMapKeyV1 key={0},otherKey={0},output={0}; xvalue* map=create(&key),*other=create(&otherKey);
    directCopyVectors(key);
    xvalue* a=xl6SlotMapKeyBoxV1(key),*b=xl6SlotMapKeyBoxV1(key),*c=xl6SlotMapKeyBoxV1(otherKey); assert(a && b && c && a!=b);
    assert(xrtValueTypeId(a)==XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID);
    assert(xl6SlotMapKeyValueAdmitsV1(a) && xl6SlotMapKeyValueAdmitsV1(b));
    assert(xrtValueEqual(a,b) && !xrtValueEqual(a,c)); uint64 ah=0,bh=0;
    assert(xrtValueHash(a,&ah) && xrtValueHash(b,&bh) && ah==bh);
    assert(xl6NativeLeafValueIsV1(a));
    const xtypedsetkeypolicy* erasedPolicy=xl6SetReferenceKeyPolicyV1();
    xtypedset* erased=xrtTypedSetCreateWithKeyPolicy(xl6SequenceStorageTypeV1(3,xrtTypeValue()),erasedPolicy);
    assert(erased && xrtTypedSetAdd(erased,&a) && xrtTypedSetAdd(erased,&b) && xrtTypedSetCount(erased)==1);
    assert(xrtTypedSetAdd(erased,&c) && xrtTypedSetCount(erased)==2);
    assert(erasedPolicy->Hash(&a,NULL)==erasedPolicy->Hash(&b,NULL) && erasedPolicy->Equal(&a,&b,NULL));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtTypedSetHas(erased,&b) && !xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xvalue* alias=xl6NativeLeafValueBoxV1(&aliasProfile,&key); assert(alias && xl6NativeLeafValueIsV1(alias));
    assert(!xl6SlotMapKeyValueAdmitsV1(alias) && !xrtValueEqual(a,alias));
    assert(!xl6SlotMapKeyUnboxV1(alias,&output) && !output.instance); xrtClearError();
    assert(!xl6SlotMapKeyValueOwnershipAdapterV1(xrtValueOwnership(alias)));
    assert(xrtTypedSetAdd(erased,&alias) && xrtTypedSetCount(erased)==3);
    assert(!xl6NativeLeafValueBoxV1(NULL,&key)); xrtClearError();
    /* XRT includes the Value tag in the boxed hash. The typed
     * key policy deliberately hashes the native key, not that outer shell. */
    const xtypedsetkeypolicy* policy=xl6SlotMapKeyContentPolicyV1();
    assert(policy->Hash(&key,policy->UserData)==xl6SlotMapKeyHashV1(key));
    assert(xrtMemDebugFailAfter(0));
    assert(xl6SlotMapKeyUnboxV1(a,&output) && xl6SlotMapKeyEqualV1(output,key));
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(!xl6SlotMapKeyUnboxV1(b,&output)); xrtClearError(); keyDrop(&output);
    assert(!xl6NativeLeafValueUnboxV1(a,xl6SlotMapKeyValueProfileV1Get(),
        (unsigned char*)&output+1)); xrtClearError();
    assert(!output.instance && !output.slot);
    for (int failure=-1;failure<=1;failure+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); releaseFailure=failure;
        assert(!xl6SlotMapKeyUnboxV1(a,&output) && !output.instance && !output.slot && !releaseFailure);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    xvalue* clone=xrtValueClone(a); assert(clone && xrtValueEqual(a,clone));
    assert(xrtValueToTyped(clone,xl6SlotMapKeyTypeV1(),&output,xl6SlotMapKeyConverterV1()));
    assert(xl6SlotMapKeyEqualV1(key,output)); keyDrop(&output);
    xvalue* converted=xrtValueFromTyped(xl6SlotMapKeyTypeV1(),&key,xl6SlotMapKeyConverterV1());
    assert(converted && xrtValueEqual(a,converted));
    xtypedset* set=xrtTypedSetCreateWithKeyPolicy(xl6SlotMapKeyTypeV1(),xl6SlotMapKeyContentPolicyV1()); assert(set);
    assert(xrtTypedSetAdd(set,&key) && xrtTypedSetAdd(set,&key) && xrtTypedSetAdd(set,&otherKey));
    assert(xrtTypedSetCount(set)==2 && xrtTypedSetHas(set,&key));
    ptr raw=(ptr)(uintptr_t)17; xvalue* forged=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(forged);
    assert(xrtValueTypeIdBind(forged,xrtValueTypeId(a)));
    assert(!xl6NativeLeafValueIsV1(forged) && !erasedPolicy->Equal(&a,&forged,NULL));
    assert(xrtTypedSetAdd(erased,&forged) && xrtTypedSetHas(erased,&forged) && xrtTypedSetCount(erased)==4);
    assert(!xl6SlotMapKeyValueAdmitsV1(forged) && !xl6SlotMapKeyValueAdmitsV1(xrtValueNull()));
    assert(!xl6SlotMapKeyUnboxV1(forged,&output) && !output.instance && !output.slot); xrtClearError();
    assert(!xl6SlotMapKeyValueOwnershipAdapterV1(xrtValueOwnership(forged)));
    xrtownershipref ref=xrtValueOwnership(a); size_t count=0;
    assert(ref.Ops->Trace(ref.Data,edge,&count) && !count);
    assert(xl6SlotMapKeyValueOwnershipAdapterV1(ref));
    for (int failure=-2;failure<=2;++failure) if (failure) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure;
        assert(!xl6SlotMapKeyUnboxV1(a,&output) && !output.instance && !output.slot && !copyFailure);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
        assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure;
        assert(!xl6SlotMapKeyBoxV1(key) && !copyFailure); result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
        assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure;
        assert(!xrtValueToTyped(a,xl6SlotMapKeyTypeV1(),&output,xl6SlotMapKeyConverterV1()) &&
            !output.instance && !output.slot && !copyFailure);
        result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
        assert(xl6ErrorScopeBegin(&parent)); copyFailure=failure;
        assert(!xrtValueFromTyped(xl6SlotMapKeyTypeV1(),&key,xl6SlotMapKeyConverterV1()) && !copyFailure);
        result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    for (int failure=-1;failure<=1;failure+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); bindFailure=failure;
        assert(!xl6SlotMapKeyBoxV1(key) && !bindFailure); xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
        assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6SlotMapKeyUnboxV1(a,&output) && !entryFailure); result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(failure<0 ? !result.error : xrtErrorKind(result.error)==XERR_MEMORY); xrtErrorFree(result.error);
    }
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* incoming=xrtGetError();
    assert(xl6SlotMapKeyValueAdmitsV1(a) && !xl6SlotMapKeyValueAdmitsV1(forged) && xrtGetError()==incoming);
    assert(xl6SlotMapKeyUnboxV1(a,&output) && xrtGetError()==incoming); keyDrop(&output); xrtClearError();
    xrtValueRelease(map); xrtValueRelease(other); keyDrop(&key); keyDrop(&otherKey);
    assert(xl6SlotMapKeyUnboxV1(a,&output) && xrtTypedSetHas(set,&output) && xrtTypedSetHas(erased,&b)); keyDrop(&output);
    xvalue* empty=xl6SlotMapKeyBoxV1((XL6_SlotMapKeyV1){0}); assert(empty);
    assert(xl6SlotMapKeyUnboxV1(empty,&output) && !output.instance && !output.slot);
    assert(!xrtValueEqual(a,empty)); xrtValueRelease(empty);
    xrtTypedSetDestroy(erased); xrtValueRelease(alias); xrtTypedSetDestroy(set); xrtValueRelease(forged); xrtValueRelease(converted); xrtValueRelease(clone);
    xrtValueRelease(c); xrtValueRelease(b); xrtValueRelease(a);
}
static bool fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_SlotMapKeyV1 key={0},output={0}; xvalue* map=create(&key);
    xvalue* box=xl6SlotMapKeyBoxV1(key); assert(box); xvalue* result=NULL;
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); assert(xrtMemDebugFailAfter(point));
    bool success=false;
    if (!strcmp(operation,"box")) { result=xl6SlotMapKeyBoxV1(key); success=result!=NULL; }
    else if (!strcmp(operation,"copy")) success=xl6NativeLeafValueCopyV1(xl6SlotMapKeyValueProfileV1Get(),&key,&output);
    else if (!strcmp(operation,"unbox")) success=xl6SlotMapKeyUnboxV1(box,&output);
    else if (!strcmp(operation,"encode")) { result=xrtValueFromTyped(xl6SlotMapKeyTypeV1(),&key,xl6SlotMapKeyConverterV1()); success=result!=NULL; }
    else if (!strcmp(operation,"decode")) success=xrtValueToTyped(box,xl6SlotMapKeyTypeV1(),&output,xl6SlotMapKeyConverterV1());
    else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if (hit) assert(!result && !output.instance && xrtErrorKind(outcome.error)==XERR_MEMORY);
    assert(xl6SlotMapKeyValueAdmitsV1(box));
    keyDrop(&output); keyDrop(&key); xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(box); xrtValueRelease(map);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); vectors(); assert(!xrtGetError() && !xl6ProductMemoryEnd());
        xl6ProductMemoryVerify(); puts("[PASS] immutable SlotMap key projection and typed content capability"); return 0;
    }
    assert(!strcmp(argv[1],"prefixes")); const char* operations[]={"box","unbox","encode","decode","copy"};
    for (size_t i=0;i<sizeof(operations)/sizeof(*operations);++i) {
        size_t point=0; while (point<4096 && fault(operations[i],point)) ++point; assert(point<4096);
        printf("slot-key %s runs=%zu first-no-hit=%zu\n",operations[i],point+1,point);
    }
    return 0;
}
