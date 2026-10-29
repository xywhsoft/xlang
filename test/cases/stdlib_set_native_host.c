#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_set_key_policy.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { SET=0xD710,DOMAIN=0xD711,ELEMENT=0xD712,CURSOR=0xD713 };
static int entryFailure,cleanupFailure,entrySkip;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (entryFailure && entrySkip>0) { --entrySkip; return __real_xl6ErrorScopeBegin(scope); }
    if (entryFailure) { int fail=entryFailure; entryFailure=0;
        if (fail>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError(); return false; }
    return __real_xl6ErrorScopeBegin(scope);
}
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failure)
{
    if (cleanupFailure) { int fail=cleanupFailure; cleanupFailure=0;
        if (fail>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError(); return false; }
    return __real_xl6ErrorScopeBeginCleanup(scope,failure);
}
static XL6_SetLayoutV1 layout(unsigned transfer,const xrttype* type)
{ return (XL6_SetLayoutV1){SET,{DOMAIN,ELEMENT,transfer,type->Size,type}}; }
static xvalue* seed(const XL6_SetLayoutV1* tuple,unsigned rule)
{
    const XL6_SequenceLayoutV1* v=&tuple->value;
    if (v->transfer==3) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,v->identity,v->elementIdentity,rule,false,v->elementIdentity);
    if (v->transfer==4) return xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,v->identity,v->elementIdentity,v->elementIdentity,false);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,v->identity,v->elementIdentity,v->transfer,v->size,v->type,rule,false,v->elementIdentity);
}
static xvalue* create(const XL6_SetLayoutV1* tuple,unsigned rule)
{
    xl6_error_scope scope={0}; if (!xl6ErrorScopeBegin(&scope)) return NULL;
    xvalue* witness=seed(tuple,rule);
    if (!witness) xl6ErrorScopeCaptureFailure(&scope);
    xvalue* result=xl6SetCreateTakeV1(tuple,witness);
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
static void add(xvalue* set,const XL6_SetLayoutV1* tuple,const void* item,bool expected)
{ bool added=!expected; assert(xl6SetAddV1(set,tuple,item,&added) && added==expected); }
static void equalSets(xvalue* a,xvalue* b,const XL6_SetLayoutV1* tuple,bool expected)
{
    bool equal=!expected; assert(xl6SetEqualsV1(a,b,tuple,&equal) && equal==expected);
    if (expected) { uint64_t x=0,y=1; assert(xl6SetHashV1(a,tuple,&x) && xl6SetHashV1(b,tuple,&y) && x==y); }
}
static void integers(void)
{
    XL6_SetLayoutV1 tuple=layout(1,xrtTypeInt64()); xvalue* a=create(&tuple,XL6_VALUE_ADMIT_I64),*b=create(&tuple,XL6_VALUE_ADMIT_I64); assert(a && b);
    const int64_t values[]={INT64_MIN,-9,0,42,INT64_MAX};
    for (unsigned i=0;i<5;++i) add(a,&tuple,&values[i],true);
    for (unsigned i=5;i-->0;) add(b,&tuple,&values[i],true);
    for (unsigned i=0;i<5;++i) add(a,&tuple,&values[i],false);
    equalSets(a,b,&tuple,true); assert(xrtValueEqual(a,b) && !xrtGetError());
    uint64 x=0,y=1; assert(xrtValueHash(a,&x) && xrtValueHash(b,&y) && x==y);
    bool found=false; int64_t output=99,missing=8;
    assert(xl6SetGetV1(a,&tuple,&missing,&output,false,&found) && !found && output==99);
    assert(xl6SetGetV1(a,&tuple,&values[0],&output,false,&found) && found && output==INT64_MIN);
    assert(xl6SetReserveV1(a,&tuple,50) && xl6SetCountV1(a,&tuple,true)>=50);
    assert(!xl6SetReserveV1(a,&tuple,-1)); xrtClearError();
    xvalue* copy=xl6SetCloneV1(a,&tuple); assert(copy); equalSets(copy,a,&tuple,true);
    assert(xl6SetGetV1(copy,&tuple,&values[0],&output,true,&found) && found && output==INT64_MIN);
    assert(xl6SetCountV1(copy,&tuple,false)==4); equalSets(copy,a,&tuple,false);
    for (unsigned op=1;op<=4;++op) {
        xvalue* result=xl6SetAlgebraV1(a,copy,&tuple,(XL6_SetAlgebraV1)op); assert(result);
        assert(xl6SetCountV1(result,&tuple,false)==(op==1 ? 5 : op==2 ? 4 : 1)); xrtValueRelease(result);
    }
    assert(xl6SetMergeV1(copy,a,&tuple) && xl6SetMergeV1(copy,copy,&tuple)); equalSets(copy,a,&tuple,true);
    assert(xl6SetClearV1(copy,&tuple) && xl6SetTrimV1(copy,&tuple) && xl6SetCountV1(copy,&tuple,false)==0);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6SetCountV1(a,&tuple,false)==5 && xl6SetHashV1(a,&tuple,&x) && xrtGetError()==prior);
    xrtClearError(); xrtValueRelease(copy); xrtValueRelease(b); xrtValueRelease(a);
}
static void profiles(void)
{
    XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1()); xvalue* set=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(set);
    char text[]={'a',0,(char)0xe4,(char)0xbd,(char)0xa0}; xstrview item={text,sizeof(text)},out={0};
    add(set,&tuple,&item,true); add(set,&tuple,&item,false); text[0]='z'; add(set,&tuple,&item,true);
    xstrview query={"a\0\xe4\xbd\xa0",5}; bool found=false;
    assert(xl6SetGetV1(set,&tuple,&query,&out,false,&found) && found && out.Size==5 && !memcmp(out.Data,query.Data,5));
    assert(xl6SetClearV1(set,&tuple) && out.Data[0]=='a'); xrtTypeDropValue(tuple.value.type,&out); xrtValueRelease(set);
    tuple=layout(1,xl6CharTypeV1()); set=create(&tuple,XL6_VALUE_ADMIT_CHAR); assert(set);
    uint32_t ch=0x4f60; add(set,&tuple,&ch,true); bool added=true; ch=0xd800;
    assert(!xl6SetAddV1(set,&tuple,&ch,&added) && added); xrtClearError(); ch=0x110000;
    assert(!xl6SetAddV1(set,&tuple,&ch,&added)); xrtClearError(); xrtValueRelease(set);
    for (unsigned rule=XL6_VALUE_ADMIT_I8;rule<=XL6_VALUE_ADMIT_U32;++rule) {
        if (rule==XL6_VALUE_ADMIT_I64) continue;
        tuple=layout(1,xrtTypeInt64()); set=create(&tuple,rule); assert(set);
        int64_t zero=0; add(set,&tuple,&zero,true);
        int64_t bad=rule>=XL6_VALUE_ADMIT_U8 ? -1 : INT64_MAX;
        assert(!xl6SetAddV1(set,&tuple,&bad,&added)); xrtClearError(); assert(xl6SetCountV1(set,&tuple,false)==1);
        xvalue* different=create(&tuple,rule==XL6_VALUE_ADMIT_I8 ? XL6_VALUE_ADMIT_I16 : XL6_VALUE_ADMIT_I8); assert(different);
        bool equal=true; assert(!xl6SetEqualsV1(set,different,&tuple,&equal) && equal); xrtClearError();
        xrtValueRelease(different); xrtValueRelease(set);
    }
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,DOMAIN,ELEMENT+1,2,tuple.value.size,tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT);
    assert(witness); set=xl6SetCreateTakeV1(&tuple,witness); assert(set);
    xstrview absent={0},empty={"",0}; add(set,&tuple,&absent,true); add(set,&tuple,&empty,true);
    assert(xl6SetCountV1(set,&tuple,false)==2); xvalue* copy=xl6SetCloneV1(set,&tuple); assert(copy); equalSets(set,copy,&tuple,true);
    absent.Size=1; assert(!xl6SetAddV1(set,&tuple,&absent,&added)); xrtClearError();
    xrtValueRelease(copy); xrtValueRelease(set);
}
static void cursors(void)
{
    XL6_SetLayoutV1 tuple=layout(1,xrtTypeInt64());
    for (unsigned reverse=0;reverse<2;++reverse) {
        xvalue* set=create(&tuple,XL6_VALUE_ADMIT_I64); assert(set);
        const int64_t values[]={7,-8,INT64_MAX}; for (unsigned i=0;i<3;++i) add(set,&tuple,&values[i],true);
        xvalue* cursor=xl6SetCursorCreateV1(set,&tuple,CURSOR,reverse); assert(cursor);
        assert(xl6SetClearV1(set,&tuple)); xrtValueRelease(set);
        bool found=false; int64_t value=99;
        assert(!xl6SetCursorAdvanceV1(cursor,&tuple,CURSOR+1,&found)); xrtClearError();
        for (unsigned i=0;i<3;++i) {
            assert(xl6SetCursorAdvanceV1(cursor,&tuple,CURSOR,&found) && found);
            assert(xl6SetCursorReadV1(cursor,&tuple,CURSOR,&value) && value==values[reverse ? 2-i : i]);
        }
        assert(xl6SetCursorAdvanceV1(cursor,&tuple,CURSOR,&found) && !found);
        assert(xl6SetCursorAdvanceV1(cursor,&tuple,CURSOR,&found) && !found);
        assert(!xl6SetCursorReadV1(cursor,&tuple,CURSOR,&value)); xrtClearError();
        found=true; assert(!xl6SetCursorAdvanceV1(cursor,&tuple,CURSOR,&found) && found); xrtClearError(); xrtValueRelease(cursor);
    }
    xvalue* set=create(&tuple,XL6_VALUE_ADMIT_I64); assert(set); add(set,&tuple,&(int64_t){1},true);
    xvalue* cursor=xl6SetCursorCreateV1(set,&tuple,CURSOR,false); assert(cursor); bool found=true;
    int64_t unchanged=99; assert(!xl6SetCursorReadV1(cursor,&tuple,CURSOR,&unchanged) && unchanged==99); xrtClearError();
    assert(!xl6SetCursorAdvanceV1(cursor,&tuple,CURSOR,&found) && found); xrtClearError();
    xrtValueRelease(cursor); xrtValueRelease(set);
}
static void optionalProfiles(void)
{
    /* Absence is independent of the present payload, including present zero.
     * Actual immutable physical descriptors, never test-provided type ops. */
#define OPTIONAL(CARRIER,GETTER,RULE,VALUE) do { \
    XL6_SetLayoutV1 tuple=layout(1,GETTER()); tuple.value.elementIdentity=ELEMENT+1; \
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,DOMAIN,ELEMENT+1,1,sizeof(CARRIER),tuple.value.type,RULE,true,ELEMENT); \
    assert(witness); xvalue* set=xl6SetCreateTakeV1(&tuple,witness); assert(set); \
    CARRIER absent={0},present={.hasValue=true,.value=VALUE},out={0}; \
    add(set,&tuple,&absent,true); add(set,&tuple,&present,true); add(set,&tuple,&present,false); \
    bool found=false; assert(xl6SetGetV1(set,&tuple,&present,&out,false,&found) && found && out.hasValue && out.value==present.value); \
    xvalue* copy=xl6SetCloneV1(set,&tuple); assert(copy); equalSets(set,copy,&tuple,true); \
    assert(xl6SetCountV1(set,&tuple,false)==2); xrtValueRelease(copy); xrtValueRelease(set); \
} while (0)
    OPTIONAL(XL6_NullableBoolV1,xl6NullableBoolTypeV1,XL6_VALUE_ADMIT_BOOL,false);
    OPTIONAL(XL6_NullableI64V1,xl6NullableI64TypeV1,XL6_VALUE_ADMIT_I64,INT64_MIN);
    OPTIONAL(XL6_NullableU64V1,xl6NullableU64TypeV1,XL6_VALUE_ADMIT_U64,UINT64_MAX);
    OPTIONAL(XL6_NullableF32V1,xl6NullableF32TypeV1,XL6_VALUE_ADMIT_F32,0.0f);
    OPTIONAL(XL6_NullableF64V1,xl6NullableF64TypeV1,XL6_VALUE_ADMIT_F64,-0.0);
    OPTIONAL(XL6_NullableTimeV1,xl6NullableTimeTypeV1,XL6_VALUE_ADMIT_TIME,INT64_MIN);
    OPTIONAL(XL6_NullableCharV1,xl6NullableCharTypeV1,XL6_VALUE_ADMIT_CHAR,0x10ffff);
#undef OPTIONAL
#define NATIVE(CARRIER,GETTER,TYPE,VALUE) do { \
    XL6_SetLayoutV1 tuple=layout(1,GETTER()); tuple.value.elementIdentity=ELEMENT+1; \
    xvalue* witness=xl6SequenceNullableCScalarCreateV1(1,DOMAIN,ELEMENT+1,tuple.value.type,TYPE(),ELEMENT); \
    assert(witness); xvalue* set=xl6SetCreateTakeV1(&tuple,witness); assert(set); \
    CARRIER absent={0},present={.hasValue=true,.value=VALUE}; add(set,&tuple,&absent,true); add(set,&tuple,&present,true); \
    xvalue* copy=xl6SetCloneV1(set,&tuple); assert(copy); equalSets(set,copy,&tuple,true); \
    assert(xl6SetCountV1(set,&tuple,false)==2); xrtValueRelease(copy); xrtValueRelease(set); \
} while (0)
    NATIVE(XL6_NullableNativeI8V1,xl6NullableNativeI8TypeV1,xrtTypeInt8,INT8_MIN);
    NATIVE(XL6_NullableNativeU8V1,xl6NullableNativeU8TypeV1,xrtTypeUInt8,UINT8_MAX);
    NATIVE(XL6_NullableNativeI16V1,xl6NullableNativeI16TypeV1,xrtTypeInt16,INT16_MIN);
    NATIVE(XL6_NullableNativeU16V1,xl6NullableNativeU16TypeV1,xrtTypeUInt16,UINT16_MAX);
    NATIVE(XL6_NullableNativeI32V1,xl6NullableNativeI32TypeV1,xrtTypeInt32,INT32_MIN);
    NATIVE(XL6_NullableNativeU32V1,xl6NullableNativeU32TypeV1,xrtTypeUInt32,UINT32_MAX);
#undef NATIVE
}
static void references(void)
{
    XL6_SetLayoutV1 tuple=layout(3,xrtTypeValue()); xvalue* set=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(set);
    xvalue* a=xrtValueArray(),*b=xrtValueArray(),*integer=xrtValueInt(1); assert(a && b && integer);
    add(set,&tuple,&a,true); add(set,&tuple,&b,true); add(set,&tuple,&a,false);
    assert(xrtValueArrayAppend(a,integer)); bool found=false;
    assert(xl6SetKeyV1(set,&tuple,&a,false,&found) && found);
    xvalue* text=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3});
    xvalue* same=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3}); assert(text && same);
    add(set,&tuple,&text,true); add(set,&tuple,&same,false);
    xvalue* absent=NULL,*nullValue=xrtValueNull(); add(set,&tuple,&absent,true); add(set,&tuple,&nullValue,false);
    xvalue* out=NULL; assert(xl6SetGetV1(set,&tuple,&same,&out,false,&found) && found && out==text); xrtValueRelease(out);
    xvalue* copy=xl6SetCloneV1(set,&tuple); assert(copy); equalSets(copy,set,&tuple,true);
    assert(xrtValueArrayAppend(b,integer)); equalSets(copy,set,&tuple,true);
    xrtValueRelease(copy); xrtValueRelease(same); xrtValueRelease(text); xrtValueRelease(integer);
    xrtValueRelease(b); xrtValueRelease(a); xrtValueRelease(set);
}
typedef struct Edges { size_t count; xrtownershipref last; } Edges;
static bool edge(xrtownershipref ref,void* opaque)
{ Edges* edges=opaque; ++edges->count; edges->last=ref; return true; }
static xrtownershipref ownerOf(xvalue* value)
{ xrtownershipref shell=xrtValueOwnership(value); Edges edges={0}; assert(shell.Ops->Trace(shell.Data,edge,&edges) && edges.count==1); return edges.last; }
static void graph(void)
{
    XL6_SetLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* a=create(&tuple,XL6_VALUE_ADMIT_ANY),*b=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(a && b);
    add(a,&tuple,&a,true); add(a,&tuple,&b,true); add(b,&tuple,&a,true);
    uint64_t hash=0; assert(xl6SetHashV1(a,&tuple,&hash));
    xrtownershipref oa=ownerOf(a),ob=ownerOf(b); Edges edges={0};
    assert(oa.Ops->Trace(oa.Data,edge,&edges) && edges.count==2);
    const xrtownershipadapterv1* aa=xl6SetOwnershipAdapterV1(oa),*ab=xl6SetOwnershipAdapterV1(ob); assert(aa && ab);
    unsigned token=0; xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(aa->Hold(oa.Data) && ab->Hold(ob.Data) && aa->Claim(oa.Data,&token) && ab->Claim(ob.Data,&token));
    aa->Restore(oa.Data,&token); assert(aa->Claim(oa.Data,&token));
    aa->Clear(oa.Data,&token); ab->Clear(ob.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); cleanupFailure=-1;
    assert(!aa->Finish(oa.Data,&token)); xl6ErrorScopeCaptureFailure(&scope);
    assert(aa->Finish(oa.Data,&token) && aa->Finish(oa.Data,&token) && ab->Finish(ob.Data,&token));
    xl6_error_result result=xl6ErrorScopeEnd(&scope); assert(!result.ok && !result.error);
    aa->Drop(oa.Data); ab->Drop(ob.Data); xrtValueRelease(b); xrtValueRelease(a);
}
static void futures(void)
{
    XL6_SetLayoutV1 tuple=layout(4,xrtTypeFuture()); xvalue* set=create(&tuple,0); assert(set);
    xfuture* a=NULL,*b=NULL,*out=NULL; xpromise* pa=xrtPromiseCreate(&a,NULL),*pb=xrtPromiseCreate(&b,NULL); assert(pa && pb);
    add(set,&tuple,&a,true); add(set,&tuple,&a,false); add(set,&tuple,&b,true);
    uint64_t before=0,after=1; assert(xl6SetHashV1(set,&tuple,&before));
    assert(xrtPromiseResolve(pa,NULL) && xrtPromiseResolve(pb,NULL) && xl6SetHashV1(set,&tuple,&after) && after==before);
    bool found=false; assert(xl6SetGetV1(set,&tuple,&a,&out,false,&found) && found && out==a); xrtFutureDestroy(out);
    xrtownershipref owner=ownerOf(set); Edges edges={0}; assert(owner.Ops->Trace(owner.Data,edge,&edges) && edges.count==2);
    const XL6_SequenceAdmissionOpsV2* allowed[]={xl6ArrayFutureAdmissionOpsV2Get()};
    assert(!xl6SetOwnershipAdapterV1(owner) && xl6SetOwnershipAdapterV2(owner,allowed,1));
    xrtValueRelease(set); xrtPromiseDestroy(pb); xrtPromiseDestroy(pa); xrtFutureDestroy(b); xrtFutureDestroy(a);
}
static void rejected(void)
{
    XL6_SetLayoutV1 tuple=layout(1,xrtTypeInt64()); xvalue* set=create(&tuple,XL6_VALUE_ADMIT_I64); assert(set);
    XL6_SetLayoutV1 copied={0}; XL6_SequenceAdmissionV3 policy={0}; assert(xl6SetSchemaCopyV1(set,&copied,&policy));
    assert(xl6SetAdmitsSchemaV1(set,&copied,&policy)); copied.value.elementIdentity++;
    assert(!xl6SetAdmitsSchemaV1(set,&copied,&policy) && !xrtGetError());
    bool added=true; assert(!xl6SetAddV1(set,&copied,&(int64_t){1},&added) && added); xrtClearError();
    copied=tuple; copied.identity++;
    assert(!xl6SetAddV1(set,&copied,&(int64_t){1},&added)); xrtClearError();
    XL6_SequenceAdmissionV3 unchanged=policy; assert(!xl6SetSchemaCopyV1(set,&copied,&policy)); xrtClearError(); assert(!memcmp(&policy,&unchanged,sizeof(policy)));
    xl6SequenceAdmissionDropV3(&policy);
    assert(!xl6SetCreateTakeV1(&tuple,xrtValueArray())); xrtClearError();
    xvalue* witness=seed(&tuple,XL6_VALUE_ADMIT_I64); assert(witness);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(witness,1,DOMAIN,ELEMENT,1,sizeof(int64_t),tuple.value.type); assert(pin);
    assert(xrtTypedArrayPush(xl6SequenceStorageV1(pin),&(int64_t){1})); xl6SequenceEndV1(pin);
    assert(!xl6SetCreateTakeV1(&tuple,witness)); xrtClearError();
    for (int fail=-1;fail<=1;fail+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=fail;
        uint64_t hash=99; assert(!xl6SetHashV1(set,&tuple,&hash) && hash==99);
        xl6ErrorScopeCaptureFailure(&parent); xl6_error_result result=xl6ErrorScopeEnd(&parent);
        assert(!result.ok && (fail<0 ? !result.error : result.error!=NULL)); xrtErrorFree(result.error);
    }
    xrtValueRelease(set);
}
typedef struct DropState { xvalue* set; XL6_SetLayoutV1 tuple; unsigned drops; bool absent; } DropState;
static bool retainOpaque(ptr item,ptr* out,ptr context) { (void)context; *out=item; return true; }
static void reentrantDrop(ptr item,ptr context)
{
    DropState* state=item; assert(state==context); ++state->drops;
    if (state->absent) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    assert(xl6SetCountV1(state->set,&state->tuple,false)==-1);
    xrtValueRelease(state->set); state->set=NULL;
}
static const xvaluehandleops reentrantOps={retainOpaque,reentrantDrop,NULL,NULL};
static void cleanup(void)
{
    for (unsigned absent=0;absent<2;++absent) {
        DropState state={.tuple=layout(3,xrtTypeValue()),.absent=absent}; state.set=create(&state.tuple,XL6_VALUE_ADMIT_ANY); assert(state.set);
        ptr raw=&state; xvalue* item=xrtValueHandleTake(&raw,&reentrantOps,&state); assert(item);
        add(state.set,&state.tuple,&item,true); xrtValueRelease(item);
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
        assert(!xl6SetClearV1(state.set,&state.tuple));
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && state.drops==1 && !state.set);
        assert(absent ? !result.error : result.error && xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1());
    for (int fail=-1;fail<=1;fail+=2) {
        xvalue* set=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(set); add(set,&tuple,&(xstrview){"a\0b",3},true);
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); cleanupFailure=fail;
        xrtValueRelease(set); assert(!cleanupFailure);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok);
        assert(fail<0 ? !result.error : result.error && xrtErrorKind(result.error)==XERR_MEMORY); xrtErrorFree(result.error);
    }
}
static xvalue* projectedCreate(const XL6_SetLayoutV1* tuple,unsigned rule,const xvalueconverter* codec)
{
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xvalue* raw=create(tuple,rule); if (!raw) xl6ErrorScopeCaptureFailure(&scope);
    xvalue* result=xl6SetProjectionTakeV1(raw,tuple,codec);
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
static void erasedMethods(void)
{
    XL6_SetLayoutV1 tuple=layout(1,xrtTypeInt64()); xvalue* set=projectedCreate(&tuple,XL6_VALUE_ADMIT_I64,NULL); assert(set);
    xvalue* item=xrtValueInt(INT64_MIN),*other=xrtValueInt(INT64_MAX); assert(item && other);
    bool result=false; int64_t count=-1;
    assert(xl6SetErasedKeyV1(set,item,XL6_SET_ADD_V1,&result) && result);
    assert(xl6SetErasedKeyV1(set,item,XL6_SET_ADD_V1,&result) && !result);
    assert(xl6SetErasedKeyV1(set,other,XL6_SET_HAS_V1,&result) && !result);
    assert(xl6SetErasedReserveV1(set,30) && xl6SetErasedCountV1(set,true,&count) && count>=30);
    xvalue* read=NULL; assert(xl6SetErasedReadV1(set,item,false,&read,&result) && result && read);
    int64_t number=0; assert(xrtValueGetInt(read,&number) && number==INT64_MIN && xrtValueTypeId(read)==ELEMENT); xrtValueRelease(read); read=NULL;
    assert(xl6SetErasedReadV1(set,other,false,&read,&result) && !result && !read);
    xvalue* copy=xl6SetErasedCloneV1(set); assert(copy);
    assert(xl6SetErasedRelationV1(set,copy,XL6_SET_EQUALS_V1,false,&result) && result);
    assert(xl6SetErasedRelationV1(set,copy,XL6_SET_SUBSET_V1,true,&result) && !result);
    assert(xl6SetErasedKeyV1(copy,other,XL6_SET_ADD_V1,&result) && result);
    assert(xl6SetErasedRelationV1(set,copy,XL6_SET_SUBSET_V1,true,&result) && result);
    assert(xl6SetErasedRelationV1(copy,set,XL6_SET_SUPERSET_V1,false,&result) && result);
    assert(xl6SetErasedRelationV1(copy,set,XL6_SET_DISJOINT_V1,false,&result) && !result);
    uint64_t hash=0; assert(xl6SetErasedHashV1(set,&hash));
    for (unsigned op=1;op<=4;++op) {
        xvalue* combined=xl6SetErasedAlgebraV1(copy,set,(XL6_SetAlgebraV1)op); assert(combined);
        assert(xl6SetErasedCountV1(combined,false,&count) && count==(op==1 ? 2 : 1)); xrtValueRelease(combined);
    }
    assert(xl6SetErasedMergeV1(set,copy) && xl6SetErasedMergeV1(set,set));
    assert(xl6SetErasedReadV1(set,item,true,&read,&result) && result && read); xrtValueRelease(read);
    assert(xl6SetErasedKeyV1(set,item,XL6_SET_REMOVE_V1,&result) && !result);
    assert(xl6SetErasedClearV1(set) && xl6SetErasedTrimV1(set));
    xrtValueRelease(copy); xrtValueRelease(other); xrtValueRelease(item); xrtValueRelease(set);
    /* Same physical descriptor and owner IDs do not grant another rule. */
    set=projectedCreate(&tuple,XL6_VALUE_ADMIT_I8,NULL); assert(set); item=xrtValueInt(128); result=true;
    assert(!xl6SetErasedKeyV1(set,item,XL6_SET_ADD_V1,&result) && result); xrtClearError();
    xrtValueRelease(item); xrtValueRelease(set);
}
static void erasedReferences(void)
{
    XL6_SetLayoutV1 tuple=layout(3,xrtTypeValue()); xvalue* set=projectedCreate(&tuple,XL6_VALUE_ADMIT_ANY,NULL); assert(set);
    xvalue* item=xrtValueArray(),*same=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3}); assert(item && same);
    bool found=false; assert(xl6SetErasedKeyV1(set,item,XL6_SET_ADD_V1,&found) && found);
    assert(xrtValueArrayAppend(item,same)); assert(xl6SetErasedKeyV1(set,item,XL6_SET_HAS_V1,&found) && found);
    xvalue* out=NULL; assert(xl6SetErasedReadV1(set,item,false,&out,&found) && found && out==item); xrtValueRelease(out);
    assert(xl6SetErasedKeyV1(set,same,XL6_SET_ADD_V1,&found) && found);
    xvalue* twin=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3}); assert(twin);
    assert(xl6SetErasedKeyV1(set,twin,XL6_SET_ADD_V1,&found) && !found);
    xrtValueRelease(twin); xrtValueRelease(same); xrtValueRelease(item); xrtValueRelease(set);
    tuple=layout(4,xrtTypeFuture()); set=projectedCreate(&tuple,0,NULL); assert(set);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    item=xrtValueFuture(future); assert(item && xrtValueTypeIdBind(item,ELEMENT));
    assert(xl6SetErasedKeyV1(set,item,XL6_SET_ADD_V1,&found) && found);
    assert(xrtPromiseResolve(promise,NULL) && xl6SetErasedKeyV1(set,item,XL6_SET_HAS_V1,&found) && found);
    out=NULL; assert(xl6SetErasedReadV1(set,item,false,&out,&found) && found && xrtValueGetFuture(out)==future && xrtValueTypeId(out)==ELEMENT);
    xrtValueRelease(out); xrtValueRelease(item); xrtValueRelease(set); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
}
static xvalue* codecOwner; static unsigned codecMode;
static bool codecDecode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    assert(!context); bool ok=xl6StringConverterV1()->ToTyped(value,type,output,NULL);
    if (codecMode==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if (codecMode==3) {
        xl6_error_scope probe={0}; assert(xl6ErrorScopeBegin(&probe)); int64_t count=99;
        assert(!xl6SetErasedCountV1(codecOwner,false,&count) && count==99);
        xl6_error_result rejected=xl6ErrorScopeEnd(&probe);
        assert(!rejected.ok && rejected.error && xrtErrorKind(rejected.error)==XERR_STATE);
        xrtErrorFree(rejected.error); /* Consume the deliberately handled probe. */
    }
    return ok;
}
static xvalue* codecEncode(const xrttype* type,const void* slot,ptr context)
{
    assert(!context); xvalue* result=codecMode==4 ? xrtValueInt(0) : xl6StringConverterV1()->FromTyped(type,slot,NULL);
    if (codecMode==2) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return result;
}
static const xvalueconverter faultCodec={.ToTyped=codecDecode,.FromTyped=codecEncode};
static bool retainForeign(ptr item,ptr* output,ptr context) { (void)context; *output=item; return true; }
static void dropForeign(ptr item,ptr context) { (void)item; (void)context; }
static const xvaluehandleops foreignOps={retainForeign,dropForeign,NULL,NULL};
static void erasedErrors(void)
{
    XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1()); codecOwner=projectedCreate(&tuple,XL6_VALUE_ADMIT_STRING,&faultCodec); assert(codecOwner);
    xvalue* item=xrtValueString((xstrview){"a\0b",3}); assert(item); bool changed=true;
    for (unsigned lane=0;lane<2;++lane) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
        codecMode=lane ? 0 : 1; if (lane) { entrySkip=1; entryFailure=-1; }
        assert(!xl6SetErasedKeyV1(codecOwner,item,XL6_SET_ADD_V1,&changed) && changed);
        xl6ErrorScopeCaptureFailure(&parent); xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !result.error);
        int64_t count=-1; assert(xl6SetErasedCountV1(codecOwner,false,&count) && count==0); xrtClearError();
    }
    codecMode=3; assert(xl6SetErasedKeyV1(codecOwner,item,XL6_SET_ADD_V1,&changed) && changed); codecMode=2;
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); xvalue* out=NULL; bool found=true;
    assert(!xl6SetErasedReadV1(codecOwner,item,false,&out,&found) && !out && found);
    xl6ErrorScopeCaptureFailure(&parent); xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !result.error);
    codecMode=4; assert(!xl6SetErasedReadV1(codecOwner,item,true,&out,&found) && !out); xrtClearError();
    int64_t retained=-1; assert(xl6SetErasedCountV1(codecOwner,false,&retained) && retained==1);
    codecMode=0;
    xvalue* cursor=NULL; XL6_SetLayoutV1 cursorLayout={0}; uint64_t identity=0;
    assert(xl6SetIteratorSnapshotErasedV1(codecOwner,false,&cursor,&cursorLayout,&identity)==1);
    codecMode=2; assert(!xl6SetIteratorAdvanceValueV1(cursor,&cursorLayout,identity,&found,&out) && !out); xrtClearError(); codecMode=0;
    assert(!xl6SetIteratorAdvanceValueV1(cursor,&cursorLayout,identity,&found,&out)); xrtClearError(); xrtValueRelease(cursor);
    xrtValueRelease(item); xrtValueRelease(codecOwner); codecOwner=NULL;
    /* Foreign same-tag/type handles with an invalid payload must never be cast. */
    ptr raw=(ptr)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign && xrtValueTypeIdBind(foreign,SET));
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(!xl6SetErasedProbeV1(foreign) && xrtGetError()==prior); xrtClearError();
    int64_t count=99; assert(!xl6SetErasedCountV1(foreign,false,&count) && count==99); xrtClearError(); xrtValueRelease(foreign);
    xvalue* unbound=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(unbound && xl6SetErasedProbeV1(unbound)==1);
    assert(!xl6SetErasedCountV1(unbound,false,&count) && count==99); xrtClearError(); xrtValueRelease(unbound);
}
static void erasedIterator(void)
{
    XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1());
    for (unsigned reverse=0;reverse<2;++reverse) {
        xvalue* set=projectedCreate(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(set);
        xvalue* a=xrtValueString((xstrview){"a\0b",3}),*b=xrtValueString((xstrview){"z",1}); assert(a && b);
        bool found=false; assert(xl6SetErasedKeyV1(set,a,XL6_SET_ADD_V1,&found) && xl6SetErasedKeyV1(set,b,XL6_SET_ADD_V1,&found));
        xvalue* cursor=NULL; XL6_SetLayoutV1 schema={0}; uint64_t identity=0;
        assert(xl6SetIteratorSnapshotErasedV1(set,reverse,&cursor,&schema,&identity)==1);
        assert(xl6SetErasedClearV1(set)); xrtValueRelease(set);
        for (unsigned i=0;i<2;++i) { xvalue* out=NULL; assert(xl6SetIteratorAdvanceValueV1(cursor,&schema,identity,&found,&out) && found && out);
            assert(xrtValueEqual(out,((i+reverse)%2) ? b : a)); xrtValueRelease(out); }
        xvalue* out=NULL; assert(xl6SetIteratorAdvanceValueV1(cursor,&schema,identity,&found,&out) && !found && !out);
        assert(xl6SetIteratorAdvanceValueV1(cursor,&schema,identity,&found,&out) && !found && !out);
        xrtValueRelease(cursor); xrtValueRelease(b); xrtValueRelease(a);
    }
}
static void projectionContracts(void)
{
    XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1());
    xvalue* set=projectedCreate(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(set);
    xvalue* same=xl6SetProjectionTakeV1(xrtValueRetain(set),&tuple,xl6StringConverterV1()); assert(same==set); xrtValueRelease(same);
    assert(!xl6SetProjectionTakeV1(xrtValueRetain(set),&tuple,&faultCodec)); xrtClearError();
    int64_t count=-1; assert(xl6SetErasedCountV1(set,false,&count) && count==0);
    xvalueconverter invalid=*xl6StringConverterV1(); invalid.Context=&tuple;
    assert(!xl6SetProjectionTakeV1(xrtValueRetain(set),&tuple,&invalid)); xrtClearError();
    invalid.Context=NULL; invalid.FromTyped=NULL;
    assert(!xl6SetProjectionTakeV1(xrtValueRetain(set),&tuple,&invalid)); xrtClearError();
    for (int fail=-1;fail<=1;fail+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=fail;
        assert(!xl6SetProjectionTakeV1(xrtValueRetain(set),&tuple,xl6StringConverterV1()));
        xl6ErrorScopeCaptureFailure(&parent); xl6_error_result r=xl6ErrorScopeEnd(&parent);
        assert(!r.ok && (fail<0 ? !r.error : r.error && xrtErrorKind(r.error)==XERR_MEMORY)); xrtErrorFree(r.error);
        assert(xl6SetErasedCountV1(set,false,&count) && count==0);
        assert(xl6ErrorScopeBegin(&parent)); cleanupFailure=fail;
        assert(!xl6SetProjectionTakeV1(xrtValueRetain(set),&tuple,xl6StringConverterV1()));
        xl6ErrorScopeCaptureFailure(&parent); r=xl6ErrorScopeEnd(&parent);
        assert(!r.ok && (fail<0 ? !r.error : r.error && xrtErrorKind(r.error)==XERR_MEMORY)); xrtErrorFree(r.error);
        assert(xl6SetErasedCountV1(set,false,&count) && count==0);
    }
    bool unchanged=true;
    assert(!xl6SetErasedRelationV1(set,set,XL6_SET_EQUALS_V1,true,&unchanged) && unchanged); xrtClearError();
    assert(!xl6SetErasedKeyV1(set,NULL,XL6_SET_ADD_V1,&unchanged) && unchanged); xrtClearError();
    assert(!xl6SetErasedReserveV1(set,-1)); xrtClearError(); xrtValueRelease(set);
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(1,DOMAIN,ELEMENT+1,2,tuple.value.size,tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT);
    assert(witness); set=xl6SetProjectionTakeV1(xl6SetCreateTakeV1(&tuple,witness),&tuple,xl6NullableStringConverterV1()); assert(set);
    xvalue* empty=xrtValueString((xstrview){"",0}); assert(empty);
    assert(xl6SetErasedKeyV1(set,xrtValueNull(),XL6_SET_ADD_V1,&unchanged) && unchanged);
    assert(xl6SetErasedKeyV1(set,empty,XL6_SET_ADD_V1,&unchanged) && unchanged);
    xvalue* out=NULL; bool found=false;
    assert(xl6SetErasedReadV1(set,xrtValueNull(),true,&out,&found) && found && xrtValueType(out)==XVALUE_NULL); xrtValueRelease(out); out=NULL;
    assert(xl6SetErasedReadV1(set,empty,false,&out,&found) && found && xrtValueType(out)==XVALUE_STRING && xrtValueTypeId(out)==ELEMENT);
    xrtValueRelease(out); xrtValueRelease(empty); xrtValueRelease(set);
    tuple=layout(1,xl6CharTypeV1()); set=projectedCreate(&tuple,XL6_VALUE_ADMIT_CHAR,xl6CharConverterV1()); assert(set);
    xvalue* ch=xrtValueChar(0x4f60); assert(ch && xl6SetErasedKeyV1(set,ch,XL6_SET_ADD_V1,&unchanged));
    out=NULL; assert(xl6SetErasedReadV1(set,ch,true,&out,&found) && found && xrtValueTypeId(out)==ELEMENT && xrtValueEqual(out,ch));
    xrtValueRelease(out); xrtValueRelease(ch); xrtValueRelease(set);
}
/* The boxed operation lanes cover their own entire allocation prefixes. They
 * are not inferred from the earlier raw-slot tests or a first-hit sample. */
static void erasedPrefixes(void)
{
    size_t attempts=0;
    for (unsigned lane=0;lane<17;++lane) {
        bool complete=false;
        for (size_t point=0;point<1024;++point) {
            xl6ProductMemoryBegin(SIZE_MAX);
            XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1());
            xvalue* a=projectedCreate(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()),*b=projectedCreate(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(a && b);
            xvalue* key=xrtValueString((xstrview){"a\0b",3}),*other=xrtValueString((xstrview){"x\0y",3}); assert(key && other);
            bool changed=false; assert(xl6SetErasedKeyV1(a,key,XL6_SET_ADD_V1,&changed) && changed);
            assert(xl6SetErasedKeyV1(b,other,XL6_SET_ADD_V1,&changed) && changed);
            xvalue* cursor=NULL; XL6_SetLayoutV1 schema={0}; uint64_t identity=0;
            if (lane==12) assert(xl6SetIteratorSnapshotErasedV1(a,false,&cursor,&schema,&identity)==1);
            xfuture* future=NULL; xpromise* promise=NULL; xvalue* special=NULL,*query=NULL;
            if (lane>=15) {
                XL6_SetLayoutV1 specialized=lane==15 ? layout(3,xrtTypeValue()) : layout(4,xrtTypeFuture());
                special=projectedCreate(&specialized,lane==15 ? XL6_VALUE_ADMIT_ANY : 0,NULL); assert(special);
                if (lane==15) query=xrtValueArray();
                else { promise=xrtPromiseCreate(&future,NULL); assert(promise); query=xrtValueFuture(future); assert(query && xrtValueTypeIdBind(query,ELEMENT)); }
                assert(query && xl6SetErasedKeyV1(special,query,XL6_SET_ADD_V1,&changed) && changed);
            }
            xvalue* result=NULL; bool success=false,found=true; assert(xrtMemDebugFailAfter(point));
            switch (lane) {
            case 0: result=projectedCreate(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); success=result!=NULL; break;
            case 1: success=xl6SetErasedKeyV1(a,other,XL6_SET_ADD_V1,&found); break;
            case 2: success=xl6SetErasedKeyV1(a,key,XL6_SET_HAS_V1,&found); break;
            case 3: case 4: success=xl6SetErasedReadV1(a,key,lane==4,&result,&found); break;
            case 5: success=xl6SetErasedKeyV1(a,key,XL6_SET_REMOVE_V1,&found); break;
            case 6: result=xl6SetErasedCloneV1(a); success=result!=NULL; break;
            case 7: case 8: case 9: case 10: result=xl6SetErasedAlgebraV1(a,b,(XL6_SetAlgebraV1)(lane-6)); success=result!=NULL; break;
            case 11: success=xl6SetIteratorSnapshotErasedV1(a,false,&result,&schema,&identity)==1; break;
            case 12: success=xl6SetIteratorAdvanceValueV1(cursor,&schema,identity,&found,&result); break;
            case 13: success=xl6SetErasedReserveV1(a,80); break;
            case 14: success=xl6SetErasedMergeV1(a,b); break;
            case 15: case 16: success=xl6SetErasedReadV1(special,query,false,&result,&found); break;
            }
            bool hit=xl6ProductMemoryEnd(); ++attempts;
            assert(hit ? !success : success); xrtClearError();
            int64_t count=-1; assert(xl6SetErasedCountV1(b,false,&count) && count==1);
            if (hit) {
                assert(xl6SetErasedCountV1(a,false,&count) && count==1 && !result && found);
                if (lane==12) { assert(!xl6SetIteratorAdvanceValueV1(cursor,&schema,identity,&found,&result) && !result); xrtClearError(); }
            } else if (lane==3 || lane==4 || lane==12) assert(found && result && xrtValueEqual(result,key));
            if (special) assert(xl6SetErasedCountV1(special,false,&count) && count==1);
            xrtValueRelease(result); xrtValueRelease(cursor); xrtValueRelease(special); xrtValueRelease(query);
            xrtPromiseDestroy(promise); xrtFutureDestroy(future);
            xrtValueRelease(other); xrtValueRelease(key); xrtValueRelease(b); xrtValueRelease(a); xrtClearError(); xl6ProductMemoryVerify();
            if (!hit) { complete=true; printf("erased-set lane=%u attempts=%zu first-no-hit=%zu\n",lane,point+1,point); break; }
        }
        assert(complete);
    }
    printf("erased-set full-prefix-attempts=%zu\n",attempts);
}
/* Each allocation attempt verifies inputs, cleans every published output,
 * then closes its own real debug allocation ledger. No first-hit shortcut. */
static void prefixes(void)
{
    size_t attempts=0;
    for (unsigned lane=0;lane<11;++lane) {
        bool complete=false;
        for (size_t point=0;point<1024;++point) {
            xl6ProductMemoryBegin(SIZE_MAX);
            XL6_SetLayoutV1 tuple=layout(2,xl6StringTypeV1()); xvalue* a=create(&tuple,XL6_VALUE_ADMIT_STRING),*b=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(a && b);
            xstrview key={"a\0b",3},other={"x\0y",3},out={0}; add(a,&tuple,&key,true); add(b,&tuple,&other,true);
            xvalue* result=NULL; bool success=false,found=false; assert(xrtMemDebugFailAfter(point));
            switch (lane) {
            case 0: result=create(&tuple,XL6_VALUE_ADMIT_STRING); break;
            case 1: success=xl6SetAddV1(a,&tuple,&other,&found); break;
            case 2: result=xl6SetCloneV1(a,&tuple); break;
            case 3: case 4: case 5: case 6: result=xl6SetAlgebraV1(a,b,&tuple,(XL6_SetAlgebraV1)(lane-2)); break;
            case 7: result=xl6SetCursorCreateV1(a,&tuple,CURSOR,false); break;
            case 8: success=xl6SetReserveV1(a,&tuple,80); break;
            case 9: success=xl6SetMergeV1(a,b,&tuple); break;
            case 10: success=xl6SetGetV1(a,&tuple,&key,&out,false,&found); break;
            }
            bool hit=xl6ProductMemoryEnd(); ++attempts;
            if (lane==0 || (lane>=2 && lane<=7)) success=result!=NULL;
            assert(hit ? !success : success); xrtClearError();
            assert(xl6SetCountV1(b,&tuple,false)==1);
            if (hit) assert(xl6SetCountV1(a,&tuple,false)==1 && !out.Data);
            xrtValueRelease(result); xrtTypeDropValue(tuple.value.type,&out); xrtValueRelease(b); xrtValueRelease(a); xrtClearError(); xl6ProductMemoryVerify();
            if (!hit) { complete=true; printf("native-set lane=%u attempts=%zu first-no-hit=%zu\n",lane,point+1,point); break; }
        }
        assert(complete);
    }
    printf("native-set full-prefix-attempts=%zu\n",attempts);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); xl6_error_scope warm={0}; assert(xl6ErrorScopeBegin(&warm) && xl6ErrorScopeEnd(&warm).ok);
    XL6_CollectionIdentityFrameV1 frame={0}; assert(xl6CollectionIdentityEnterV1(&frame)); xl6CollectionIdentityLeaveV1(&frame);
    xl6ProductMemoryBegin(SIZE_MAX); integers(); profiles(); optionalProfiles(); cursors(); references(); graph(); futures(); rejected(); cleanup();
    erasedMethods(); erasedReferences(); erasedIterator(); erasedErrors(); projectionContracts();
    assert(!xrtGetError()); xl6ProductMemoryVerify(); prefixes(); erasedPrefixes(); puts("[PASS] genuine resident typed Set"); return 0;
}
