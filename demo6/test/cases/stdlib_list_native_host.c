#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_list_schema.h"
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
static void equalLists(const xvalue*,const xvalue*,const XL6_ListLayoutV1*,bool);
extern void xl6TestListSchemaVectorsV1(const char*);
extern bool xl6TestListSchemaFaultV1(const char*,size_t);
#ifdef XL6_TEST_LIST_GRAPH
extern void xl6TestListGraphVectorsV1(const char*);
extern bool xl6TestListGraphFaultV1(const char*,size_t);
#endif
/* Linker call-boundary fault injection, not a replacement List/provider.
 * Negative budget models explicit failure without an allocated diagnostic. */
static int cleanupFailure;
static int entryFailure;
#ifdef XL6_TEST_LIST_GRAPH
void xl6TestListGraphBoundaryFailureV1(int entry,int cleanup)
{ entryFailure=entry; cleanupFailure=cleanup; }
#endif
extern bool __real_xl6ErrorScopeBegin(xl6_error_scope*);
bool __wrap_xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (entryFailure) {
        int selected=entryFailure; entryFailure=0;
        if (selected>0) xrtSetErrorKind(XERR_MEMORY); else xrtClearError();
        return false;
    }
    return __real_xl6ErrorScopeBegin(scope);
}
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
    xvalue* copy=xl6ListCloneV1(list,&tuple); assert(copy); equalLists(list,copy,&tuple,true); xrtValueRelease(copy);
    xrtValueRelease(list);
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID+1,
        2,sizeof(xstrview),tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID);
    assert(witness); list=xl6ListCreateTakeV1(&tuple,witness); assert(list);
    xstrview absent={0},present={"a\0b",3},read={0};
    assert(xl6ListSetV1(list,&tuple,-1,&absent) && xl6ListSetV1(list,&tuple,2,&present));
    copy=xl6ListCloneV1(list,&tuple); assert(copy); equalLists(list,copy,&tuple,true); xrtValueRelease(copy);
    assert(xl6ListGetV1(list,&tuple,2,&read,false,&found) && found && read.Size==3 && !memcmp(read.Data,present.Data,3));
    assert(xl6ListGetV1(list,&tuple,-1,&read,false,&found) && found && !read.Data && !read.Size);
    absent.Size=1; assert(!xl6ListSetV1(list,&tuple,3,&absent)); xrtClearError();
    xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    assert(xl6ListClearV1(list,&tuple)); xrtValueRelease(list);
    int64_t key=0;
    assert(xl6ListCursorAdvanceV1(cursor,&tuple,CURSOR_ID,&found) && found);
    assert(xl6ListCursorReadV1(cursor,&tuple,CURSOR_ID,&key,&read) && key==-1 && !read.Data && !read.Size);
    xrtTypeDropValue(tuple.value.type,&read); xrtValueRelease(cursor);
    list=create(&tuple,XL6_VALUE_ADMIT_STRING); /* Reject optional physical storage with a nonnullable policy. */
    assert(!list); xrtClearError();
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
    bool same=false; assert(xl6ListEqualsV1(list,copy,&tuple,&same) && same && xrtValueEqual(list,copy));
    uint64_t hash=23; assert(!xl6ListHashV1(list,&tuple,&hash) && hash==23);
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
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
    equalLists(list,copy,&tuple,true);
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
    copy=xl6ListCloneV1(list,&tuple); assert(copy); equalLists(list,copy,&tuple,true); xrtValueRelease(copy);
    xrtValueRelease(list);
}
static void phased(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(list);
    /* Actual self edges, not a synthetic graph returned by a test callback. */
    assert(xl6ListSetV1(list,&tuple,1,&list) && xl6ListSetV1(list,&tuple,2,&list));
    XL6_ListLayoutV1 schemaLayout={0}; XL6_SequenceAdmissionV3 schema={0};
    assert(xl6ListSchemaCopyV1(list,&schemaLayout,&schema));
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
    XL6_ListLayoutV1 unchanged={.identity=777}; XL6_SequenceAdmissionV3 empty={0};
    assert(!xl6ListSchemaCopyV1(list,&unchanged,&empty) && unchanged.identity==777 && !empty.ops);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_STATE);
    const xerror* clearedError=xrtGetError();
    assert(!xl6ListAdmitsSchemaV1(list,&schemaLayout,&schema) && xrtGetError()==clearedError); xrtClearError();
    xl6SequenceAdmissionDropV3(&schema);
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
static void equalLists(const xvalue* a,const xvalue* b,const XL6_ListLayoutV1* tuple,bool expected)
{
    bool result=!expected;
    assert(xl6ListEqualsV1(a,b,tuple,&result) && result==expected);
    assert(xrtValueEqual(a,b)==expected && !xrtGetError());
    if (expected) {
        uint64_t first=0,second=1,dynamic=2,dynamicPeer=3;
        assert(xl6ListHashV1(a,tuple,&first) && xl6ListHashV1(b,tuple,&second) && first==second);
        assert(xrtValueHash(a,&dynamic) && xrtValueHash(b,&dynamicPeer) && dynamic==dynamicPeer);
    }
}
static void identity(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* a=create(&tuple,XL6_VALUE_ADMIT_I64),*b=create(&tuple,XL6_VALUE_ADMIT_I64); assert(a && b);
    const int64_t keys[]={INT64_MIN,-900,0,42,INT64_MAX};
    for (unsigned i=0;i<5;++i) { int64_t value=i; assert(xl6ListSetV1(a,&tuple,keys[i],&value)); }
    for (unsigned i=5;i-->0;) { int64_t value=i; assert(xl6ListSetV1(b,&tuple,keys[i],&value)); }
    equalLists(a,b,&tuple,true);
    int64_t value=17; assert(xl6ListSetV1(b,&tuple,42,&value)); equalLists(a,b,&tuple,false);
    bool found=false; assert(xl6ListContainsV1(b,&tuple,&value,&found) && found);
    value=99; assert(xl6ListContainsV1(b,&tuple,&value,&found) && !found);
    xvalue* copy=xl6ListCloneV1(a,&tuple); assert(copy); equalLists(a,copy,&tuple,true);
    assert(xl6ListKeyV1(copy,&tuple,INT64_MIN,true,&found) && found);
    assert(xl6ListSetV1(copy,&tuple,INT64_MIN+1,&(int64_t){0})); equalLists(a,copy,&tuple,false);
    xvalue* cursor=xl6ListCursorCreateV1(a,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    uint64 hash=23; assert(!xrtValueHash(cursor,&hash) && hash==23);
    assert(xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED); xrtClearError(); xrtValueRelease(cursor);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError(); assert(prior);
    assert(xl6ListHashV1(a,&tuple,&hash) && xrtGetError()==prior);
    assert(xl6ListEqualsV1(a,b,&tuple,&found) && !found && xrtGetError()==prior); xrtClearError();
    /* Keep structural keys stable while stored; identity is not a freeze API. */
    xvalue* set=xrtValueSet(); assert(set && xrtValueSetAdd(set,a));
    xvalue* same=xl6ListCloneV1(a,&tuple); assert(same && xrtValueSetAdd(set,same));
    assert(xrtValueCount(set)==1 && xrtValueSetHas(set,a)); xrtValueRelease(set); xrtValueRelease(same);
    xrtValueRelease(copy); xrtValueRelease(b); xrtValueRelease(a);
    tuple=layout(2,xl6StringTypeV1()); a=create(&tuple,XL6_VALUE_ADMIT_STRING); b=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(a && b);
    xstrview full={"a\0b",3},shorter=XRT_STR_INIT("a");
    assert(xl6ListSetV1(a,&tuple,-7,&full) && xl6ListSetV1(b,&tuple,-7,&shorter)); equalLists(a,b,&tuple,false);
    assert(xl6ListSetV1(b,&tuple,-7,&full)); equalLists(a,b,&tuple,true);
    assert(xl6ListContainsV1(a,&tuple,&full,&found) && found);
    assert(xl6ListContainsV1(a,&tuple,&shorter,&found) && !found);
    xrtValueRelease(b); xrtValueRelease(a);
    tuple=layout(3,xrtTypeValue()); a=create(&tuple,XL6_VALUE_ADMIT_ANY); b=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(a && b);
    xvalue* absent=NULL,*canonical=xrtValueNull();
    assert(xl6ListSetV1(a,&tuple,-1,&absent) && xl6ListSetV1(b,&tuple,-1,&canonical));
    xvalue* x=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3});
    xvalue* y=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(x && y && x!=y);
    assert(xl6ListSetV1(a,&tuple,INT64_MAX,&x) && xl6ListSetV1(b,&tuple,INT64_MAX,&y)); equalLists(a,b,&tuple,true);
    assert(xl6ListContainsV1(a,&tuple,&y,&found) && found);
    xrtValueRelease(y); xrtValueRelease(x); xrtValueRelease(b); xrtValueRelease(a);
}
static void merges(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xrtTypeInt64());
    xvalue* target=create(&tuple,XL6_VALUE_ADMIT_I64),*source=create(&tuple,XL6_VALUE_ADMIT_I64); assert(target && source);
    assert(xl6ListSetV1(target,&tuple,-9,&(int64_t){7}) && xl6ListSetV1(source,&tuple,-9,&(int64_t){8}));
    assert(xl6ListSetV1(source,&tuple,INT64_MAX,&(int64_t){12}));
    assert(xl6ListMergeV1(target,source,&tuple,false) && xl6ListCountV1(target,&tuple)==2);
    int64_t out=0; bool found=false;
    assert(xl6ListGetV1(target,&tuple,-9,&out,false,&found) && found && out==7);
    assert(xl6ListMergeV1(target,source,&tuple,true)); equalLists(target,source,&tuple,true);
    assert(xl6ListMergeV1(target,target,&tuple,false) && xl6ListMergeV1(target,target,&tuple,true));
    xvalue* narrow=create(&tuple,XL6_VALUE_ADMIT_U8); assert(narrow);
    bool equal=true; assert(!xl6ListEqualsV1(target,narrow,&tuple,&equal) && equal); xrtClearError();
    assert(!xl6ListMergeV1(target,narrow,&tuple,true)); xrtClearError();
    assert(!xrtValueEqual(target,narrow) && !xrtGetError());
    xrtValueRelease(narrow); xrtValueRelease(source); xrtValueRelease(target);
}
typedef struct IdentityFailure { unsigned hashes,equals; bool noDiagnostic; } IdentityFailure;
static uint64 failingHash(void* data,void* context)
{
    (void)context; IdentityFailure* state=data; ++state->hashes;
    if (state->noDiagnostic) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else xrtSetErrorKind(XERR_VALUE);
    return 17; /* A returned payload is not proof of success. */
}
static bool failingEqual(void* left,void* right,void* context)
{
    (void)right; (void)context; IdentityFailure* state=left; ++state->equals;
    if (state->noDiagnostic) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    else xrtSetErrorKind(XERR_VALUE);
    return true;
}
static const xvaluehandleops failingOps={foreignRetain,foreignDrop,failingHash,failingEqual};
static void identityErrors(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    for (unsigned absent=0;absent<2;++absent) {
        IdentityFailure state={.noDiagnostic=absent};
        void* raw=&state; xvalue* x=xrtValueHandleTake(&raw,&failingOps,NULL); assert(x);
        raw=&state; xvalue* y=xrtValueHandleTake(&raw,&failingOps,NULL); assert(y && x!=y);
        xvalue* a=create(&tuple,XL6_VALUE_ADMIT_ANY),*b=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(a && b);
        for (int64_t key=0;key<2;++key) assert(xl6ListSetV1(a,&tuple,key,&x) && xl6ListSetV1(b,&tuple,key,&y));
        for (unsigned operation=0;operation<5;++operation) {
            state.hashes=state.equals=0;
            xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
            uint64_t hash=23; bool output=false,success=false;
            if (operation==0) success=xl6ListHashV1(a,&tuple,&hash);
            if (operation==1) success=xrtValueHash(a,&hash);
            if (operation==2) success=xl6ListEqualsV1(a,b,&tuple,&output);
            if (operation==3) success=xrtValueEqual(a,b);
            if (operation==4) success=xl6ListContainsV1(a,&tuple,&y,&output);
            assert(!success && !output && hash==23);
            xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
            assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_VALUE);
            assert(operation<2 ? state.hashes==1 && !state.equals : state.equals==1 && !state.hashes);
            xrtErrorFree(outcome.error);
        }
        xrtValueRelease(b); xrtValueRelease(a); xrtValueRelease(y); xrtValueRelease(x);
    }
}
static xvalue* nativeArray(int64_t item)
{
    xvalue* value=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID,1,sizeof(item),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,ELEMENT_ID); assert(value);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID,1,sizeof(item),xrtTypeInt64());
    assert(pin && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&item)); xl6SequenceEndV1(pin); return value;
}
static void sharedDepth(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* x=nativeArray(7),*y=nativeArray(7);
    xvalue* a=create(&tuple,XL6_VALUE_ADMIT_ANY),*b=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(a && b);
    assert(xl6ListSetV1(a,&tuple,1,&x) && xl6ListSetV1(b,&tuple,1,&y)); equalLists(a,b,&tuple,true);
    XL6_CollectionIdentityFrameV1 frames[XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1]={0};
    for (unsigned i=0;i<XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1-1;++i) assert(xl6CollectionIdentityEnterV1(&frames[i]));
    for (unsigned operation=0;operation<3;++operation) {
        bool output=true,success=false; uint64_t hash=23;
        if (operation==0) success=xl6ListHashV1(a,&tuple,&hash);
        if (operation==1) success=xl6ListEqualsV1(a,b,&tuple,&output);
        if (operation==2) success=xl6ListContainsV1(a,&tuple,&y,&output);
        assert(!success && output && hash==23 && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    }
    for (unsigned i=XL6_SEQUENCE_IDENTITY_DEPTH_MAX_V1-1;i-->0;) xl6CollectionIdentityLeaveV1(&frames[i]);
    equalLists(a,b,&tuple,true);
    xrtValueRelease(b); xrtValueRelease(a); xrtValueRelease(y); xrtValueRelease(x);
}
static void guardProtocol(void)
{
    assert(!xl6CollectionIdentityEnterV1(NULL) && xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    XL6_CollectionIdentityFrameV1 first={0},second={0};
    assert(xl6CollectionIdentityEnterV1(&first));
    assert(!xl6CollectionIdentityEnterV1(&first) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    assert(xl6CollectionIdentityEnterV1(&second));
    assert(!xl6CollectionIdentityEnterV1(&first) && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    xl6CollectionIdentityLeaveV1(&second); xl6CollectionIdentityLeaveV1(&first);
    assert(!first.next && !first.thread && !first.depth && !second.next && !second.thread && !second.depth);
    assert(xl6CollectionIdentityEnterV1(&first)); xl6CollectionIdentityLeaveV1(&first);
}
static xvalue* deepValue(void)
{
    xvalue* value=xrtValueInt(7); assert(value);
    for (unsigned i=0;i<48;++i) {
        xvalue* parent=xrtValueObject(); assert(parent);
        assert(xrtValueObjectSetNew(parent,(xstrview)XRT_STR_INIT("child"),value)); value=parent;
    }
    return value;
}
static void equalityFault(size_t point)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    xvalue* a=create(&tuple,XL6_VALUE_ADMIT_ANY),*b=create(&tuple,XL6_VALUE_ADMIT_ANY); assert(a && b);
    xvalue* x=deepValue(),*y=deepValue();
    assert(xl6ListSetV1(a,&tuple,INT64_MIN,&x) && xl6ListSetV1(b,&tuple,INT64_MIN,&y));
    xrtValueRelease(y); xrtValueRelease(x);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(xrtMemDebugFailAfter(point)); bool output=false;
    bool success=xl6ListEqualsV1(a,b,&tuple,&output);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok && !output : success && outcome.ok && output);
    if (hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
    assert(xl6ListCountV1(a,&tuple)==1 && xl6ListCountV1(b,&tuple)==1);
    assert(xl6ListEqualsV1(a,b,&tuple,&output) && output && xrtValueEqual(a,b));
    xrtErrorFree(outcome.error); xrtValueRelease(b); xrtValueRelease(a);
    printf("native List ref-equal point=%zu hit=%d complete-prefix balanced\n",point,hit);
}
static xvalue* projected(const XL6_ListLayoutV1* tuple,unsigned rule,const xvalueconverter* codec)
{ return xl6ListProjectionTakeV1(create(tuple,rule),tuple,codec); }
static void readText(const xvalue* list,int64_t key,xstrview expected)
{
    bool found=false; xvalue* item=NULL; xstrview actual={0};
    assert(xl6ListErasedReadV1(list,key,false,&item,&found) && found && item);
    assert(xrtValueTypeId(item)==ELEMENT_ID && xrtValueGetString(item,&actual));
    assert(actual.Size==expected.Size && !memcmp(actual.Data,expected.Data,actual.Size)); xrtValueRelease(item);
}
static void projection(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1());
    xvalue* list=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(list);
    xstrview text={"a\0\xE4\xBD\xA0",5}; assert(xl6ListSetV1(list,&tuple,INT64_MIN,&text));
    bool found=true; xvalue* item=NULL;
    assert(!xl6ListErasedReadV1(list,INT64_MIN,false,&item,&found) && !item && found); xrtClearError();
    const xvalueconverter* codec=xl6StringConverterV1();
    xvalueconverter wrong=*codec; wrong.Context=&found;
    assert(!xl6ListEraseV1(list,&tuple,&wrong)); xrtClearError();
    wrong=*codec; wrong.FromTyped=NULL; assert(!xl6ListEraseV1(list,&tuple,&wrong)); xrtClearError();
    xvalue* alias=xl6ListEraseV1(list,&tuple,codec); assert(alias==list);
    xvalue* repeated=xl6ListEraseV1(list,&tuple,codec); assert(repeated==list); xrtValueRelease(repeated);
    assert(!xl6ListEraseV1(list,&tuple,NULL)); xrtClearError(); readText(alias,INT64_MIN,text);
    xvalue* replacement=xrtValueString((xstrview)XRT_STR_INIT("new")); assert(replacement);
    assert(xl6ListErasedSetV1(alias,INT64_MIN,replacement)); readText(list,INT64_MIN,(xstrview)XRT_STR_INIT("new"));
    assert(xl6ListErasedSetV1(list,INT64_MIN,xrtValueNull())==false); xrtClearError();
    int64_t key=99; assert(xl6ListErasedAppendV1(alias,replacement,&key) && key==INT64_MIN+1);
    assert(xl6ListSetV1(list,&tuple,INT64_MAX,&text)); key=99;
    assert(!xl6ListErasedAppendV1(list,replacement,&key) && key==99 && xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    xvalue* copy=xl6ListCloneV1(list,&tuple); assert(copy); readText(copy,INT64_MAX,text);
    assert(xl6ListClearV1(copy,&tuple)); readText(list,INT64_MAX,text);
    assert(xl6ListErasedReadV1(alias,17,false,&item,&found) && !item && !found);
    found=false; assert(xl6ListErasedReadV1(alias,INT64_MAX,true,&item,&found) && item && found);
    assert(xrtValueTypeId(item)==ELEMENT_ID); xrtValueRelease(item); item=NULL;
    assert(xl6ListErasedReadV1(alias,INT64_MAX,false,&item,&found) && !item && !found);
    xvalue* cursor=xl6ListCursorCreateV1(list,&tuple,CURSOR_ID,false,false,0); assert(cursor);
    assert(!xl6ListToWireV1(cursor)); xrtClearError(); xrtValueRelease(cursor);
    void* raw=(void*)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign);
    assert(!xl6ListToWireV1(foreign)); xrtClearError(); xrtValueRelease(foreign);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    readText(list,INT64_MIN,(xstrview)XRT_STR_INIT("new")); assert(xrtGetError()==prior); xrtClearError();
    xrtValueRelease(replacement); xrtValueRelease(copy); xrtValueRelease(alias); xrtValueRelease(list);
}
static void projectionProfiles(void)
{
    XL6_ListLayoutV1 tuple=layout(1,xl6CharTypeV1());
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_CHAR,xl6CharConverterV1()); assert(list);
    uint32_t character=0x4F60; assert(xl6ListSetV1(list,&tuple,-1,&character));
    xvalue* item=NULL; bool found=false;
    assert(xl6ListErasedReadV1(list,-1,false,&item,&found) && found && xrtValueType(item)==XVALUE_CHAR);
    assert(xrtValueTypeId(item)==ELEMENT_ID); xrtValueRelease(item);
    item=xrtValueInt(character); assert(item && !xl6ListErasedSetV1(list,-1,item)); xrtClearError(); xrtValueRelease(item); xrtValueRelease(list);
    tuple=layout(2,xl6NullableStringTypeV1()); tuple.value.elementIdentity=ELEMENT_ID+1;
    xvalue* witness=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,ARRAY_ID,ELEMENT_ID+1,2,sizeof(xstrview),
        tuple.value.type,XL6_VALUE_ADMIT_STRING,true,ELEMENT_ID); assert(witness);
    list=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&tuple,witness),&tuple,xl6NullableStringConverterV1()); assert(list);
    assert(xl6ListErasedSetV1(list,-1,xrtValueNull())); item=NULL;
    assert(xl6ListErasedReadV1(list,-1,false,&item,&found) && found && item==xrtValueNull()); xrtValueRelease(item);
    item=xrtValueString((xstrview){"\0",1}); assert(item && xl6ListErasedSetV1(list,-1,item)); xrtValueRelease(item); item=NULL;
    assert(xl6ListErasedReadV1(list,-1,false,&item,&found) && found && xrtValueTypeId(item)==ELEMENT_ID);
    xstrview text={0}; assert(xrtValueGetString(item,&text) && text.Size==1 && !text.Data[0]); xrtValueRelease(item); xrtValueRelease(list);
    tuple=layout(3,xrtTypeValue()); list=projected(&tuple,XL6_VALUE_ADMIT_BYTES,NULL); assert(list);
    item=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(item && xl6ListErasedSetV1(list,-7,item));
    xvalue* out=NULL; assert(xl6ListErasedReadV1(list,-7,false,&out,&found) && found && out==item);
    assert(!xl6ListEraseV1(list,&tuple,xl6StringConverterV1())); xrtClearError();
    xrtValueRelease(out); xrtValueRelease(item); xrtValueRelease(list);
    tuple=layout(XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1,xrtTypeFuture()); list=projected(&tuple,0,NULL); assert(list);
    xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise && future);
    assert(xl6ListSetV1(list,&tuple,1,&future)); out=NULL;
    assert(xl6ListErasedReadV1(list,1,false,&out,&found) && found && out && xrtValueIsFuture(out));
    assert(xrtValueTypeId(out)==ELEMENT_ID && xl6ListErasedSetV1(list,2,out));
    xrtValueRelease(out); xrtValueRelease(list); xrtFutureDestroy(future); xrtPromiseDestroy(promise);
}
static bool alternateDecode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{ return xl6StringConverterV1()->ToTyped(value,type,output,context); }
static xvalue* alternateEncode(const xrttype* type,const void* slot,ptr context)
{ return xl6StringConverterV1()->FromTyped(type,slot,context); }
static const xvalueconverter alternateCodec={.ToTyped=alternateDecode,.FromTyped=alternateEncode};
static void imports(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1()); xstrview text={"a\0b",3};
    xvalue* source=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(source);
    assert(xl6ListSetV1(source,&tuple,INT64_MIN,&text) && xl6ListSetV1(source,&tuple,INT64_MAX,&text));
    xvalue* wire=xl6ListToWireV1(source); assert(wire && xrtValueType(wire)==XVALUE_INT_MAP && xrtValueCount(wire)==2);
    const xvalue* boxed=xrtValueIntMapGet(wire,INT64_MIN); assert(boxed && xrtValueTypeId(boxed)==ELEMENT_ID);
    for (unsigned path=0;path<3;++path) {
        xvalue* target=projected(&tuple,XL6_VALUE_ADMIT_STRING,path==2 ? &alternateCodec : xl6StringConverterV1()); assert(target);
        xvalue* original=target;
        target=xl6ListImportTakeV1(target,path==0 ? wire : source); assert(target==original);
        equalLists(target,source,&tuple,true); readText(target,INT64_MIN,text); readText(target,INT64_MAX,text);
        assert(xl6ListClearV1(target,&tuple)); readText(source,INT64_MIN,text); xrtValueRelease(target);
    }
    xvalue* target=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(target);
    xvalue* alias=xrtValueClone(target); assert(alias==target);
    assert(!xl6ListImportTakeV1(target,alias)); xrtClearError(); assert(xl6ListCountV1(alias,&tuple)==0); xrtValueRelease(alias);
    target=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(target);
    /* A low-level host can hold two shells for the same genuine owner. This
     * is not a second storage/provider and must not bypass seed alias refusal. */
    void* raw=NULL; void* user=NULL; void* held=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(target,&raw,&ops,&user) && raw && ops && !user && ops->Clone(raw,&held,user));
    alias=xrtValueHandleTake(&held,ops,user); assert(alias && alias!=target && !held);
    assert(xrtValueTypeIdBind(alias,LIST_ID));
    assert(!xl6ListImportTakeV1(target,alias)); xrtClearError(); assert(xl6ListCountV1(alias,&tuple)==0); xrtValueRelease(alias);
    target=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(target);
    assert(xl6ListSetV1(target,&tuple,0,&text) && !xl6ListImportTakeV1(target,wire)); xrtClearError();
    target=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(target && !xl6ListImportTakeV1(target,wire)); xrtClearError();
    xvalue* wrong=xrtValueArray(); assert(wrong);
    target=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(target && !xl6ListImportTakeV1(target,wrong));
    xrtClearError(); xrtValueRelease(wrong);
    wrong=xrtValueIntMap(); assert(wrong);
    assert(xrtValueIntMapSetNew(wrong,-1,xrtValueInt(7)));
    target=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(target && !xl6ListImportTakeV1(target,wrong));
    xrtClearError(); assert(xrtValueCount(wrong)==1); xrtValueRelease(wrong);
    readText(source,INT64_MIN,text); readText(source,INT64_MAX,text); xrtValueRelease(wire); xrtValueRelease(source);
}
static unsigned codecFaultMode,codecCalls;
static bool codecFaultNull;
static xvalue* codecSource;
static void failCodec(void)
{
    if (codecFaultNull) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); else xrtSetErrorKind(XERR_VALUE);
}
static bool faultDecode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{
    ++codecCalls; bool result=alternateDecode(value,type,output,context);
    if (codecFaultMode==1) failCodec();
    if (codecFaultMode==3 && result) {
        xrtTypeDropValue(type,output); *(xstrview*)output=(xstrview){NULL,1};
    }
    return result;
}
static xvalue* faultEncode(const xrttype* type,const void* slot,ptr context)
{
    ++codecCalls; xvalue* result=alternateEncode(type,slot,context);
    if (codecFaultMode==2) failCodec();
    if (codecFaultMode==4 && codecSource) {
        xvalue* source=codecSource; codecSource=NULL; xrtValueRelease(source);
    }
    if (codecFaultMode==5) {
        xvalue* out=NULL; bool found=true;
        assert(codecSource && !xl6ListErasedReadV1(codecSource,-1,false,&out,&found) && !out && found);
        xrtClearError(); /* Clearing diagnostics cannot clear the failed bit. */
    }
    return result;
}
static const xvalueconverter faultCodec={.ToTyped=faultDecode,.FromTyped=faultEncode};
static void codecErrors(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1()); xstrview text=XRT_STR_INIT("original"),replacement=XRT_STR_INIT("replacement");
    for (unsigned absent=0;absent<2;++absent) for (unsigned operation=0;operation<5;++operation) {
        codecFaultMode=0; codecFaultNull=absent;
        xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_STRING,&faultCodec); assert(list);
        assert(xl6ListSetV1(list,&tuple,-1,&text) && xl6ListSetV1(list,&tuple,2,&text));
        xvalue* item=xrtValueString(replacement),*out=NULL; assert(item);
        bool found=true,success=false; int64_t key=99;
        codecCalls=0; codecFaultMode=operation<2 ? 1 : 2;
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        if (operation==0) success=xl6ListErasedSetV1(list,-1,item);
        if (operation==1) success=xl6ListErasedAppendV1(list,item,&key);
        if (operation==2) success=xl6ListErasedReadV1(list,-1,true,&out,&found);
        if (operation==3) { out=xl6ListToWireV1(list); success=out!=NULL; }
        if (operation==4) {
            xvalue* destination=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(destination);
            out=xl6ListImportTakeV1(destination,list); success=out!=NULL;
        }
        assert(!success && !out && found && key==99 && codecCalls==1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_VALUE);
        xrtErrorFree(outcome.error); codecFaultMode=0;
        assert(xl6ListCountV1(list,&tuple)==2); readText(list,-1,text); readText(list,2,text);
        xrtValueRelease(item); xrtValueRelease(list);
    }
    codecFaultMode=0;
    xvalue* list=projected(&tuple,XL6_VALUE_ADMIT_STRING,&faultCodec); assert(list);
    assert(xl6ListSetV1(list,&tuple,-1,&text) && xl6ListSetV1(list,&tuple,2,&text));
    xvalue* item=xrtValueString(replacement); assert(item); codecFaultMode=3;
    assert(!xl6ListErasedSetV1(list,-1,item) && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    codecFaultMode=0; readText(list,-1,text); xrtValueRelease(item);
    codecFaultMode=5; codecSource=list; codecCalls=0;
    assert(!xl6ListToWireV1(list) && codecCalls==1 && xrtErrorKind(xrtGetError())==XERR_STATE); xrtClearError();
    codecFaultMode=0; readText(list,-1,text); readText(list,2,text);
    codecFaultMode=4; codecSource=list;
    xvalue* wire=xl6ListToWireV1(list); assert(wire && !codecSource && xrtValueCount(wire)==2);
    xrtValueRelease(wire); codecFaultMode=0;
}
static bool projectionFault(const char* operation,size_t point)
{
    const char* names[]={"value-get","value-take","value-set","value-append","wire","import-wire","import-native","import-convert","projection-take"};
    bool recognized=false;
    for (unsigned i=0;i<sizeof(names)/sizeof(*names);++i) recognized=recognized || !strcmp(operation,names[i]);
    if (!recognized) return false;
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1()); xstrview text={"a\0b",3},replacement=XRT_STR_INIT("changed");
    xvalue* source=projected(&tuple,XL6_VALUE_ADMIT_STRING,xl6StringConverterV1()); assert(source);
    assert(xl6ListSetV1(source,&tuple,-7,&text) && xl6ListSetV1(source,&tuple,42,&text));
    bool import=!strncmp(operation,"import-",7),consume=!strcmp(operation,"projection-take");
    xvalue* target=import ? projected(&tuple,XL6_VALUE_ADMIT_STRING,!strcmp(operation,"import-convert") ? &alternateCodec : xl6StringConverterV1()) :
        consume ? create(&tuple,XL6_VALUE_ADMIT_STRING) : NULL;
    assert(!import && !consume ? !target : target!=NULL);
    xvalue* wire=!strcmp(operation,"import-wire") ? xl6ListToWireV1(source) : NULL;
    assert(strcmp(operation,"import-wire") || wire);
    xvalue* item=xrtValueString(replacement); assert(item); xvalue* result=NULL;
    bool found=true,success=false; int64_t key=99;
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    if (!strcmp(operation,"value-get") || !strcmp(operation,"value-take"))
        success=xl6ListErasedReadV1(source,-7,!strcmp(operation,"value-take"),&result,&found);
    else if (!strcmp(operation,"value-set")) success=xl6ListErasedSetV1(source,-7,item);
    else if (!strcmp(operation,"value-append")) success=xl6ListErasedAppendV1(source,item,&key);
    else if (!strcmp(operation,"wire")) { result=xl6ListToWireV1(source); success=result!=NULL; }
    else if (import) { result=xl6ListImportTakeV1(target,wire ? wire : source); target=NULL; success=result!=NULL; }
    else { result=xl6ListProjectionTakeV1(target,&tuple,xl6StringConverterV1()); target=NULL; success=result!=NULL; }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !success && !outcome.ok && !result : success && outcome.ok);
    if (hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
    int64_t count=2;
    if (!hit && !strcmp(operation,"value-take")) count=1;
    if (!hit && !strcmp(operation,"value-append")) count=3;
    assert(found && xl6ListCountV1(source,&tuple)==count);
    if (strcmp(operation,"value-take") || hit) readText(source,-7,!hit && !strcmp(operation,"value-set") ? replacement : text);
    readText(source,42,text);
    if (!strcmp(operation,"value-append")) assert(hit ? key==99 : key==43);
    if (import && !hit) { readText(result,-7,text); readText(result,42,text); }
    if (!strcmp(operation,"wire") && !hit) assert(xrtValueType(result)==XVALUE_INT_MAP && xrtValueCount(result)==2);
    if (wire) assert(xrtValueCount(wire)==2);
    xrtErrorFree(outcome.error); xrtValueRelease(result); xrtValueRelease(target); xrtValueRelease(wire); xrtValueRelease(item); xrtValueRelease(source);
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit); return true;
}
static void projectionEntryErrors(void)
{
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1());
    for (unsigned absent=0;absent<2;++absent) for (unsigned operation=0;operation<4;++operation) {
        /* Exact consumed ownership even when Begin never installs a scope.
         * The later genuine finalizer error must not replace Memory/NULL. */
        unsigned drops=0; void* raw=&drops;
        xvalue* seed=xrtValueHandleTake(&raw,&finalizerOps,NULL); assert(seed);
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        entryFailure=absent ? -1 : 1;
        xvalue* result=operation==3 ? xl6SequenceListCreateTakeV1(1,900,901,seed,false) :
            operation==2 ? xl6ListCreateTakeV1(&tuple,seed) :
            operation==1 ? xl6ListImportTakeV1(seed,xrtValueNull()) :
            xl6ListProjectionTakeV1(seed,&tuple,xl6StringConverterV1());
        assert(!result && !entryFailure && drops==1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
        xrtErrorFree(outcome.error);
    }
}
static void schemaCleanupErrors(void)
{
    XL6_ListLayoutV1 tuple=layout(3,xrtTypeValue());
    /* Capturing only metadata must not hide a real destructor failure while
     * the consumed populated child loses its last Value reference. */
    unsigned drops=0; void* raw=&drops;
    xvalue* item=xrtValueHandleTake(&raw,&finalizerOps,NULL),*child=create(&tuple,XL6_VALUE_ADMIT_ANY);
    assert(item && child && xl6ListSetV1(child,&tuple,0,&item)); xrtValueRelease(item);
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
    assert(!xl6SequenceListCreateTakeV1(1,900,LIST_ID,child,false) && drops==1);
    xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error)==XERR_VALUE); xrtErrorFree(outcome.error);
    /* The upstream factory's failed bit is independent of an error object. */
    assert(xl6ErrorScopeBegin(&scope)); xl6ErrorScopeCaptureFailure(&scope);
    assert(!xl6SequenceListCreateTakeV1(1,900,LIST_ID,NULL,false));
    outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok && !outcome.error);
}
static void fault(const char* operation,size_t point)
{
#ifdef XL6_TEST_LIST_GRAPH
    if (xl6TestListGraphFaultV1(operation,point)) return;
#endif
    if (xl6TestListSchemaFaultV1(operation,point)) return;
    if (projectionFault(operation,point)) return;
    if (!strcmp(operation,"ref-equal")) { equalityFault(point); return; }
    XL6_ListLayoutV1 tuple=layout(2,xl6StringTypeV1());
    xvalue* list=NULL,*result=NULL,*cursor=NULL,*peer=NULL;
    xstrview item={"a\0\xE4\xBD\xA0",5},changed=XRT_STR_INIT("replacement"),output={0};
    xstrview original=XRT_STR_INIT("keep");
    assert(xrtTypeCopyValue(tuple.value.type,&output,&original));
    if (strcmp(operation,"create")) {
        list=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(list && xl6ListSetV1(list,&tuple,7,&item));
    }
    bool merge=!strcmp(operation,"merge-keep") || !strcmp(operation,"merge-replace");
    if (merge) {
        peer=create(&tuple,XL6_VALUE_ADMIT_STRING); assert(peer);
        assert(xl6ListSetV1(peer,&tuple,7,&changed) && xl6ListSetV1(peer,&tuple,42,&changed));
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
    else if (merge) success=xl6ListMergeV1(list,peer,&tuple,!strcmp(operation,"merge-replace"));
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
        int64_t count=hit ? 1 : (merge || !strcmp(operation,"insert") || !strcmp(operation,"append")) ? 2 :
            (!strcmp(operation,"take") || !strcmp(operation,"clear")) ? 0 : 1;
        assert(xl6ListCountV1(list,&tuple)==count);
        if (count) {
            xstrview actual={0}; bool exists=false;
            const xstrview* expected=!hit && (!strcmp(operation,"replace") || !strcmp(operation,"merge-replace")) ? &changed : &item;
            assert(xl6ListGetV1(list,&tuple,7,&actual,false,&exists) && exists);
            assert(actual.Size==expected->Size && !memcmp(actual.Data,expected->Data,expected->Size));
            xrtTypeDropValue(tuple.value.type,&actual);
        }
    }
    if (peer) {
        xstrview actual={0}; bool exists=false;
        assert(xl6ListCountV1(peer,&tuple)==2 && xl6ListGetV1(peer,&tuple,7,&actual,false,&exists) && exists);
        assert(actual.Size==changed.Size && !memcmp(actual.Data,changed.Data,changed.Size)); xrtTypeDropValue(tuple.value.type,&actual);
    }
    xrtTypeDropValue(tuple.value.type,&output); xrtErrorFree(outcome.error);
    xrtValueRelease(result); xrtValueRelease(cursor); xrtValueRelease(peer); xrtValueRelease(list); xrtClearError();
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(argc==2 || argc==3); xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) fault(argv[1],(size_t)strtoull(argv[2],NULL,10));
#ifdef XL6_TEST_LIST_GRAPH
    else if (!strncmp(argv[1],"graph-",6)) xl6TestListGraphVectorsV1(argv[1]);
#endif
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
    else if (!strcmp(argv[1],"identity")) identity();
    else if (!strcmp(argv[1],"merges")) merges();
    else if (!strcmp(argv[1],"identity-errors")) identityErrors();
    else if (!strcmp(argv[1],"shared-depth")) sharedDepth();
    else if (!strcmp(argv[1],"guard-protocol")) guardProtocol();
    else if (!strcmp(argv[1],"projection")) projection();
    else if (!strcmp(argv[1],"projection-profiles")) projectionProfiles();
    else if (!strcmp(argv[1],"imports")) imports();
    else if (!strcmp(argv[1],"codec-errors")) codecErrors();
    else if (!strcmp(argv[1],"projection-entry-errors")) projectionEntryErrors();
    else if (!strcmp(argv[1],"schema-cleanup-errors")) schemaCleanupErrors();
    else if (!strncmp(argv[1],"schema-",7)) xl6TestListSchemaVectorsV1(argv[1]);
    else assert(0);
    assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    if (argc==2) printf("native List %s vectors balanced\n",argv[1]);
    return 0;
}
