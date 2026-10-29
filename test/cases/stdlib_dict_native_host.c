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
static int codecMode,decodeCalls,encodeCalls;
static xvalue* codecOwner;
static bool toScalar(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    assert(!context); ++decodeCalls;
    bool ok=xrtValueToTyped(value,type,output,NULL);
    if (codecMode==1) *(int64_t*)output=256;
    if (codecMode==2) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if (codecMode==5) { int64_t count=99; assert(!xl6DictErasedCountV1(codecOwner,false,&count) && count==99); }
    return ok;
}
static xvalue* fromScalar(const xrttype* type,const void* slot,ptr context)
{
    assert(!context); ++encodeCalls;
    if (codecMode==3) return xrtValueString(XRT_STR_LITERAL("wrong-domain"));
    if (codecMode==4) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    return xrtValueFromTyped(type,slot,NULL);
}
static const xvalueconverter scalarCodec={.ToTyped=toScalar,.FromTyped=fromScalar};
static bool toText(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    assert(!context && type==xl6StringTypeV1()); xstrview text={0};
    return xrtValueGetString(value,&text) && xrtTypeCopyValue(type,output,&text);
}
static xvalue* fromText(const xrttype* type,const void* slot,ptr context)
{ assert(!context && type==xl6StringTypeV1()); return xrtValueString(*(const xstrview*)slot); }
static const xvalueconverter textCodec={.ToTyped=toText,.FromTyped=fromText};
static xvalue* projected(const XL6_DictLayoutV1* tuple,unsigned rule,const xvalueconverter* codec)
{
    if (!codec && tuple->value.type==xl6StringTypeV1()) codec=&textCodec;
    return xl6DictProjectionTakeV1(create(tuple,rule),tuple,codec);
}
static void projectedRead(const xvalue* dict,xstrview key,int64_t expected)
{
    xvalue* item=NULL; bool found=false; int64_t number=0;
    assert(xl6DictErasedReadV1(dict,key,false,&item,&found) && found && item);
    assert(xrtValueGetInt(item,&number) && number==expected); xrtValueRelease(item);
}
typedef struct KeyVisit { const xvalue* source; unsigned mode,count; } KeyVisit;
static bool visitKey(xstrview key,void* context)
{
    KeyVisit* state=context; ++state->count;
    assert((key.Size==3 && !memcmp(key.Data,"p\0q",3)) || (key.Size==1 && key.Data[0]=='x'));
    if (state->mode==1) return false;
    if (state->mode==2) { xrtSetErrorKind(XERR_STATE); return false; }
    if (state->mode==3) {
        bool removed=false; assert(!xl6DictErasedKeyV1(state->source,key,true,&removed));
        return false;
    }
    return true;
}
static void readKeyBoundaries(void)
{
    XL6_DictLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* dict=projected(&tuple,XL6_VALUE_ADMIT_I64,&scalarCodec),*item=xrtValueInt(7); assert(dict && item);
    assert(xl6DictErasedSetV1(dict,XRT_STR_LITERAL("p\0q"),item) &&
        xl6DictErasedSetV1(dict,XRT_STR_LITERAL("x"),item)); xrtValueRelease(item);
    KeyVisit state={dict,0,0}; assert(xrtMemDebugFailAfter(0));
    assert(xl6DictErasedVisitKeysV1(dict,visitKey,&state) && state.count==2);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    for(unsigned mode=1;mode<=3;++mode) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
        state=(KeyVisit){dict,mode,0}; assert(!xl6DictErasedVisitKeysV1(dict,visitKey,&state) && state.count==1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
        assert(!outcome.ok && (mode==1 ? !outcome.error : outcome.error!=NULL));
        xrtErrorFree(outcome.error); xrtClearError();
        state=(KeyVisit){dict,0,0}; assert(xl6DictErasedVisitKeysV1(dict,visitKey,&state) && state.count==2);
    }
    for(int mode=-1;mode<=1;mode+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
        iteratorFailure=mode; state=(KeyVisit){dict,0,0}; assert(!xl6DictErasedVisitKeysV1(dict,visitKey,&state));
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok && state.count==0);
        xrtErrorFree(outcome.error); xrtClearError();
    }
    xrtValueRelease(dict); puts("[PASS] exact zero-allocation key visit, checked EOF/failure and reentry refusal");
}
static unsigned readDrops,readDropMode;
static void readDrop(ptr raw,ptr user)
{
    assert(raw==(ptr)1 && !user); ++readDrops;
    if (readDropMode==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if (readDropMode==2) xrtSetErrorKind(XERR_STATE);
}
static void readFinishBoundaries(void)
{
    static const xvaluehandleops ops={NULL,readDrop,NULL,NULL};
    for(unsigned ready=0;ready<2;++ready) for(unsigned mode=0;mode<3;++mode) {
        ptr raw=(ptr)1; xvalue* value=xrtValueHandleTake(&raw,&ops,NULL); assert(value && !raw);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (!ready) xrtSetErrorKind(XERR_VALUE);
        readDropMode=mode; bool ok=xl6DictValueFinishReadV1(value,ready!=0);
        xl6_error_result result=xl6ErrorScopeEnd(&scope);
        assert(ok==(ready && !mode) && result.ok==ok);
        if (!ready) assert(result.error && xrtErrorKind(result.error)==XERR_VALUE);
        else if (mode==1) assert(!result.error);
        else if (mode==2) assert(result.error && xrtErrorKind(result.error)==XERR_STATE);
        xrtErrorFree(result.error); xrtClearError();
    }
    readDropMode=0;
    for(int mode=-1;mode<=1;mode+=2) {
        ptr raw=(ptr)1; xvalue* value=xrtValueHandleTake(&raw,&ops,NULL); assert(value);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        cleanupFailure=mode; assert(!xl6DictValueFinishReadV1(value,true));
        xl6_error_result result=xl6ErrorScopeEnd(&scope); assert(!result.ok);
        xrtErrorFree(result.error); xrtClearError();
    }
    assert(readDrops==8);
}
static void projections(void)
{
    XL6_DictLayoutV1 tuple=layout(1,xrtTypeInt64()); const xstrview key={"a\0b",3};
    xvalue* dict=create(&tuple,XL6_VALUE_ADMIT_U8),*item=xrtValueInt(1),*out=NULL;
    assert(dict && item && xl6DictErasedProbeV1(dict)==1); int64_t count=99; bool found=true;
    assert(!xl6DictErasedCountV1(dict,false,&count) && count==99); xrtClearError();
    dict=xl6DictProjectionTakeV1(dict,&tuple,&scalarCodec); assert(dict);
    xvalue* alias=xrtValueRetain(dict); assert(alias);
    xvalueconverter invalid=scalarCodec; invalid.Context=(ptr)(uintptr_t)1;
    assert(!xl6DictProjectionTakeV1(alias,&tuple,&invalid)); xrtClearError();
    alias=xrtValueRetain(dict); assert(!xl6DictProjectionTakeV1(alias,&tuple,NULL)); xrtClearError();
    alias=xrtValueRetain(dict); alias=xl6DictProjectionTakeV1(alias,&tuple,&scalarCodec); assert(alias); xrtValueRelease(alias);
    assert(xl6DictErasedSetV1(dict,key,item));
    xvalue* wrong=xrtValueInt(256); assert(wrong);
    assert(!xl6DictErasedSetV1(dict,key,wrong)); xrtClearError(); xrtValueRelease(wrong);
    wrong=xrtValueInt(2); assert(wrong && xrtValueTypeIdBind(wrong,ELEMENT_ID+1));
    assert(!xl6DictErasedSetV1(dict,key,wrong)); xrtClearError(); xrtValueRelease(wrong);
    for (int mode=1;mode<=5;++mode) {
        if (mode==3 || mode==4) continue;
        xvalue* attempt=xrtValueInt(2); assert(attempt);
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); codecMode=mode; codecOwner=dict;
        assert(!xl6DictErasedSetV1(dict,key,attempt)); codecMode=0; codecOwner=NULL;
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok);
        assert(mode==2 ? !outcome.error : outcome.error!=NULL); xrtErrorFree(outcome.error);
        xrtValueRelease(attempt);
        projectedRead(dict,key,1);
    }
    for (int mode=3;mode<=4;++mode) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); codecMode=mode;
        assert(!xl6DictErasedReadV1(dict,key,true,&out,&found) && !out && found); codecMode=0;
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok);
        assert(mode==4 ? !outcome.error : outcome.error!=NULL); xrtErrorFree(outcome.error);
        projectedRead(dict,key,1);
    }
    assert(xl6DictErasedCountV1(dict,false,&count) && count==1);
    assert(xl6DictErasedReserveV1(dict,32) && xl6DictErasedCountV1(dict,true,&count) && count>=32);
    assert(xl6DictErasedTrimV1(dict));
    assert(!xl6DictErasedReserveV1(dict,-1)); xrtClearError();
    assert(xl6DictErasedReadV1(dict,XRT_STR_LITERAL("absent"),false,&out,&found) && !out && !found);
    assert(xl6DictErasedKeyV1(dict,key,false,&found) && found);
    xvalue* copy=xl6DictErasedCloneV1(dict); assert(copy); projectedRead(copy,key,1); xrtValueRelease(copy);
    assert(xl6DictErasedReadV1(dict,key,true,&out,&found) && found && out); xrtValueRelease(out); out=NULL;
    assert(xl6DictErasedCountV1(dict,false,&count) && count==0);
    assert(xl6DictErasedSetV1(dict,key,item) && xl6DictErasedKeyV1(dict,key,true,&found) && found);
    assert(xl6DictErasedClearV1(dict)); xrtValueRelease(dict); xrtValueRelease(item);
    ptr opaque=(ptr)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&opaque,&foreignOps,NULL); assert(foreign);
    assert(xrtValueTypeIdBind(foreign,DICT_ID) && xl6DictErasedProbeV1(foreign)==0);
    assert(!xl6DictErasedCountV1(foreign,false,&count)); xrtClearError(); xrtValueRelease(foreign);

    tuple=layout(3,xrtTypeValue()); dict=projected(&tuple,XL6_VALUE_ADMIT_ANY,NULL); item=xrtValueObject(); assert(dict && item);
    assert(xrtValueTypeIdBind(item,ELEMENT_ID+10));
    assert(xl6DictErasedSetV1(dict,key,item) && xl6DictErasedSetV1(dict,XRT_STR_LITERAL("self"),dict));
    assert(xl6DictErasedReadV1(dict,key,false,&out,&found) && found && out==item);
    assert(xrtValueTypeId(out)==ELEMENT_ID+10); xrtValueRelease(out); out=NULL;
    xvalue* wire=xl6DictToWireV1(dict); assert(wire && xrtValueObjectGet(wire,key)==item);
    assert(xrtValueObjectGet(wire,XRT_STR_LITERAL("self"))==dict);
    copy=xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_ANY,NULL),wire); assert(copy);
    assert(xl6DictErasedReadV1(copy,key,false,&out,&found) && found && out==item); xrtValueRelease(out); out=NULL;
    assert(xrtValueTypeId(item)==ELEMENT_ID+10); xrtValueRelease(copy); xrtValueRelease(wire);
    assert(xl6DictErasedClearV1(dict)); xrtValueRelease(item); xrtValueRelease(dict);
    puts("[PASS] checked resident Dict projection and erased methods");
}
static void projectedSnapshots(void)
{
    XL6_DictLayoutV1 tuple=layout(2,xl6StringTypeV1());
    const xstrview keys[]={XRT_STR_LITERAL(""),{"a\0b",3},{"\xE4\xBD\xA0",3}};
    for (unsigned reverse=0;reverse<2;++reverse) {
        xvalue* dict=projected(&tuple,XL6_VALUE_ADMIT_STRING,NULL); assert(dict);
        for (unsigned i=0;i<3;++i) assert(xl6DictSetV1(dict,&tuple,keys[i],&keys[i]));
        xvalue* cursor=NULL; XL6_DictLayoutV1 actual={0}; uint64_t identity=0;
        assert(xl6DictIteratorSnapshotErasedV1(dict,reverse,&cursor,&actual,&identity)==1);
        assert(identity && identity!=DICT_ID && identity!=ARRAY_ID); xrtValueRelease(dict);
        for (unsigned i=0;i<3;++i) {
            unsigned index=reverse ? 2-i : i; xvalue* item=NULL; bool found=false; xstrview key={0},text={0};
            assert(xl6DictIteratorAdvanceValueV1(cursor,&actual,identity,&found,&item,&key) && found);
            assert(key.Data && key.Size==keys[index].Size && !memcmp(key.Data,keys[index].Data,key.Size));
            assert(xrtValueGetString(item,&text) && text.Size==key.Size && !memcmp(text.Data,key.Data,key.Size));
            xrtFree((ptr)key.Data); xrtValueRelease(item);
        }
        for (unsigned i=0;i<2;++i) {
            xvalue* item=NULL; bool found=true; xstrview key={0};
            assert(xl6DictIteratorAdvanceValueV1(cursor,&actual,identity,&found,&item,&key) && !found && !item && !key.Data);
        }
        xrtValueRelease(cursor);
    }
    tuple=layout(1,xrtTypeInt64()); xvalue* dict=projected(&tuple,XL6_VALUE_ADMIT_I64,&scalarCodec); assert(dict);
    assert(xl6DictSetV1(dict,&tuple,XRT_STR_LITERAL("key"),&(int64_t){8}));
    xvalue* cursor=NULL,*item=NULL; XL6_DictLayoutV1 actual={0}; uint64_t identity=0;
    assert(xl6DictIteratorSnapshotErasedV1(dict,false,&cursor,&actual,&identity)==1); xrtValueRelease(dict);
    xstrview key={0}; bool found=true;
    assert(!xl6DictIteratorAdvanceValueV1(cursor,&actual,identity,&found,(xvalue**)&key,&key)); xrtClearError();
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); codecMode=4;
    assert(!xl6DictIteratorAdvanceValueV1(cursor,&actual,identity,&found,&item,&key) && found && !item && !key.Data);
    codecMode=0; xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok && !outcome.error);
    assert(!xl6DictIteratorAdvanceValueV1(cursor,&actual,identity,&found,&item,&key)); xrtClearError(); xrtValueRelease(cursor);
    puts("[PASS] owned exact Dict snapshot keys and sticky projection failure");
}
static void imports(void)
{
    XL6_DictLayoutV1 tuple=layout(1,xrtTypeInt64()); const xstrview key={"a\0b",3};
    xvalue* source=projected(&tuple,XL6_VALUE_ADMIT_I64,&scalarCodec); assert(source);
    assert(xl6DictSetV1(source,&tuple,key,&(int64_t){255}));
    int decodes=decodeCalls,encodes=encodeCalls;
    xvalue* copy=xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_I64,&scalarCodec),source); assert(copy);
    assert(decodeCalls==decodes && encodeCalls==encodes); projectedRead(copy,key,255); xrtValueRelease(copy);
    copy=xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_U8,&scalarCodec),source); assert(copy);
    assert(decodeCalls>decodes && encodeCalls>encodes); projectedRead(copy,key,255); xrtValueRelease(copy);
    xvalue* wire=xl6DictToWireV1(source); assert(wire && xrtValueType(wire)==XVALUE_OBJECT && xrtValueCount(wire)==1);
    copy=xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_U8,NULL),wire); assert(copy); projectedRead(copy,key,255);
    xrtValueRelease(copy); xrtValueRelease(wire);
    assert(xl6DictSetV1(source,&tuple,XRT_STR_LITERAL("bad"),&(int64_t){256}));
    assert(!xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_U8,NULL),source)); xrtClearError();
    int64_t count=0; assert(xl6DictErasedCountV1(source,false,&count) && count==2); projectedRead(source,key,255);
    copy=xrtValueRetain(source); assert(!xl6DictImportTakeV1(copy,source)); xrtClearError();
    copy=projected(&tuple,XL6_VALUE_ADMIT_I64,NULL); assert(copy);
    assert(xl6DictSetV1(copy,&tuple,key,&(int64_t){2})); assert(!xl6DictImportTakeV1(copy,source)); xrtClearError();
    copy=create(&tuple,XL6_VALUE_ADMIT_I64); assert(copy);
    assert(!xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_I64,NULL),copy)); xrtClearError(); xrtValueRelease(copy);
    wire=xrtValueArray(); assert(wire);
    assert(!xl6DictImportTakeV1(projected(&tuple,XL6_VALUE_ADMIT_I64,NULL),wire)); xrtClearError(); xrtValueRelease(wire);
    xrtValueRelease(source);
    puts("[PASS] explicit one-level Dict wire conversion and checked consuming import");
}
static void projectedDomains(void)
{
    XL6_DictLayoutV1 tuple=layout(1,xrtTypeUInt64()); xvalue* item=xrtValueUInt(UINT64_MAX),*out=NULL;
    xvalue* dict=projected(&tuple,XL6_VALUE_ADMIT_U64,NULL); bool found=false; uint64_t number=0;
    assert(dict && item && xl6DictErasedSetV1(dict,XRT_STR_LITERAL("max"),item));
    assert(xl6DictErasedReadV1(dict,XRT_STR_LITERAL("max"),false,&out,&found) && found);
    assert(xrtValueGetUInt(out,&number) && number==UINT64_MAX && xrtValueTypeId(out)==ELEMENT_ID);
    xrtValueRelease(item); xrtValueRelease(out); xrtValueRelease(dict); out=NULL;
    tuple=layout(1,xl6CharTypeV1()); dict=projected(&tuple,XL6_VALUE_ADMIT_CHAR,xl6CharConverterV1());
    item=xrtValueChar(0x4F60); assert(dict && item && xl6DictErasedSetV1(dict,XRT_STR_LITERAL("char"),item));
    assert(xl6DictErasedReadV1(dict,XRT_STR_LITERAL("char"),false,&out,&found) && found && xrtValueType(out)==XVALUE_CHAR);
    xrtValueRelease(item); xrtValueRelease(out); xrtValueRelease(dict); out=NULL;
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID+1,
        2,sizeof(xstrview),tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID); assert(witness);
    dict=xl6DictProjectionTakeV1(xl6DictCreateTakeV1(&tuple,witness),&tuple,xl6NullableStringConverterV1());
    assert(dict && xl6DictErasedSetV1(dict,XRT_STR_LITERAL("absent"),xrtValueNull()));
    assert(xl6DictErasedReadV1(dict,XRT_STR_LITERAL("absent"),false,&out,&found) && found && xrtValueType(out)==XVALUE_NULL);
    xrtValueRelease(out); out=NULL; item=xrtValueString((xstrview){"a\0b",3}); assert(item);
    assert(xl6DictErasedSetV1(dict,XRT_STR_LITERAL("text"),item));
    xvalue* wire=xl6DictToWireV1(dict); assert(wire && xrtValueCount(wire)==2);
    xstrview text={0}; assert(xrtValueGetString(xrtValueObjectGet(wire,XRT_STR_LITERAL("text")),&text) && text.Size==3);
    xrtValueRelease(item); xrtValueRelease(wire); xrtValueRelease(dict);
    tuple=layout(3,xrtTypeValue()); dict=projected(&tuple,XL6_VALUE_ADMIT_BYTES,NULL);
    item=xrtValueBytes((xbytesview){(const uint8*)"a\0b",3}); assert(dict && item);
    assert(xl6DictErasedSetV1(dict,XRT_STR_LITERAL("bytes"),item));
    assert(xl6DictErasedReadV1(dict,XRT_STR_LITERAL("bytes"),true,&out,&found) && found && out==item);
    /* Unbound incoming primitives remain unbound: read never retags a reference. */
    assert(!xrtValueTypeId(item)); xrtValueRelease(out); xrtValueRelease(item); xrtValueRelease(dict);
    puts("[PASS] Dict uint64 char nullable-string and bytes projection domains");
}
static bool projectionFault(const char* operation,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_DictLayoutV1 tuple=layout(2,xl6StringTypeV1());
    const xstrview key={"a\0b",3},text={"v\0\xE4\xBD\xA0",5};
    xvalue* dict=projected(&tuple,XL6_VALUE_ADMIT_STRING,NULL),*input=xrtValueString(text),*result=NULL;
    xvalue* cursor=NULL,*wire=NULL,*target=NULL; bool found=false,success=false; xstrview outputKey={0};
    assert(dict && input && xl6DictSetV1(dict,&tuple,key,&text));
    if (!strcmp(operation,"project")) target=create(&tuple,XL6_VALUE_ADMIT_STRING);
    if (!strcmp(operation,"step")) cursor=xl6DictCursorCreateV1(dict,&tuple,CURSOR_ID,false);
    if (!strcmp(operation,"import-native") || !strcmp(operation,"import-wire")) target=projected(&tuple,XL6_VALUE_ADMIT_STRING,NULL);
    if (!strcmp(operation,"import-wire")) wire=xl6DictToWireV1(dict);
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); assert(xrtMemDebugFailAfter(point));
    if (!strcmp(operation,"project")) { result=xl6DictProjectionTakeV1(target,&tuple,&textCodec); target=NULL; success=result!=NULL; }
    else if (!strcmp(operation,"set")) success=xl6DictErasedSetV1(dict,XRT_STR_LITERAL("new"),input);
    else if (!strcmp(operation,"read") || !strcmp(operation,"take"))
        success=xl6DictErasedReadV1(dict,key,!strcmp(operation,"take"),&result,&found);
    else if (!strcmp(operation,"snapshot")) { XL6_DictLayoutV1 actual={0}; uint64_t id=0;
        success=xl6DictIteratorSnapshotErasedV1(dict,false,&result,&actual,&id)==1;
        if (!success) assert(!actual.identity && !id);
    }
    else if (!strcmp(operation,"step")) success=xl6DictIteratorAdvanceValueV1(cursor,&tuple,CURSOR_ID,&found,&result,&outputKey);
    else if (!strcmp(operation,"wire")) { result=xl6DictToWireV1(dict); success=result!=NULL; }
    else if (!strcmp(operation,"import-native") || !strcmp(operation,"import-wire")) {
        result=xl6DictImportTakeV1(target,wire ? wire : dict); target=NULL; success=result!=NULL;
    } else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    if (hit) assert(!result && !found && !outputKey.Data);
    if (!strcmp(operation,"step") && hit) {
        assert(!xl6DictIteratorAdvanceValueV1(cursor,&tuple,CURSOR_ID,&found,&result,&outputKey)); xrtClearError();
    }
    int64_t count=0; assert(xl6DictErasedCountV1(dict,false,&count));
    assert(count==(hit ? 1 : !strcmp(operation,"take") ? 0 : !strcmp(operation,"set") ? 2 : 1));
    xrtFree((ptr)outputKey.Data); xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(target);
    xrtValueRelease(wire); xrtValueRelease(cursor); xrtValueRelease(input); xrtValueRelease(dict); xrtClearError();
    assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2); xl6ProductMemoryBegin(SIZE_MAX);
    if (!strcmp(argv[1],"vectors")) {
        keys(); domains(); snapshots(); references(); phased(); boundaries();
        projections(); readKeyBoundaries(); readFinishBoundaries(); projectedSnapshots(); imports(); projectedDomains();
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
    const char* projections[]={"project","set","read","take","snapshot","step","wire","import-native","import-wire"};
    for (size_t i=0;i<sizeof(projections)/sizeof(projections[0]);++i) {
        bool complete=false;
        for (size_t point=0;point<4097;++point) {
            ++attempts;
            if (!projectionFault(projections[i],point)) {
                printf("native Dict projection-%s first-no-hit=%zu\n",projections[i],point); complete=true; break;
            }
        }
        assert(complete);
    }
    printf("native-dict full-prefix-attempts=%zu\n",attempts); return 0;
}
