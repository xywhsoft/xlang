#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LIST_ID UINT64_C(0xD610)
#define ARRAY_ID UINT64_C(0xD611)
#define ELEMENT_ID UINT64_C(0xD612)
#define CURSOR_ID UINT64_C(0xD613)
/* Linker call-boundary fault injection, not a replacement List/provider.
 * Negative budget models explicit failure without an allocated diagnostic. */
static int cleanupFailure;
extern bool __real_xl6ErrorScopeBeginCleanup(xl6_error_scope*,bool);
bool __wrap_xl6ErrorScopeBeginCleanup(xl6_error_scope* scope,bool failure)
{
    if (cleanupFailure) {
        int selected=cleanupFailure; cleanupFailure=0;
        if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError();
        return false;
    }
    return __real_xl6ErrorScopeBeginCleanup(scope,failure);
}
static XL6_ListLayoutV1 layout(unsigned transfer,const xrttype* type)
{ return (XL6_ListLayoutV1){LIST_ID,{ARRAY_ID,ELEMENT_ID,transfer,type->Size,type}}; }
static xvalue* seed(const XL6_ListLayoutV1* tuple,unsigned rule)
{
    const XL6_SequenceLayoutV1* t=&tuple->value;
    if (t->transfer==3) return xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,
        t->identity,t->elementIdentity,rule,false,t->elementIdentity);
    if (t->transfer==XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
        return xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,t->identity,t->elementIdentity,
            t->elementIdentity,false);
    return xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,t->identity,t->elementIdentity,
        t->transfer,t->size,t->type,rule,false,t->elementIdentity);
}
static xvalue* create(const XL6_ListLayoutV1* tuple,unsigned rule)
{
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xvalue* witness=seed(tuple,rule);
    if (!witness) xl6ErrorScopeCaptureFailure(&scope);
    xvalue* result=xl6ListCreateTakeV1(tuple,witness);
    if (!result) xl6ErrorScopeCaptureFailure(&scope);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return result;
}
static void integers(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_I64); assert(list);
    bool found=true; int64_t item=12,output=99,key=99;
    assert(xl6ListGetV1(list,&tuple,0,&output,false,&found) && !found && output==99);
    assert(xl6ListKeyV1(list,&tuple,0,true,&found) && !found);
    assert(xl6ListAppendV1(list,&tuple,&item,&key) && key==0);
    assert(xl6ListSetV1(list,&tuple,INT64_MIN,&item));
    assert(xl6ListSetV1(list,&tuple,-2,&item));
    item=42; assert(xl6ListSetV1(list,&tuple,INT64_MAX,&item));
    assert(xl6ListCountV1(list,&tuple)==4);
    assert(xl6ListGetV1(list,&tuple,INT64_MIN,&output,false,&found) && found && output==12);
    key=99; assert(!xl6ListAppendV1(list,&tuple,&item,&key) && key==99);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    assert(xl6ListCountV1(list,&tuple)==4);
    assert(xl6ListGetV1(list,&tuple,INT64_MAX,&output,true,&found) && found && output==42);
    assert(xl6ListCountV1(list,&tuple)==3);
    assert(xl6ListAppendV1(list,&tuple,&item,&key) && key==1);
    assert(xl6ListKeyV1(list,&tuple,-2,true,&found) && found);
    assert(xl6ListTrimV1(list,&tuple,-1)==-1); xrtClearError();
    assert(xl6ListClearV1(list,&tuple) && xl6ListTrimV1(list,&tuple,0)>=0);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6ListCountV1(list,&tuple)==0 && xrtGetError()==prior);
    assert(!xl6ListCursorCreateV1(list,&tuple,LIST_ID,false,false,0) && xrtGetError()==prior);
    xrtClearError(); xrtValueRelease(list);
}
static void text(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(list);
    char original[]={'a',0,(char)0xE4,(char)0xBD,(char)0xA0};
    xstrview item={original,sizeof(original)},read={0}; bool found=false;
    assert(xl6ListSetV1(list,&tuple,-9,&item)); original[0]='z';
    xvalue* copy=xl6ListCloneV1(list,&tuple); assert(copy);
    assert(xl6ListClearV1(list,&tuple)); xrtValueRelease(list);
    assert(xl6ListGetV1(copy,&tuple,-9,&read,false,&found) && found && read.Size==5 && read.Data[0]=='a');
    assert(!memcmp(read.Data+1,original+1,4));
    xstrview replacement=XRT_STR_INIT("different");
    assert(xl6ListSetV1(copy,&tuple,-9,&replacement) && read.Size==5 && read.Data[0]=='a');
    xrtTypeDropValue(tuple.value.type,&read); xrtValueRelease(copy);
}
static void profiles(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xl6CharTypeV1());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_CHAR); assert(list);
    uint32_t item=0x4F60,out=0;
    assert(xl6ListSetV1(list,&tuple,1,&item));
    item=0xD800; assert(!xl6ListSetV1(list,&tuple,1,&item)); xrtClearError();
    item=0x110000; assert(!xl6ListSetV1(list,&tuple,2,&item)); xrtClearError();
    bool found=false;
    assert(xl6ListGetV1(list,&tuple,1,&out,false,&found) && found && out==0x4F60 && xl6ListCountV1(list,&tuple)==1);
    xrtValueRelease(list);
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID+1,
        2,sizeof(xstrview),tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID);
    assert(witness); list=xl6ListCreateTakeV1(&tuple,witness); assert(list);
    xstrview absent={0},present={"a\0b",3},read={0};
    assert(xl6ListSetV1(list,&tuple,-1,&absent) && xl6ListSetV1(list,&tuple,2,&present));
    assert(xl6ListGetV1(list,&tuple,2,&read,false,&found) && found && read.Size==3 && !memcmp(read.Data,present.Data,3));
    assert(xl6ListGetV1(list,&tuple,-1,&read,false,&found) && found && !read.Data && !read.Size);
    absent.Size=1; assert(!xl6ListSetV1(list,&tuple,3,&absent)); xrtClearError();
    xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    assert(xl6ListClearV1(list,&tuple)); xrtValueRelease(list);
    int64_t key=0;
    assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
    assert(xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&read) && key==-1 && !read.Data && !read.Size);
    xrtTypeDropValue(tuple.value.type,&read); xrtValueRelease(cursor);
}
static void cursors(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeInt64());
    const int64_t keys[]={INT64_MIN,-9,0,4,INT64_MAX};
    for (unsigned reverse=0;reverse<2;++reverse) for (unsigned bounded=0;bounded<2;++bounded) {
        xvalue* list=create(&tuple,XL6_VALUE_ADMIT_I64); assert(list);
        for (unsigned i=0;i<5;++i) { int64_t item=(int64_t)i; assert(xl6ListSetV1(list,&tuple,keys[i],&item)); }
        xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,reverse,bounded,0); assert(cursor);
        assert(xl6ListClearV1(list,&tuple)); xrtValueRelease(list);
        bool found=true; int64_t key=99,item=99;
        assert(!xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&item) && key==99 && item==99); xrtClearError();
        assert(xl6ListCountV1(cursor,&tuple)==-1); xrtClearError();
        assert(!xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID+1,&found) && found); xrtClearError();
        unsigned count=bounded ? 3 : 5;
        for (unsigned i=0;i<count;++i) {
            unsigned index=reverse ? (bounded ? 2 : 4)-i : (bounded ? 2 : 0)+i;
            assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
            assert(xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&item) && key==keys[index] && item==(int64_t)index);
        }
        assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && !found);
        assert(xl6ListCursorEndV1(cursor,&tuple,CURSOR_ID) && xl6ListCursorEndV1(cursor,&tuple,CURSOR_ID));
        assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && !found);
        xrtValueRelease(cursor);
    }
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_I64); assert(list);
    xvalue* empty=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,true,true,INT64_MIN); assert(empty);
    bool found=true; assert(xl6ListCursorAdvanceV1(empty,&tuple,CURSOR_ID,&found) && !found); xrtValueRelease(empty);
    for (int64_t key=1023;key>=0;--key) assert(xl6ListSetV1(list,&tuple,key,&key));
    xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    for (int64_t i=0;i<1024;++i) {
        int64_t key=-1,item=-1;
        assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
        assert(xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&item) && key==i && item==i);
        if (i==7) { xrtValueRelease(list); list=NULL; }
    }
    /* Drop a second snapshot with its iterator still active. */
    xrtValueRelease(cursor);
    list=create(&tuple,XL6_VALUE_ADMIT_I64); assert(list); int64_t item=1;
    assert(xl6ListSetV1(list,&tuple,1,&item));
    cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
    xrtValueRelease(cursor); xrtValueRelease(list);
}
static bool foreignRetain(void* data,void** out,void* context)
{ (void)context; *out=data; return true; }
static void foreignDrop(void* data,void* context) { (void)data; (void)context; }
static const xvaluehandleops foreignOps={foreignRetain,foreignDrop,NULL,NULL};
static void rejected(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* bytes=create(&tuple,XL6_VALUE_ADMIT_BYTES);
    xvalue* any=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(bytes && any);
    XL6_SequenceAdmissionV3 a={0},b={0};
    xvalue* witness=seed(&tuple,XL6_VALUE_ADMIT_BYTES); assert(witness);
    assert(xl6ListDomainCopyV1(&tuple,witness,&a)); xrtValueRelease(witness);
    witness=seed(&tuple,XL6_VALUE_ADMIT_ANY); assert(witness);
    assert(xl6ListDomainCopyV1(&tuple,witness,&b)); xrtValueRelease(witness);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6ListAdmitsSchemaV1(bytes,&tuple,&a) && !xl6ListAdmitsSchemaV1(any,&tuple,&a));
    assert(!xl6ListAdmitsSchemaV1(bytes,&tuple,&b) && xrtGetError()==prior); xrtClearError();
    xvalue* item=xrtValueString((xstrview)XRT_STR_INIT("not bytes")); assert(item);
    assert(!xl6ListSetV1(bytes,&tuple,1,&item)); xrtClearError();
    assert(xl6ListCountV1(bytes,&tuple)==0);
    XL6_ListLayoutV1 bad=tuple; ++bad.value.elementIdentity;
    assert(xl6ListCountV1(bytes,&bad)==-1); xrtClearError();
    xvalue* wire=xrtValueIntMap(); assert(wire && xrtValueTypeIdBind(wire,LIST_ID));
    assert(!xl6ListAdmitsSchemaV1(wire,&tuple,&a));
    assert(xl6ListCountV1(wire,&tuple)==-1); xrtClearError(); xrtValueRelease(wire);
    void* invalid=(void*)(uintptr_t)1;
    xvalue* foreign=xrtValueHandleTake(&invalid,&foreignOps,NULL); assert(foreign && xrtValueTypeIdBind(foreign,LIST_ID));
    assert(!xl6ListAdmitsSchemaV1(foreign,&tuple,&a));
    assert(xl6ListCountV1(foreign,&tuple)==-1); xrtClearError(); xrtValueRelease(foreign);
    witness=seed(&tuple,XL6_VALUE_ADMIT_ANY); assert(witness);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(witness,XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID,3,sizeof(xvalue*),xrtTypeValue());
    assert(pin && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&item)); xl6SequenceEndV1(pin);
    assert(!xl6ListCreateTakeV1(&tuple,witness)); xrtClearError();
    xl6SequenceAdmissionDropV3(&a); xl6SequenceAdmissionDropV3(&b);
    xrtValueRelease(item); xrtValueRelease(bytes); xrtValueRelease(any);
    XL6_ListLayoutV1 narrow=layout(1,xrtTypeInt64());
    xvalue* small=create(&narrow,XL6_VALUE_ADMIT_U8); assert(small);
    witness=seed(&narrow,XL6_VALUE_ADMIT_I64); assert(witness);
    assert(xl6ListDomainCopyV1(&narrow,witness,&a) && !xl6ListAdmitsSchemaV1(small,&narrow,&a));
    xrtValueRelease(witness); xl6SequenceAdmissionDropV3(&a);
    int64_t number=256; assert(!xl6ListSetV1(small,&narrow,0,&number)); xrtClearError();
    number=255; assert(xl6ListSetV1(small,&narrow,0,&number)); xrtValueRelease(small);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6ListCreateTakeV1(&tuple,NULL));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok); xrtErrorFree(outcome.error);
}
typedef struct Edges { unsigned count; xrtownershipref first; bool same; } Edges;
static bool edge(xrtownershipref ref,void* context)
{
    Edges* edges=context;
    if (!edges->count) { edges->first=ref; edges->same=true; }
    else edges->same=edges->same && edges->first.Data==ref.Data && edges->first.Ops==ref.Ops;
    ++edges->count; return true;
}
static xrtownershipref ownerOf(const xvalue* value)
{
    xrtownershipref root=xrtValueOwnership(value); Edges edges={0};
    assert(root.Ops->Trace(root.Data,edge,&edges) && edges.count==1); return edges.first;
}
static void references(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(list);
    xvalue* item=xrtValueArray(); assert(item && xrtValueArrayAppendNew(item,xrtValueInt(42)));
    assert(xl6ListSetV1(list,&tuple,-1,&item) && xl6ListSetV1(list,&tuple,5,&item));
    xvalue* copy=xl6ListCloneV1(list,&tuple); assert(copy);
    xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    xvalue* shell=xrtValueClone(list); assert(shell==list);
    xrtownershipref owner=ownerOf(list),alias=ownerOf(shell); size_t count=0;
    assert(owner.Data==alias.Data && owner.Ops==alias.Ops && owner.Ops->Count(owner.Data,&count) && count==1);
    assert(xl6ListOwnershipAdapterV1(owner));
    for (unsigned i=0;i<3;++i) {
        xrtownershipref actual=ownerOf(i==0 ? list : i==1 ? copy : cursor); Edges slots={0};
        assert(actual.Ops->Trace(actual.Data,edge,&slots) && slots.count==2 && slots.same);
        xrtownershipref expected=xrtValueOwnership(item);
        assert(slots.first.Data==expected.Data && slots.first.Ops==expected.Ops);
    }
    bool advanced=false; assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&advanced) && advanced);
    xrtownershipref active=ownerOf(cursor); Edges activeSlots={0};
    assert(active.Ops->Trace(active.Data,edge,&activeSlots) && activeSlots.count==2 && activeSlots.same);
    xvalue* out=NULL; bool found=false;
    assert(xl6ListGetV1(copy,&tuple,5,&out,false,&found) && found && out==item); xrtValueRelease(out);
    xrtValueRelease(shell); xrtValueRelease(copy); xrtValueRelease(cursor); xrtValueRelease(list); xrtValueRelease(item);
}
static void futures(void)
{
    XL6_ListLayoutV1 tuple=layout(XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,xrtTypeFuture());
    xvalue* list=create(&tuple,0); assert(list);
    xfuture* item=NULL; xpromise* promise=xrtPromiseCreate(&item,NULL); assert(promise && item);
    assert(xl6ListSetV1(list,&tuple,1,&item) && xl6ListSetV1(list,&tuple,2,&item));
    xrtownershipref owner=ownerOf(list); Edges slots={0};
    assert(!xl6ListOwnershipAdapterV1(owner));
    const XL6_SequenceAdmissionOpsV2* allowed[]={xl6ArrayFutureAdmissionOpsV2Get()};
    const XL6_SequenceAdmissionOpsV2* wrong[]={(const XL6_SequenceAdmissionOpsV2*)(uintptr_t)1};
    assert(!xl6ListOwnershipAdapterV2(owner,wrong,1));
    assert(xl6ListOwnershipAdapterV2(owner,allowed,1));
    assert(owner.Ops->Trace(owner.Data,edge,&slots) && slots.count==2 && slots.same);
    xrtownershipref expected=xrtFutureOwnership(item);
    assert(slots.first.Data==expected.Data && slots.first.Ops==expected.Ops);
    xfuture* out=NULL; bool found=false;
    assert(xl6ListGetV1(list,&tuple,1,&out,false,&found) && found && out==item); xrtFutureDestroy(out);
    xvalue* copy=xl6ListCloneV1(list,&tuple); assert(copy);
    xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    xrtValueRelease(list); xrtValueRelease(copy); xrtPromiseDestroy(promise); xrtFutureDestroy(item);
    assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
    int64_t key=0; out=NULL;
    assert(xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&out) && key==1 && out==item);
    xrtFutureDestroy(out); xrtValueRelease(cursor);
    tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequenceFutureCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID+1,ELEMENT_ID,true);
    assert(witness); list=xl6ListCreateTakeV1(&tuple,witness); assert(list);
    item=NULL; assert(xl6ListSetV1(list,&tuple,-1,&item));
    owner=ownerOf(list); slots=(Edges){0};
    assert(xl6ListOwnershipAdapterV2(owner,allowed,1) && owner.Ops->Trace(owner.Data,edge,&slots) && !slots.count);
    xrtValueRelease(list);
}
static void phased(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(list);
    /* Actual self edges, not a synthetic graph returned by a test callback. */
    assert(xl6ListSetV1(list,&tuple,1,&list) && xl6ListSetV1(list,&tuple,2,&list));
    xrtownershipref owner=ownerOf(list); Edges slots={0};
    assert(owner.Ops->Trace(owner.Data,edge,&slots) && slots.count==2 && slots.same);
    const xrtownershipadapterv1* adapter=xl6ListOwnershipAdapterV1(owner); assert(adapter);
    unsigned token=0; xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token));
    adapter->Restore(owner.Data,&token); assert(adapter->Claim(owner.Data,&token));
    assert(xrtOwnershipScopeEnd(&freeze));
    assert(xl6ListCountV1(list,&tuple)==2); /* Finalization precedes Clear. */
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(adapter->Finish(owner.Data,&token) && xrtGetError()==prior);
    assert(adapter->Finish(owner.Data,&token) && xrtGetError()==prior); xrtClearError();
    adapter->Drop(owner.Data); xrtValueRelease(list);
    list=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(list);
    assert(xl6ListSetV1(list,&tuple,1,&list)); owner=ownerOf(list);
    adapter=xl6ListOwnershipAdapterV1(owner); assert(adapter);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token));
    adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    xl6ErrorScopeCaptureFailure(&scope); /* Failed outcome with NULL first diagnostic. */
    assert(adapter->Finish(owner.Data,&token) && adapter->Finish(owner.Data,&token));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error);
    adapter->Drop(owner.Data); xrtValueRelease(list);
}
typedef struct Reentrant { xvalue* list; XL6_ListLayoutV1 tuple; unsigned drops; bool noDiagnostic; } Reentrant;
static void callbackDrop(void* data,void* context)
{
    Reentrant* state=data; assert(state==context); ++state->drops;
    /* Inject a genuinely explicit checked failure without a diagnostic.
     * This is not an allocator-fault claim: XRT's state error is static. */
    if (state->noDiagnostic) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    assert(xl6ListCountV1(state->list,&state->tuple)==-1);
    xrtValueRelease(state->list); state->list=NULL;
}
static const xvaluehandleops callbackOps={foreignRetain,callbackDrop,NULL,NULL};
static void reentrant(void)
{
    for (unsigned fail=0;fail<2;++fail) {
        Reentrant state={.tuple=layout(3,xrtTypeValue()),.noDiagnostic=fail};
        state.list=create(&state.tuple,XL6_VALUE_ADMIT_ANY); assert(state.list);
        void* raw=&state; xvalue* item=xrtValueHandleTake(&raw,&callbackOps,&state); assert(item);
        assert(xl6ListSetV1(state.list,&state.tuple,1,&item)); xrtValueRelease(item);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (fail) assert(xrtMemDebugFailAfter(0));
        assert(!xl6ListClearV1(state.list,&state.tuple));
        bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
        assert(!outcome.ok && state.drops==1 && !state.list && !hit);
        assert(fail ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_STATE);
        xrtErrorFree(outcome.error);
    }
}
static void finalizerError(void* data,void* context)
{
    (void)context; ++*(unsigned*)data; xrtSetErrorKind(XERR_VALUE);
}
static const xvaluehandleops finalizerOps={foreignRetain,finalizerError,NULL,NULL};
static void boundaries(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    for (unsigned mode=0;mode<3;++mode) {
        unsigned drops=0; xvalue* list=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(list);
        void* raw=&drops; xvalue* item=xrtValueHandleTake(&raw,&finalizerOps,NULL); assert(item);
        assert(xl6ListSetV1(list,&tuple,1,&item)); xrtValueRelease(item);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        const xerror* prior=NULL;
        if (!mode) { xrtSetErrorKind(XERR_ARGUMENT); prior=xrtGetError(); assert(prior); }
        cleanupFailure=mode==2 ? -1 : 1;
        xrtValueRelease(list); assert(drops==1 && !cleanupFailure);
        assert(mode==2 ? !xrtGetError() : xrtGetError() && (!mode ? xrtGetError()==prior : xrtErrorKind(xrtGetError())==XERR_MEMORY));
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(mode==2 ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==(!mode ? XERR_ARGUMENT : XERR_MEMORY));
        xrtErrorFree(outcome.error);
    }
    /* A checked Finish boundary failure must leave detached storage available
     * for retry, rather than leak it or return false after silently losing it. */
    tuple=layout(1,xrtTypeInt64()); xvalue* list=create(&tuple,XL6_VALUE_ADMIT_I64); assert(list);
    int64_t item=7; assert(xl6ListSetV1(list,&tuple,1,&item));
    xrtownershipref owner=ownerOf(list);
    const xrtownershipadapterv1* adapter=xl6ListOwnershipAdapterV1(owner); assert(adapter);
    unsigned token=0; xrtownershipscope freeze={0}; assert(xrtOwnershipFreezeTryBegin(&freeze));
    assert(adapter->Hold(owner.Data) && adapter->Claim(owner.Data,&token));
    adapter->Clear(owner.Data,&token); assert(xrtOwnershipScopeEnd(&freeze));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); cleanupFailure=1;
    assert(!adapter->Finish(owner.Data,&token) && !cleanupFailure); xl6ErrorScopeCaptureFailure(&scope);
    assert(adapter->Finish(owner.Data,&token) && adapter->Finish(owner.Data,&token));
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY); xrtErrorFree(outcome.error);
    adapter->Drop(owner.Data); xrtValueRelease(list);
}
static void fault(const char* operation,size_t point)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1());
    xvalue* list=NULL,*result=NULL,*cursor=NULL;
    xstrview item={"a\0\xE4\xBD\xA0",5},changed=XRT_STR_INIT("replacement"),output={0};
    xstrview original=XRT_STR_INIT("keep");
    assert(xrtTypeCopyValue(tuple.value.type,&output,&original));
    if (strcmp(operation,"create")) {
        list=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(list && xl6ListSetV1(list,&tuple,7,&item));
    }
    if (!strcmp(operation,"cursor-read") || !strcmp(operation,"cursor-advance")) {
        cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
        if (!strcmp(operation,"cursor-read")) { bool found=false; assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found); }
    }
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(xrtMemDebugFailAfter(point)); bool success=false,found=false; int64_t key=99;
    if (!strcmp(operation,"create")) { result=create(&tuple,XL6_VALUE_ADMIT_STRING); success=result!=NULL; }
    else if (!strcmp(operation,"insert")) success=xl6ListSetV1(list,&tuple,42,&changed);
    else if (!strcmp(operation,"replace")) success=xl6ListSetV1(list,&tuple,7,&changed);
    else if (!strcmp(operation,"append")) success=xl6ListAppendV1(list,&tuple,&changed,&key);
    else if (!strcmp(operation,"clone")) { result=xl6ListCloneV1(list,&tuple); success=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); success=result!=NULL; }
    else if (!strcmp(operation,"get")) success=xl6ListGetV1(list,&tuple,7,&output,false,&found);
    else if (!strcmp(operation,"take")) success=xl6ListGetV1(list,&tuple,7,&output,true,&found);
    else if (!strcmp(operation,"cursor-read")) success=xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&output);
    else if (!strcmp(operation,"cursor-advance")) success=xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found);
    else if (!strcmp(operation,"clear")) success=xl6ListClearV1(list,&tuple);
    else if (!strcmp(operation,"trim")) success=xl6ListTrimV1(list,&tuple,0)>=0;
    else assert(0);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok : success && outcome.ok);
    bool read=!strcmp(operation,"get") || !strcmp(operation,"take") || !strcmp(operation,"cursor-read");
    if (read) assert(hit ? !found && key==99 && output.Size==original.Size && !memcmp(output.Data,original.Data,original.Size) :
        output.Size==item.Size && !memcmp(output.Data,item.Data,item.Size));
    if (!strcmp(operation,"append")) assert(hit ? key==99 : key==8);
    if (!strcmp(operation,"cursor-read")) assert(hit ? key==99 : key==7);
    if (list) {
        int64_t count=hit ? 1 : (!strcmp(operation,"insert") || !strcmp(operation,"append")) ? 2 :
            (!strcmp(operation,"take") || !strcmp(operation,"clear")) ? 0 : 1;
        assert(xl6ListCountV1(list,&tuple)==count);
        if (count) {
            xstrview actual={0}; bool exists=false;
            const xstrview* expected=!hit && !strcmp(operation,"replace") ? &changed : &item;
            assert(xl6ListGetV1(list,&tuple,7,&actual,false,&exists) && exists);
            assert(actual.Size==expected->Size && !memcmp(actual.Data,expected->Data,expected->Size));
            xrtTypeDropValue(tuple.value.type,&actual);
        }
    }
    xrtTypeDropValue(tuple.value.type,&output); xrtErrorFree(outcome.error);
    xrtValueRelease(result); xrtValueRelease(cursor); xrtValueRelease(list); xrtClearError();
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3); xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
    else if (!strcmp(argv[1],"integers")) integers();
    else if (!strcmp(argv[1],"text")) text();
    else if (!strcmp(argv[1],"profiles")) profiles();
    else if (!strcmp(argv[1],"cursors")) cursors();
    else if (!strcmp(argv[1],"rejected")) rejected();
    else if (!strcmp(argv[1],"references")) references();
    else if (!strcmp(argv[1],"futures")) futures();
    else if (!strcmp(argv[1],"phased")) phased();
    else if (!strcmp(argv[1],"reentrant")) reentrant();
    else if (!strcmp(argv[1],"boundaries")) boundaries();
    else assert(0);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    if (argc==2) printf("native List %s vectors balanced\n",argv[1]);
    return 0;
}
