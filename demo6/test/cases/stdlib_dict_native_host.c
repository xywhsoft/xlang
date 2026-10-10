#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DICT_ID UINT64_C(0xD710)
#define ARRAY_ID UINT64_C(0xD711)
#define ELEMENT_ID UINT64_C(0xD712)
#define CURSOR_ID UINT64_C(0xD713)
static int entryFailure,cleanupFailure,iteratorFailure;
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
extern ptr __real_xrtTypedDictIterNext(xtypeddictiter*,xstrview*);
ptr __wrap_xrtTypedDictIterNext(xtypeddictiter* iterator,xstrview* key)
{
    if (iteratorFailure) {
        int selected=iteratorFailure; iteratorFailure=0;
        if (selected>0) xrtSetErrorKind(XERR_STATE);
        else xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        return NULL;
    }
    return __real_xrtTypedDictIterNext(iterator,key);
}
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (entryFailure) {
        int selected=entryFailure; entryFailure=0;
        if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError();
        return false;
    }
    return __real_xl6ErrorScopeBegin(scope);
}
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failure)
{
    if (cleanupFailure) {
        int selected=cleanupFailure; cleanupFailure=0;
        if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError();
        return false;
    }
    return __real_xl6ErrorScopeBeginCleanup(scope,failure);
}
static XL6_DictLayoutV1 layout(unsigned transfer,const xrttype* type)
{ return (XL6_DictLayoutV1){DICT_ID,{ARRAY_ID,ELEMENT_ID,transfer,type->Size,type}}; }
static xvalue* seed(const XL6_DictLayoutV1* tuple,unsigned rule)
{
    const XL6_SequenceLayoutV1* t=&tuple->value;
    if (t->transfer==3) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,
        t->identity,t->elementIdentity,rule,false,t->elementIdentity);
    if (t->transfer==4) return xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,
        t->identity,t->elementIdentity,t->elementIdentity,false);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,t->identity,t->elementIdentity,
        t->transfer,t->size,t->type,rule,false,t->elementIdentity);
}
static xvalue* create(const XL6_DictLayoutV1* tuple,unsigned rule)
{
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xvalue* witness=seed(tuple,rule); if (!witness) xl6ErrorScopeCaptureFailure(&scope);
    xvalue* result=xl6DictCreateTakeV1(tuple,witness);
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
static void keys(void)
{
    XL6_DictLayoutV1 tuple=layout(1,xrtTypeInt64()); xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_I64); assert(dict);
    char original[]={'a',0,'b',(char)0xE4,(char)0xBD,(char)0xA0};
    xstrview key={original,sizeof(original)}; int64_t item=INT64_MIN,read=99; bool found=true;
    assert(xl6DictGetV1(dict,&tuple,key,&read,false,&found) && !found && read==99);
    assert(xl6DictKeyV1(dict,&tuple,key,true,&found) && !found);
    assert(xl6DictSetV1(dict,&tuple,key,&item)); original[0]='z';
    assert(xl6DictSetV1(dict,&tuple,(xstrview){"a",1},&(int64_t){12}));
    assert(xl6DictSetV1(dict,&tuple,(xstrview){NULL,0},&(int64_t){42}));
    assert(xl6DictSetV1(dict,&tuple,(xstrview){"a\0b\xE4\xBD\xA0",6},&(int64_t){INT64_MAX}));
    assert(xl6DictCountV1(dict,&tuple)==3);
    assert(xl6DictGetV1(dict,&tuple,(xstrview){"a\0b\xE4\xBD\xA0",6},&read,false,&found) && found && read==INT64_MAX);
    assert(xl6DictGetV1(dict,&tuple,(xstrview){"",0},&read,true,&found) && found && read==42);
    assert(xl6DictCountV1(dict,&tuple)==2 && xl6DictReserveV1(dict,&tuple,40) && xl6DictCapacityV1(dict,&tuple)>=40);
    assert(!xl6DictReserveV1(dict,&tuple,-1)); xrtClearError();
    assert(!xl6DictSetV1(dict,&tuple,(xstrview){NULL,1},&item)); xrtClearError();
    assert(!xl6DictKeyV1(dict,&tuple,(xstrview){NULL,1},false,&found)); xrtClearError();
    assert(xl6DictKeyV1(dict,&tuple,(xstrview){"a",1},true,&found) && found);
    assert(xl6DictTrimV1(dict,&tuple) && xl6DictClearV1(dict,&tuple));
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6DictCountV1(dict,&tuple)==0 && xrtGetError()==prior); xrtClearError();
    xrtValueRelease(dict);
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID+1,
        2,sizeof(xstrview),tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID); assert(witness);
    dict=xl6DictCreateTakeV1(&tuple,witness); assert(dict);
    xstrview absent={0},present={"a\0b",3},text={0}; found=false;
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("absent"),&absent));
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("present"),&present));
    assert(xl6DictGetV1(dict,&tuple,XRT_STR_LITERAL("absent"),&text,false,&found) && found && !text.Data);
    assert(xl6DictGetV1(dict,&tuple,XRT_STR_LITERAL("present"),&text,false,&found) && found && text.Size==3);
    xrtTypeDropValue(tuple.value.type,&text); xrtValueRelease(dict);
    tuple=layout(3,xrtTypeValue()); dict=create(&tuple,XL6_VALUE_ADMIT_BYTES); assert(dict);
    xvalue* bytes=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3}),*wrong=xrtValueInt(3); assert(bytes && wrong);
    assert(xrtValueTypeIdBind(bytes,ELEMENT_ID));
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("bytes"),&bytes));
    assert(!xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("bytes"),&wrong)); xrtClearError();
    xrtValueRelease(wrong); xrtValueRelease(bytes); xrtValueRelease(dict);
}
static bool opaqueRetain(ptr data,ptr* out,ptr context) { (void)context; *out=data; return true; }
static void opaqueDrop(ptr data,ptr context) { (void)data; (void)context; }
static const xvaluehandleops foreignOps={opaqueRetain,opaqueDrop,NULL,NULL};
static void domains(void)
{
    XL6_DictLayoutV1 tuple=layout(1,xrtTypeInt64()); xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_U8); assert(dict);
    assert(!xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("range"),&(int64_t){256})); xrtClearError();
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("range"),&(int64_t){255}));
    XL6_DictLayoutV1 actual={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6DictSchemaCopyV1(dict,&actual,&policy) && xl6DictAdmitsSchemaV1(dict,&actual,&policy));
    xvalue* peer=create(&tuple,XL6_VALUE_ADMIT_I64); assert(peer);
    assert(!xl6DictAdmitsSchemaV1(peer,&actual,&policy));
    assert(!xl6DictMergeV1(dict,peer,&tuple,true)); xrtClearError();
    assert(xl6DictCountV1(dict,&tuple)==1 && xl6DictCountV1(peer,&tuple)==0);
    XL6_DictLayoutV1 changed=tuple; changed.value.elementIdentity++;
    assert(!xl6DictAdmitsSchemaV1(dict,&changed,&policy));
    assert(!xl6DictSetV1(dict,&changed,XRT_STR_LITERAL("bad"),&(int64_t){1})); xrtClearError();
    ptr opaque=(ptr)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&opaque,&foreignOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,DICT_ID));
    assert(!xl6DictAdmitsSchemaV1(foreign,&tuple,&policy));
    XL6_DictLayoutV1 untouched={.identity=777}; XL6_SequenceAdmissionV3 empty={0};
    assert(!xl6DictSchemaCopyV1(foreign,&untouched,&empty) && untouched.identity==777 && !empty.ops); xrtClearError();
    bool found=true; int64_t value=99;
    assert(!xl6DictGetV1(foreign,&tuple,XRT_STR_LITERAL("x"),&value,false,&found) && found && value==99); xrtClearError();
    uint64 hash=99; assert(!xrtValueHash(dict,&hash) && hash==99); xrtClearError();
    xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(foreign); xrtValueRelease(peer); xrtValueRelease(dict);
    xvalue* witness=seed(&tuple,XL6_VALUE_ADMIT_I64); assert(witness);
    XL6_Sequence* owner=xl6SequenceBeginV1(witness,ARRAY_ID,ELEMENT_ID,1,sizeof(int64_t),xrtTypeInt64()); assert(owner);
    assert(xrtTypedArrayPush(xl6SequenceStorageV1(owner),&(int64_t){4})); xl6SequenceEndV1(owner);
    assert(!xl6DictCreateTakeV1(&tuple,witness)); xrtClearError();
    assert(!xl6DictCreateTakeV1(&tuple,xrtValueArray())); xrtClearError();
    tuple=layout(1,xl6CharTypeV1()); dict=create(&tuple,XL6_VALUE_ADMIT_CHAR); assert(dict);
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("char"),&(uint32_t){0x4F60}));
    assert(!xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("char"),&(uint32_t){0xD800})); xrtClearError();
    xrtValueRelease(dict);
    tuple=layout(1,xrtTypeUInt64()); dict=create(&tuple,XL6_VALUE_ADMIT_U64); assert(dict);
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("max"),&(uint64_t){UINT64_MAX}));
    uint64_t read=0; found=false;
    assert(xl6DictGetV1(dict,&tuple,XRT_STR_LITERAL("max"),&read,false,&found) && found && read==UINT64_MAX);
    xrtValueRelease(dict);
}
static void snapshots(void)
{
    XL6_DictLayoutV1 tuple=layout(2,xl6StringTypeV1());
    const xstrview keys[]={XRT_STR_LITERAL(""),{"a\0b",3},{"\xE4\xBD\xA0",3}};
    for (unsigned reverse=0;reverse<2;++reverse) {
        xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(dict);
        for (unsigned i=0;i<3;++i) assert(xl6DictSetV1(dict,&tuple,keys[i],&keys[i]));
        xvalue* copy=xl6DictCloneV1(dict,&tuple),*cursor=xl6DictCursorCreateV1(dict,&tuple,CURSOR_ID,reverse); assert(copy && cursor);
        assert(xl6DictClearV1(dict,&tuple)); xrtValueRelease(dict); xrtValueRelease(copy);
        assert(xl6DictCountV1(cursor,&tuple)==-1); xrtClearError();
        bool found=true; xstrview key={0},value={0};
        assert(!xl6DictCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&value) && !key.Data && !value.Data); xrtClearError();
        for (unsigned i=0;i<3;++i) {
            unsigned index=reverse ? 2-i : i;
            assert(xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
            assert(!xl6DictCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&key) && !key.Data); xrtClearError();
            assert(xl6DictCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&value));
            assert(key.Data && key.Size==keys[index].Size && !memcmp(key.Data,keys[index].Data,key.Size));
            assert(value.Data && value.Size==key.Size && !memcmp(value.Data,key.Data,key.Size));
            xrtFree((ptr)key.Data); key=(xstrview){0}; xrtTypeDropValue(tuple.value.type,&value);
        }
        assert(xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && !found);
        assert(xl6DictCursorEndV1(cursor,&tuple,CURSOR_ID) && xl6DictCursorEndV1(cursor,&tuple,CURSOR_ID));
        assert(xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && !found); xrtValueRelease(cursor);
    }
    xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(dict);
    for (unsigned i=0;i<1024;++i) {
        char key[20]; int length=snprintf(key,sizeof(key),"key-%u",i); xstrview text={key,(size_t)length};
        assert(xl6DictSetV1(dict,&tuple,text,&text));
    }
    xvalue* cursor=xl6DictCursorCreateV1(dict,&tuple,CURSOR_ID,false); assert(cursor); xrtValueRelease(dict);
    for (unsigned i=0;i<1024;++i) {
        bool found=false; xstrview key={0},value={0};
        assert(xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
        assert(xl6DictCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&value) && key.Size==value.Size && !memcmp(key.Data,value.Data,key.Size));
        xrtFree((ptr)key.Data); xrtTypeDropValue(tuple.value.type,&value);
    }
    xrtValueRelease(cursor);
    for (int failure=-1;failure<=1;failure+=2) {
        dict=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(dict);
        assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("key"),&(xstrview){"value",5}));
        cursor=xl6DictCursorCreateV1(dict,&tuple,CURSOR_ID,false); assert(cursor); xrtValueRelease(dict);
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); iteratorFailure=failure;
        bool found=true; assert(!xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found && !iteratorFailure);
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok);
        assert(failure<0 ? !outcome.error : outcome.error!=NULL); xrtErrorFree(outcome.error);
        assert(xl6DictCursorEndV1(cursor,&tuple,CURSOR_ID));
        assert(!xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found); xrtClearError();
        xrtValueRelease(cursor);
    }
}
typedef struct Edges { size_t count; xrtownershipref first; bool same; } Edges;
static bool edge(xrtownershipref ref,ptr opaque)
{
    Edges* edges=opaque;
    if (!edges->count) { edges->first=ref; edges->same=true; }
    else edges->same=edges->same && edges->first.Data==ref.Data && edges->first.Ops==ref.Ops;
    ++edges->count; return true;
}
static xrtownershipref ownerOf(xvalue* value)
{
    xrtownershipref shell=xrtValueOwnership(value); Edges edges={0};
    assert(shell.Ops->Trace(shell.Data,edge,&edges) && edges.count==1); return edges.first;
}
static void references(void)
{
    XL6_DictLayoutV1 tuple=layout(3,xrtTypeValue()); xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(dict);
    xvalue* item=xrtValueArray(); assert(item);
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("first"),&item) && xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("second"),&item));
    xvalue* copy=xl6DictCloneV1(dict,&tuple); assert(copy);
    assert(xrtValueArrayAppendNew(item,xrtValueInt(42)));
    xrtownershipref owner=ownerOf(dict); Edges edges={0};
    assert(owner.Ops->Trace(owner.Data,edge,&edges) && edges.count==2 && edges.same);
    xrtownershipref expected=xrtValueOwnership(item); assert(edges.first.Data==expected.Data && edges.first.Ops==expected.Ops);
    xvalue* out=NULL; bool found=false;
    assert(xl6DictGetV1(copy,&tuple,XRT_STR_LITERAL("first"),&out,false,&found) && found && out==item);
    xvalue* unchanged=out; found=false;
    assert(!xl6DictGetV1(copy,&tuple,XRT_STR_LITERAL("first"),&out,true,&found) && !found && out==unchanged);
    xrtClearError(); assert(xl6DictCountV1(copy,&tuple)==2);
    xrtValueRelease(out); xrtValueRelease(item); xrtValueRelease(copy); xrtValueRelease(dict);
    tuple=layout(4,xrtTypeFuture()); dict=create(&tuple,0); assert(dict);
    xfuture* future=NULL,*read=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("first"),&future) && xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("second"),&future));
    owner=ownerOf(dict); edges=(Edges){0};
    const XL6_SequenceAdmissionOpsV2* allowed[]={xl6ArrayFutureAdmissionOpsV2Get()};
    assert(!xl6DictOwnershipAdapterV1(owner) && xl6DictOwnershipAdapterV2(owner,allowed,1));
    assert(owner.Ops->Trace(owner.Data,edge,&edges) && edges.count==2 && edges.same);
    expected=xrtFutureOwnership(future); assert(edges.first.Data==expected.Data && edges.first.Ops==expected.Ops);
    assert(xl6DictGetV1(dict,&tuple,XRT_STR_LITERAL("first"),&read,true,&found) && found && read==future);
    xrtFutureDestroy(read); xrtValueRelease(dict); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
}
static void phased(void)
{
    XL6_DictLayoutV1 tuple=layout(3,xrtTypeValue()); xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(dict);
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("first"),&dict) && xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("second"),&dict));
    xrtownershipref owner=ownerOf(dict); Edges edges={0}; assert(owner.Ops->Trace(owner.Data,edge,&edges) && edges.count==2 && edges.same);
    const xrtownershipadapterv1* adapter=xl6DictOwnershipAdapterV1(owner); assert(adapter);
    unsigned token=0; xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token));
    adapter->Restore(owner.Data,&token); assert(adapter->Claim(owner.Data,&token));
    adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    XL6_DictLayoutV1 actual={.identity=777}; XL6_SequenceAdmissionV3 policy={0};
    assert(!xl6DictSchemaCopyV1(dict,&actual,&policy) && actual.identity==777 && !policy.ops); xrtClearError();
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); cleanupFailure=-1;
    assert(!adapter->Finish(owner.Data,&token) && !cleanupFailure); xl6ErrorScopeCaptureFailure(&parent);
    assert(adapter->Finish(owner.Data,&token) && adapter->Finish(owner.Data,&token));
    xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !result.error);
    adapter->Drop(owner.Data); xrtValueRelease(dict);
}
typedef struct DropState { xvalue* dict; XL6_DictLayoutV1 tuple; unsigned drops; bool absent; } DropState;
static void reentrantDrop(ptr raw,ptr context)
{
    DropState* state=raw; assert(state==context); ++state->drops;
    XL6_DictLayoutV1 actual={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6DictSchemaCopyV1(state->dict,&actual,&policy) && xl6DictAdmitsSchemaV1(state->dict,&actual,&policy));
    xl6SequenceAdmissionDropV3(&policy);
    if (state->absent) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    assert(xl6DictCountV1(state->dict,&state->tuple)==-1);
    xrtValueRelease(state->dict); state->dict=NULL;
}
static const xvaluehandleops reentrantOps={opaqueRetain,reentrantDrop,NULL,NULL};
static void boundaries(void)
{
    for (unsigned absent=0;absent<2;++absent) {
        DropState state={.tuple=layout(3,xrtTypeValue()),.absent=absent}; state.dict=create(&state.tuple,XL6_VALUE_ADMIT_ANY); assert(state.dict);
        ptr raw=&state; xvalue* item=xrtValueHandleTake(&raw,&reentrantOps,&state); assert(item);
        assert(xl6DictSetV1(state.dict,&state.tuple,XRT_STR_LITERAL("key"),&item)); xrtValueRelease(item);
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); assert(!xl6DictClearV1(state.dict,&state.tuple));
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && state.drops==1 && !state.dict);
        assert(absent ? !result.error : result.error && xrtErrorKind(result.error)==XERR_STATE); xrtErrorFree(result.error);
    }
    XL6_DictLayoutV1 tuple=layout(2,xl6StringTypeV1());
    for (int failure=-1;failure<=1;failure+=2) {
        xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(dict);
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); cleanupFailure=failure; xrtValueRelease(dict);
        xl6_error_result result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !cleanupFailure);
        assert(failure<0 ? !result.error : result.error!=NULL); xrtErrorFree(result.error);
        dict=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(dict);
        parent=(xl6_error_scope){0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("key"),&(xstrview){"value",5})); xl6ErrorScopeCaptureFailure(&parent);
        result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !entryFailure);
        assert(failure<0 ? !result.error : result.error!=NULL); xrtErrorFree(result.error);
        assert(xl6DictCountV1(dict,&tuple)==0); xrtValueRelease(dict);
        xvalue* witness=seed(&tuple,XL6_VALUE_ADMIT_STRING); assert(witness);
        parent=(xl6_error_scope){0}; assert(xl6ErrorScopeBegin(&parent)); entryFailure=failure;
        assert(!xl6DictCreateTakeV1(&tuple,witness)); xl6ErrorScopeCaptureFailure(&parent);
        result=xl6ErrorScopeEnd(&parent); assert(!result.ok && !entryFailure); xrtErrorFree(result.error);
    }
}
static bool fault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    XL6_DictLayoutV1 tuple=layout(2,xl6StringTypeV1()); xvalue* dict=NULL,*peer=NULL,*cursor=NULL,*result=NULL;
    const xstrview key={"a\0\xE4\xBD\xA0",5},item={"v\0\xE4\xBD\xA0",5},changed=XRT_STR_LITERAL("replacement");
    xstrview output={0},outputKey={0}; bool found=false,success=false;
    bool merge=!strcmp(operation,"merge-keep") || !strcmp(operation,"merge-replace");
    if (strcmp(operation,"create")) { dict=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(dict && xl6DictSetV1(dict,&tuple,key,&item)); }
    if (merge) {
        peer=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(peer);
        assert(xl6DictSetV1(peer,&tuple,key,&changed) && xl6DictSetV1(peer,&tuple,XRT_STR_LITERAL("new"),&changed));
    }
    if (!strcmp(operation,"cursor-read") || !strcmp(operation,"cursor-advance")) {
        cursor=xl6DictCursorCreateV1(dict,&tuple,CURSOR_ID,false); assert(cursor);
        if (!strcmp(operation,"cursor-read")) assert(xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
        found=false;
    }
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); assert(xrtMemDebugFailAfter(point));
    if (!strcmp(operation,"create")) { result=create(&tuple,XL6_VALUE_ADMIT_STRING); success=result!=NULL; }
    else if (!strcmp(operation,"insert")) success=xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("new"),&changed);
    else if (!strcmp(operation,"replace")) success=xl6DictSetV1(dict,&tuple,key,&changed);
    else if (!strcmp(operation,"clone")) { result=xl6DictCloneV1(dict,&tuple); success=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6DictCursorCreateV1(dict,&tuple,CURSOR_ID,false); success=result!=NULL; }
    else if (!strcmp(operation,"get")) success=xl6DictGetV1(dict,&tuple,key,&output,false,&found);
    else if (!strcmp(operation,"take")) success=xl6DictGetV1(dict,&tuple,key,&output,true,&found);
    else if (!strcmp(operation,"cursor-read")) success=xl6DictCursorReadV1(cursor,&tuple,CURSOR_ID,&outputKey,&output);
    else if (!strcmp(operation,"cursor-advance")) success=xl6DictCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found);
    else if (!strcmp(operation,"reserve")) success=xl6DictReserveV1(dict,&tuple,96);
    else if (!strcmp(operation,"clear")) success=xl6DictClearV1(dict,&tuple);
    else if (!strcmp(operation,"trim")) success=xl6DictTrimV1(dict,&tuple);
    else if (merge) success=xl6DictMergeV1(dict,peer,&tuple,!strcmp(operation,"merge-replace"));
    else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    bool read=!strcmp(operation,"get") || !strcmp(operation,"take") || !strcmp(operation,"cursor-read");
    if (read) assert(hit ? !output.Data && !output.Size && !outputKey.Data && !found :
        output.Size==item.Size && !memcmp(output.Data,item.Data,item.Size));
    if (dict) {
        int64_t count=hit ? 1 : (merge || !strcmp(operation,"insert")) ? 2 :
            (!strcmp(operation,"take") || !strcmp(operation,"clear")) ? 0 : 1;
        assert(xl6DictCountV1(dict,&tuple)==count);
        if (count) {
            xstrview actual={0}; bool exists=false;
            const xstrview* expected=!hit && (!strcmp(operation,"replace") || !strcmp(operation,"merge-replace")) ? &changed : &item;
            assert(xl6DictGetV1(dict,&tuple,key,&actual,false,&exists) && exists);
            assert(actual.Size==expected->Size && !memcmp(actual.Data,expected->Data,actual.Size));
            xrtTypeDropValue(tuple.value.type,&actual);
        }
    }
    if (peer) assert(xl6DictCountV1(peer,&tuple)==2);
    xrtFree((ptr)outputKey.Data); xrtTypeDropValue(tuple.value.type,&output); xrtErrorFree(outcome.error);
    xrtValueRelease(result); xrtValueRelease(cursor); xrtValueRelease(peer); xrtValueRelease(dict); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); xl6ProductMemoryBegin(SIZE_MAX);
    if (!strcmp(argv[1],"vectors")) {
        keys(); domains(); snapshots(); references(); phased(); boundaries();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
        puts("[PASS] genuine resident typed Dict vectors"); return 0;
    }
    assert(!strcmp(argv[1],"prefixes")); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    const char* operations[]={"create","insert","replace","clone","cursor","get","take","cursor-read",
        "cursor-advance","reserve","clear","trim","merge-keep","merge-replace"};
    size_t attempts=0;
    for (size_t i=0;i<sizeof(operations)/sizeof(operations[0]);++i) {
        bool complete=false;
        for (size_t point=0;point<4097;++point) {
            ++attempts;
            if (!fault(operations[i],point)) { printf("native Dict %s first-no-hit=%zu\n",operations[i],point); complete=true; break; }
        }
        assert(complete);
    }
    printf("native-dict full-prefix-attempts=%zu\n",attempts); return 0;
}
