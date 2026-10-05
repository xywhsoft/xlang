#include "src/runtime/xl6_array_input.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_process_config.h"
#include "src/runtime/xl6_pattern_internal.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VIEW(s) (xstrview){s,sizeof(s)-1}
#define STR_ID UINT64_C(1002)
#define INT_ID UINT64_C(1004)
#define ANY_ID UINT64_C(1006)

static xvalue* bound(xvalue* value,uint64_t id,uint64_t element,unsigned transfer,
    size_t size,const xrttype* type,const xvalueconverter* converter)
{
    assert(value);
    xvalue* alias=xl6ArrayEraseV1(value,id,element,transfer,size,type,converter);
    assert(alias==value); xrtValueRelease(alias); return value;
}
static xvalue* strings(const xstrview* texts,size_t count)
{
    xvalue* a=bound(xl6ArrayPrimitiveCreateV2(1001,STR_ID,2,sizeof(xstrview),
        xl6StringTypeV1(),XL6_VALUE_ADMIT_STRING,false,STR_ID),1001,STR_ID,2,
        sizeof(xstrview),xl6StringTypeV1(),xl6StringConverterV1());
    for (size_t i=0;i<count;++i) {
        xvalue* text=xrtValueString(texts[i]); assert(text);
        assert(xl6ArrayErasedPushV1(a,text)); xrtValueRelease(text);
    }
    return a;
}
static xvalue* integers(int64_t n,size_t count)
{
    xvalue* a=bound(xl6ArrayPrimitiveCreateV2(1003,INT_ID,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,INT_ID),1003,INT_ID,1,
        sizeof(int64_t),xrtTypeInt64(),NULL);
    for (size_t i=0;i<count;++i) {
        xvalue* v=xrtValueInt(n); assert(v);
        assert(xl6ArrayErasedPushV1(a,v)); xrtValueRelease(v);
    }
    return a;
}
static xvalue* references(const xvalue* item,size_t count)
{
    xvalue* a=bound(xl6ArrayReferenceCreateV2(1005,ANY_ID,XL6_VALUE_ADMIT_ANY,
        false,ANY_ID),1005,ANY_ID,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    for (size_t i=0;i<count;++i) assert(xl6ArrayErasedPushV1(a,item));
    return a;
}
static void expectText(xstrview actual,xstrview expected)
{
    assert(actual.Data && actual.Size==expected.Size &&
        !memcmp(actual.Data,expected.Data,actual.Size));
    xrtFree((void*)actual.Data);
}
static xvalue* stateCreate(void* builder)
{
    xvalue* state=xrtValueObject(); assert(state);
    xvalue* identity=xl6PatternBuilderIdentityV1(builder); assert(identity);
    xvalue* tags=xrtValueIntMap(); assert(tags);
    assert(xrtValueObjectSetTake(state,VIEW("identity"),&identity));
    assert(xrtValueObjectSetTake(state,VIEW("tags"),&tags)); return state;
}
static void expectError(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void foreignDrop(void* handle,void* context)
{ (void)handle; if (context) xrtSetErrorKind((xerrkind)(uintptr_t)context); }
static const xvaluehandleops foreignOps={NULL,foreignDrop,NULL,NULL};
static xvalue* foreign(xerrkind cleanup)
{
    void* raw=(void*)(uintptr_t)1;
    xvalue* value=xrtValueHandleTake(&raw,&foreignOps,(void*)(uintptr_t)cleanup);
    assert(value && !raw); return value;
}
static void functional(void)
{
    const xstrview texts[]={VIEW("a\0b"),VIEW("你"),VIEW("")};
    xvalue* a=strings(texts,3); XL6_ArrayInputV1 input={0};
    assert(xl6ArrayInputAcquireV1(a,&input) && input.count==3);
    assert(xl6ArrayErasedClearV1(a)); xrtValueRelease(a);
    for (size_t i=0;i<3;++i) {
        xstrview text; assert(xrtValueGetString(input.items[i],&text));
        assert(text.Size==texts[i].Size && !memcmp(text.Data,texts[i].Data,text.Size));
        assert(xrtValueTypeId(input.items[i])==STR_ID);
    }
    xl6ArrayInputReleaseV1(&input); assert(!input.items && !input.count);
    xl6ArrayInputReleaseV1(&input);
    xvalue* item=xrtValueObject(); assert(item);
    a=references(item,3);
    assert(xl6ArrayInputAcquireV1(a,&input));
    xrtValueRelease(item); xrtValueRelease(a);
    assert(input.items[0]==input.items[1] && input.items[1]==input.items[2]);
    xl6ArrayInputReleaseV1(&input);
    a=strings(texts,2);
    expectText(xl6TextJoinV1(a,VIEW("|"),-1,-1),VIEW("a\0b|你"));
    xrtValueRelease(a);
    const xstrview ascii[]={VIEW("abcd"),VIEW("bc")}; a=strings(ascii,2);
    expectText(xl6TextJoinV1(a,VIEW("|"),-1,1),VIEW("a|b")); xrtValueRelease(a);
}
static void boundaries(void)
{
    XL6_ArrayInputV1 output={0};
    assert(!xl6ArrayInputAcquireV1(xrtValueBool(true),&output)); expectError(XERR_TYPE);
    xvalue* map=xrtValueIntMap(); assert(map);
    assert(!xl6ArrayInputAcquireV1(map,&output)); expectError(XERR_TYPE); xrtValueRelease(map);
    xvalue* other=foreign(0);
    assert(xrtValueTypeIdBind(other,1003));
    assert(!xl6ArrayInputAcquireV1(other,&output)); expectError(XERR_TYPE); xrtValueRelease(other);
    xvalue* unbound=xl6ArrayPrimitiveCreateV2(1003,INT_ID,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,INT_ID); assert(unbound);
    assert(!xl6ArrayInputAcquireV1(unbound,&output)); expectError(XERR_STATE);
    xrtValueRelease(unbound);
    xvalue* stack=xl6StackCreateV1(1011,INT_ID,1,sizeof(int64_t),xrtTypeInt64()); assert(stack);
    assert(!xl6ArrayInputAcquireV1(stack,&output)); expectError(XERR_TYPE); xrtValueRelease(stack);
    xvalue* a=integers(42,2);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(a,XL6_SEQUENCE_ARRAY_V1,1003,INT_ID,1,
        sizeof(int64_t),xrtTypeInt64()); assert(pin);
    assert(!xl6ArrayInputAcquireV1(a,&output)); expectError(XERR_STATE); xl6SequenceEndV1(pin);
    XL6_ArrayInputV1 canary={(xvalue**)(uintptr_t)1,1};
    assert(!xl6ArrayInputAcquireV1(a,&canary) && canary.items==(xvalue**)(uintptr_t)1 && canary.count==1);
    expectError(XERR_ARGUMENT);
    xrtSetErrorKind(XERR_PROTOCOL); const xerror* prior=xrtGetError();
    assert(xl6ArrayInputAcquireV1(a,&output) && xrtGetError()==prior);
    xl6ArrayInputReleaseV1(&output); assert(xrtGetError()==prior); xrtClearError();
    xrtValueRelease(a);
    xvalue* wire=xrtValueArray(); assert(wire);
    assert(xl6ArrayInputAcquireV1(wire,&output) && output.count==0);
    xl6ArrayInputReleaseV1(&output); xrtValueRelease(wire);
    wire=xrtValueArray(); assert(wire);
    assert(xrtValueArrayAppendNew(wire,foreign(XERR_PROTOCOL)));
    assert(xrtValueArrayAppendNew(wire,foreign(XERR_STATE)));
    assert(xl6ArrayInputAcquireV1(wire,&output)); xrtValueRelease(wire);
    xl6ArrayInputReleaseV1(&output); expectError(XERR_PROTOCOL);
    const xstrview bad[]={VIEW("a\0b")}; a=strings(bad,1);
    assert(!xl6ProcessCommandDirectV1(VIEW("tool"),a,-1,-1)); expectError(XERR_VALUE);
    xrtValueRelease(a);
    const xstrview utf8[]={VIEW("你")}; a=strings(utf8,1);
    assert(!xl6TextJoinV1(a,VIEW("|"),-1,1).Data); expectError(XERR_VALUE);
    xrtValueRelease(a); assert(!output.items && !output.count);
}
static void consumers(void)
{
    const xstrview args[]={VIEW("你"),VIEW("")}; xvalue* a=strings(args,2);
    XL6_ProcessCommand* command=xl6ProcessCommandDirectV1(VIEW("tool"),a,-1,-1); assert(command);
    assert(command->config.ArgCount==2 && !strcmp(command->config.Args[0],"你") && !strcmp(command->config.Args[1],""));
    xrtValueRelease(a); assert(!strcmp(command->config.Args[0],"你")); xl6ProcessCommandDestroyV1(command);
    const xstrview patterns[]={VIEW("a\0b"),VIEW("你")}; a=strings(patterns,2);
    void* set=xl6RegexSetCreateV1(a,0,4096,16,8,-1); assert(set); xrtValueRelease(a);
    expectText(xl6RegexSetPatternV1(set,0),VIEW("a\0b"));
    xvalue* hits=xl6RegexSetHitsV1(set,VIEW("你a\0b"),0,-1); assert(hits && xrtValueCount(hits)==2);
    xrtValueRelease(hits); xl6RegexSetDestroyV1(set);
    void* b=xl6PatternBuilderCreateV1(); assert(b); xvalue* state=stateCreate(b);
    const xstrview paths[]={VIEW("/a/{id}"),VIEW("/b/{id}")}; a=strings(paths,2);
    xvalue* numbers=integers(-1,2); /* Same Array is valid for priority and size. */
    void* edit=xl6PatternEditAddManyV1(b,a,numbers,numbers); assert(edit);
    xrtValueRelease(a); xrtValueRelease(numbers);
    void* key=xl6PatternKeyCreateV1(edit,0); assert(key);
    xvalue* tag=xrtValueObject(); assert(tag); a=references(tag,2);
    xl6PatternEditApplyManyV1(edit,state,a); assert(!xrtGetError() && xl6PatternBuilderCountV1(b)==2);
    xvalue* tags=xl6PatternBuilderTagsV1(b,state); assert(tags);
    int64_t slot=xl6PatternTagKeyFromId(xl6PatternKeyIdV1(key));
    assert(xrtValueIntMapGet(tags,slot)==tag); xrtValueRelease(tags); xrtValueRelease(tag);
    xrtValueRelease(a); xl6PatternKeyDestroyV1(key); xl6PatternEditDestroyV1(edit);
    xl6PatternBuilderDestroyV1(b); xrtValueRelease(state);
}
static bool injected(size_t point)
{ if (point==SIZE_MAX) return false; assert(xrtMemDebugFailAfter(point)); return true; }
static void sweep(unsigned mode,size_t point)
{
    const xstrview texts[]={VIEW("a\0b"),VIEW("你"),VIEW("")};
    xvalue* a=strings(texts,3); void* result=NULL; xstrview text={0};
    XL6_ArrayInputV1 input={0}; void* builder=NULL; void* edit=NULL;
    xvalue* numbers=NULL; xvalue* state=NULL; bool ok=false;
    xfuture* futures[2]={0}; xpromise* promises[2]={0};
    if (mode==0) { xrtValueRelease(a); a=xrtValueArray(); assert(a);
        xvalue* v=xrtValueString(texts[0]); assert(v && xrtValueArrayAppendNew(a,v)); }
    if (mode==3) { const xstrview argv[]={VIEW("first"),VIEW("")}; xrtValueRelease(a); a=strings(argv,2); }
    if (mode==5 || mode==6) {
        const xstrview paths[]={VIEW("/a/{id}"),VIEW("/b/{id}")}; xrtValueRelease(a); a=strings(paths,2);
        builder=xl6PatternBuilderCreateV1(); assert(builder); state=stateCreate(builder); numbers=integers(-1,2);
        if (mode==6) { edit=xl6PatternEditAddManyV1(builder,a,numbers,numbers); assert(edit);
            xrtValueRelease(a); xvalue* tag=xrtValueObject(); assert(tag); a=references(tag,2); xrtValueRelease(tag); }
    }
    if (mode>=7) {
        xrtValueRelease(a); promises[0]=xrtPromiseCreate(&futures[0],NULL);
        promises[1]=xrtPromiseCreate(&futures[1],NULL); assert(promises[0] && promises[1]);
        xvalue* first=xrtValueFuture(futures[0]); assert(first); a=references(first,2); xrtValueRelease(first);
        xvalue* last=xrtValueFuture(futures[1]); assert(last); assert(xl6ArrayErasedPushV1(a,last)); xrtValueRelease(last);
        if (mode>=10) { assert(xrtPromiseResolve(promises[0],NULL) && xrtPromiseResolve(promises[1],NULL)); }
    }
    injected(point);
    if (mode<=1) ok=xl6ArrayInputAcquireV1(a,&input);
    if (mode==2) { text=xl6TextJoinV1(a,VIEW("|"),-1,-1); ok=text.Data!=NULL; }
    if (mode==3) { result=xl6ProcessCommandDirectV1(VIEW("tool"),a,-1,-1); ok=result!=NULL; }
    if (mode==4) { result=xl6RegexSetCreateV1(a,0,4096,16,8,-1); ok=result!=NULL; }
    if (mode==5) { result=xl6PatternEditAddManyV1(builder,a,numbers,numbers); ok=result!=NULL; }
    if (mode==6) { xl6PatternEditApplyManyV1(edit,state,a); ok=xrtGetError()==NULL; }
    if (mode>=7) {
        unsigned which=(mode-7)%3;
        result=which==0 ? xl6FutureAggregateAllArrayV2(a,NULL,NULL) : which==1 ? xl6FutureAggregateRaceArrayV2(a) : xl6FutureAggregateWhenAnyIndexArrayV2(a);
        ok=result!=NULL;
    }
    bool hit=xl6ProductMemoryEnd();
    if (!ok) { assert(hit && xrtGetError()); if (mode<=1) assert(!input.count && !input.items); }
    else assert(!xrtGetError());
    if (!hit) assert(ok);
    xrtClearError();
    if (mode<=1) xl6ArrayInputReleaseV1(&input);
    if (mode==2) xrtFree((void*)text.Data);
    if (mode==3) xl6ProcessCommandDestroyV1(result);
    if (mode==4) xl6RegexSetDestroyV1(result);
    if (mode==5) xl6PatternEditDestroyV1(result);
    if (mode==6) {
        size_t expected=ok?2:0; assert((size_t)xl6PatternBuilderCountV1(builder)==expected);
        xvalue* tags=xl6PatternBuilderTagsV1(builder,state); assert(tags && xrtValueCount(tags)==expected); xrtValueRelease(tags);
    }
    if (mode>=7) {
        if (result) { if (!xrtFutureDone(result)) assert(xrtFutureCancel(result)); xrtFutureDestroy(result); }
        for (size_t i=0;i<2;++i) { xrtPromiseDestroy(promises[i]); xrtFutureDestroy(futures[i]); }
    }
    if (edit) xl6PatternEditDestroyV1(edit);
    if (builder) xl6PatternBuilderDestroyV1(builder);
    xrtValueRelease(numbers); xrtValueRelease(state); xrtValueRelease(a);
    xrtClearError(); xl6ProductMemoryVerify();
    printf("array input mode=%u point=%zu hit=%u balanced\n",mode,point,(unsigned)hit);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if (argc==3) { unsigned mode=(unsigned)strtoul(argv[1],NULL,10); assert(mode<13);
        sweep(mode,(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); functional(); boundaries(); consumers();
    assert(!xrtGetError()); (void)xl6ProductMemoryEnd(); xl6ProductMemoryVerify();
    puts("array input: owned native/wire reads, 5 consumer families, exact text, alias/atomic guards balanced"); return 0;
}
