#include "src/runtime/xl6_list_graph.h"
#include "src/runtime/xl6_list_graph_access.h"
#include "src/runtime/xl6_list_schema.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_graph.h"
#include "src/runtime/xl6_persistence_project.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define ROOT_ID UINT64_C(0xEA00)
#define ARRAY_ID UINT64_C(0xEA01)
#define ELEMENT_ID UINT64_C(0xEA02)
#define CHILD_ID UINT64_C(0xEA10)
#define CLASS_ID UINT64_C(0xEA20)
#define WIRE_ID UINT64_C(0xEA30)
#define TUPLE(t) (t).identity,(t).elementIdentity,(t).transfer,(t).size,(t).type
extern void xl6TestListGraphBoundaryFailureV1(int,int);
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static XL6_ListLayoutV1 layout(unsigned profile)
{
    const xrttype* type=profile==0 ? xrtTypeInt64() : profile==1 ? xl6StringTypeV1() :
        profile==2 ? xrtTypeValue() : profile==3 ? xl6CharTypeV1() : profile==4 ? xl6NullableStringTypeV1() :
        profile==5 ? xrtTypeFuture() : xl6NullableI64TypeV1();
    return (XL6_ListLayoutV1){ROOT_ID,{ARRAY_ID,ELEMENT_ID,
        profile==2 ? 3 : profile==5 ? 4 : profile==1 || profile==4 ? 2 : 1,type->Size,type}};
}
static xvalue* create(unsigned profile,bool projected)
{
    XL6_ListLayoutV1 t=layout(profile); xvalue* seed;
    if (profile==2) seed=xl6SequenceReferenceCreateV1(1,ARRAY_ID,ELEMENT_ID,XL6_VALUE_ADMIT_ANY,false,ELEMENT_ID);
    else if (profile==5) seed=xl6SequenceFutureCreateV1(1,ARRAY_ID,ELEMENT_ID,ELEMENT_ID,false);
    else seed=xl6SequencePrimitiveCreateV1(1,TUPLE(t.value),profile==0 || profile==6 ? XL6_VALUE_ADMIT_I8 :
        profile==3 ? XL6_VALUE_ADMIT_CHAR : XL6_VALUE_ADMIT_STRING,profile==4 || profile==6,
        profile==4 || profile==6 ? ELEMENT_ID+1 : ELEMENT_ID);
    xvalue* result=xl6ListCreateTakeV1(&t,seed);
    const xvalueconverter* codec=profile==1 ? xl6StringConverterV1() : profile==4 ? xl6NullableStringConverterV1() :
        profile==6 ? xl6NullableI64ConverterV1() : profile==3 ? xl6CharConverterV1() : NULL;
    return projected ? xl6ListProjectionTakeV1(result,&t,codec) : result;
}
static void fill(xvalue* value,unsigned profile)
{
    XL6_ListLayoutV1 t=layout(profile); int64_t n=7; xstrview text={"a\0\xE4\xBD\xA0",5};
    uint32_t ch=0x4F60; XL6_NullableI64V1 optional={true,7}; xvalue* bytes=NULL;
    xfuture* future=NULL; xpromise* promise=NULL;
    if (profile==2) { bytes=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3}); assert(bytes); }
    if (profile==5) { promise=xrtPromiseCreate(&future,NULL); assert(promise && future); }
    const void* slot=profile==0 ? (const void*)&n : profile==2 ? (const void*)&bytes :
        profile==3 ? (const void*)&ch : profile==5 ? (const void*)&future : profile==6 ? (const void*)&optional : (const void*)&text;
    assert(xl6ListSetV1(value,&t,INT64_MIN,slot) && xl6ListSetV1(value,&t,INT64_MAX,slot));
    if (profile==4) { text=(xstrview){0}; assert(xl6ListSetV1(value,&t,0,&text)); }
    if (profile==6) { optional=(XL6_NullableI64V1){0}; assert(xl6ListSetV1(value,&t,0,&optional)); }
    xrtValueRelease(bytes); xrtFutureDestroy(future); xrtPromiseDestroy(promise);
}
static XL6_ValueGraphPlanV1 plan(const XL6_ValueGraphProviderV1* const* providers,
    const XL6_ValueGraphSequenceV1* sequences,size_t count)
{ return (XL6_ValueGraphPlanV1){sizeof(XL6_ValueGraphPlanV1),providers,1,sequences,count}; }
static void observed(unsigned profile,const xvalue* value,bool native,bool sequence)
{
    xvalue* item=NULL; bool found=false;
    if (native) assert(xl6ListErasedReadV1(value,INT64_MIN,false,&item,&found) && found);
    else item=xrtValueRetain(sequence ? xrtValueArrayAt(value,0) : xrtValueIntMapGet(value,INT64_MIN));
    assert(item);
    if (profile==0 || profile==6) { int64_t n=0; assert(xrtValueGetInt(item,&n) && n==7); }
    if (profile==1 || profile==4) { xstrview t={0}; assert(xrtValueGetString(item,&t) && t.Size==5 && !memcmp(t.Data,"a\0\xE4\xBD\xA0",5)); }
    if (profile==2) { xbytesview b={0}; assert(xrtValueGetBytes(item,&b) && b.Size==3 && !memcmp(b.Data,"a\0b",3)); }
    if (profile==3) { uint32 n=0; assert(xrtValueGetChar(item,&n) && n==0x4F60); }
    if (profile==5) assert(xrtValueIsFuture(item));
    assert(profile==2 || xrtValueTypeId(item)==(profile==4 || profile==6 ? ELEMENT_ID+1 : ELEMENT_ID));
    xrtValueRelease(item);
}
static void profiles(void)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()};
    const XL6_ValueGraphSequenceV1 rule={ROOT_ID,providers[0]};
    XL6_ValueGraphPlanV1 native=plan(providers,NULL,0),sequence=plan(providers,&rule,1);
    for (unsigned profile=0;profile<7;++profile) {
        xvalue* source=create(profile,true); assert(source); fill(source,profile);
        XL6_ListLayoutV1 t=layout(profile),captured={0}; XL6_SequenceAdmissionV3 policy={0};
        assert(xl6ListSchemaCopyV1(source,&captured,&policy));
        xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError();
        xvalue* deep=xl6ValueDeepCloneNativeV1(source,&sequence); assert(deep && deep!=source && xrtGetError()==pending); xrtClearError();
        assert(xl6ListAdmitsSchemaV1(deep,&captured,&policy)); observed(profile,deep,true,false);
        bool same=false; assert(xl6ListEqualsV1(source,deep,&t,&same) && same);
        xvalue* data=xl6ValueDataSnapshotNativeV1(source,&native); assert(data && xrtValueType(data)==XVALUE_INT_MAP);
        observed(profile,data,false,false);
        assert(xrtValueIntMapGet(data,INT64_MAX)); xrtValueRelease(data);
        data=xl6ValueDataSnapshotNativeV1(source,&sequence); assert(data && xrtValueType(data)==XVALUE_ARRAY);
        observed(profile,data,false,true); assert(xrtValueCount(data)==(profile==4 || profile==6 ? 3 : 2)); xrtValueRelease(data);
        assert(xl6ListClearV1(deep,&t) && xl6ListCountV1(source,&t)==(profile==4 || profile==6 ? 3 : 2));
        observed(profile,source,true,false); xl6SequenceAdmissionDropV3(&policy); xrtValueRelease(deep); xrtValueRelease(source);
    }
}
static void foreignDrop(ptr raw,ptr context) { (void)raw; (void)context; }
static const xvaluehandleops foreignOps={.Drop=foreignDrop};
static xvalue* mixed(bool wide)
{
    xvalue* child=create(1,true); assert(child); fill(child,1);
    xvalue* array=xl6SequenceListCreateTakeV1(1,CHILD_ID,ROOT_ID,xrtValueRetain(child),false); assert(array);
    array=xl6ArrayProjectionTakeV1(array,CHILD_ID,ROOT_ID,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(array);
    assert(xl6ArrayErasedPushV1(array,child) && xl6ArrayErasedPushV1(array,child));
    XL6_ListLayoutV1 outer={ROOT_ID+100,{ARRAY_ID+100,CHILD_ID,3,sizeof(xvalue*),xrtTypeValue()}};
    xvalue* seed=xl6SequenceNestedCreateTakeV2(1,outer.value.identity,CHILD_ID,xrtValueRetain(array),false);
    xvalue* list=xl6ListProjectionTakeV1(xl6ListCreateTakeV1(&outer,seed),&outer,NULL); assert(list);
    assert(xl6ListErasedSetV1(list,INT64_MIN,array) && xl6ListErasedSetV1(list,INT64_MAX,array));
    xvalue* wire=xrtValueIntMap(),*root=xrtValueObject(); assert(wire && root && xrtValueTypeIdBind(root,CLASS_ID));
    assert(xrtValueTypeIdBind(wire,WIRE_ID) && xrtValueIntMapSet(wire,-9,list));
    assert(xrtValueObjectSet(root,XRT_STR_LITERAL("one"),list) && xrtValueObjectSet(root,XRT_STR_LITERAL("array"),array) &&
        xrtValueObjectSet(root,XRT_STR_LITERAL("leaf"),child) && xrtValueObjectSetTake(root,XRT_STR_LITERAL("wire"),&wire));
    ptr raw=(ptr)(uintptr_t)1; xvalue* opaque=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(opaque);
    assert(xrtValueObjectSetTake(root,XRT_STR_LITERAL("hidden"),&opaque));
    if (wide) {
        xvalue* many=xrtValueArray(); assert(many);
        for (unsigned i=0;i<70;++i) { xvalue* next=create(1,true); assert(next); fill(next,1);
            assert(xrtValueArrayAppend(many,next) && xrtValueArrayAppend(many,next)); xrtValueRelease(next); }
        assert(xrtValueObjectSetTake(root,XRT_STR_LITERAL("many"),&many));
    }
    xrtValueRelease(list); xrtValueRelease(array); xrtValueRelease(child); return root;
}
static XL6_PersistencePlanV1 objectPlan(void)
{
    static const XL6_PersistenceFieldV1 fields[]={{XRT_STR_LITERAL("one"),XRT_STR_LITERAL("one"),0},
        {XRT_STR_LITERAL("array"),XRT_STR_LITERAL("array"),0},{XRT_STR_LITERAL("leaf"),{"renamed\0leaf",12},0},
        {XRT_STR_LITERAL("wire"),XRT_STR_LITERAL("wire"),0},{XRT_STR_LITERAL("many"),XRT_STR_LITERAL("many"),XL6_PERSIST_OMIT_NULL_V1}};
    static const XL6_PersistenceSchemaV1 schema={CLASS_ID,0,5,fields,NULL};
    static const xvaluegraphsequencev1 wireRule={WIRE_ID,XVALUE_INT_MAP};
    return (XL6_PersistencePlanV1){sizeof(XL6_PersistencePlanV1),1,&schema,NULL,&wireRule,1};
}
static void mixedCheck(const xvalue* source,const xvalue* result,bool deep,bool wide)
{
    assert(result && result!=source);
    const xvalue* after=xrtValueObjectGet(result,XRT_STR_LITERAL("one"));
    assert(after && after!=xrtValueObjectGet(source,XRT_STR_LITERAL("one")));
    xvalue* child=NULL; bool found=false;
    if (deep) { assert(xl6ListErasedReadV1(after,INT64_MIN,false,&child,&found) && found); }
    else { assert(xrtValueType(after)==XVALUE_ARRAY); child=xrtValueRetain(xrtValueArrayAt(after,0));
        assert(child==xrtValueArrayAt(after,1)); }
    assert(child==xrtValueObjectGet(result,XRT_STR_LITERAL("array")));
    xvalue* leaf=deep ? xl6ArrayErasedReadV1(child,0) : xrtValueRetain(xrtValueArrayAt(child,0)); assert(leaf);
    xvalue* alias=deep ? xl6ArrayErasedReadV1(child,1) : xrtValueRetain(xrtValueArrayAt(child,1)); assert(leaf==alias);
    assert(leaf==xrtValueObjectGet(result,deep ? (xstrview)XRT_STR_LITERAL("leaf") : (xstrview){"renamed\0leaf",12}));
    observed(1,leaf,deep,!deep); xrtValueRelease(alias); xrtValueRelease(leaf); xrtValueRelease(child);
    const xvalue* wire=xrtValueObjectGet(result,XRT_STR_LITERAL("wire"));
    assert(deep ? xrtValueIntMapGet(wire,-9)==after : xrtValueArrayAt(wire,0)==after);
    if (wide) {
        const xvalue* many=xrtValueObjectGet(result,XRT_STR_LITERAL("many")); assert(xrtValueCount(many)==140);
        for (unsigned i=0;i<140;i+=2) assert(xrtValueArrayAt(many,i)==xrtValueArrayAt(many,i+1));
    }
}
static xvalue* project(const xvalue* source,const XL6_ValueGraphProviderV1* const* providers)
{
    const XL6_ValueGraphSequenceV1 rules[]={{ROOT_ID,providers[0]},{ROOT_ID+100,providers[0]}};
    XL6_ValueGraphPlanV1 native=plan(providers,rules,2); XL6_PersistencePlanV1 objects=objectPlan();
    return xl6PersistenceProjectNativeV1(source,&objects,&native);
}
static void mixedVectors(void)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()}; XL6_ValueGraphPlanV1 native=plan(providers,NULL,0);
    xvalue* source=mixed(true),*copy=project(source,providers); assert(copy); mixedCheck(source,copy,false,true);
    assert(!xrtValueObjectGet(copy,XRT_STR_LITERAL("hidden")));
    size_t size=0; char* encoded=xrtJsonStringify(copy,false,&size); assert(encoded && size); xrtFree(encoded);
    encoded=xrtXlonStringify(copy,false,&size); assert(encoded && size); xrtFree(encoded); xrtValueRelease(copy);
    /* Deep copying the full object must visit the intentionally opaque hidden
     * field. A class/data projection excludes it BEFORE entering its memo. */
    assert(!xl6ValueDeepCloneNativeV1(source,&native)); error(XERR_UNSUPPORTED);
    assert(xrtValueObjectRemove(source,XRT_STR_LITERAL("hidden")));
    copy=xl6ValueDeepCloneNativeV1(source,&native); assert(copy); mixedCheck(source,copy,true,true);
    xrtValueRelease(copy); xrtValueRelease(source);
}
static void cycles(void)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()}; XL6_ValueGraphPlanV1 native=plan(providers,NULL,0);
    XL6_ListLayoutV1 t=layout(2); xvalue* list=create(2,true); assert(list && xl6ListErasedSetV1(list,-9,list));
    assert(!xl6ValueDeepCloneNativeV1(list,&native)); error(XERR_VALUE);
    assert(!xl6ValueDataSnapshotNativeV1(list,&native)); error(XERR_VALUE); assert(xl6ListClearV1(list,&t)); xrtValueRelease(list);
    list=create(2,true); xvalue* array=xl6SequenceReferenceCreateV1(1,CHILD_ID,ROOT_ID,XL6_VALUE_ADMIT_ANY,false,ROOT_ID); assert(list && array);
    array=xl6ArrayProjectionTakeV1(array,CHILD_ID,ROOT_ID,3,sizeof(xvalue*),xrtTypeValue(),NULL); assert(array);
    assert(xl6ListErasedSetV1(list,-9,array) && xl6ArrayErasedPushV1(array,list));
    assert(!xl6ValueDeepCloneNativeV1(array,&native)); error(XERR_VALUE);
    assert(!xl6ValueDataSnapshotNativeV1(list,&native)); error(XERR_VALUE);
    assert(xl6ArrayErasedClearV1(array) && xl6ListClearV1(list,&t)); xrtValueRelease(array); xrtValueRelease(list);
    xvalue* nodes[300];
    for (unsigned i=0;i<300;++i) { nodes[i]=create(2,true); assert(nodes[i]); if (i) assert(xl6ListErasedSetV1(nodes[i-1],0,nodes[i])); }
    assert(!xl6ValueDeepCloneNativeV1(nodes[0],&native)); error(XERR_VALUE);
    assert(!xl6ValueDataSnapshotNativeV1(nodes[0],&native)); error(XERR_VALUE);
    for (unsigned i=0;i<300;++i) { assert(xl6ListClearV1(nodes[i],&t)); xrtValueRelease(nodes[i]); }
}
static void plans(void)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1(),xl6ListGraphProviderV1()};
    XL6_ValueGraphSequenceV1 rules[]={{ROOT_ID,providers[0]},{ROOT_ID,providers[0]}};
    XL6_ValueGraphPlanV1 native=plan(providers,rules,1); xvalue* source=create(1,true); assert(source); fill(source,1);
    XL6_ValueGraphPlanV1 bad=native; bad.size=0; assert(!xl6ValueDataSnapshotNativeV1(source,&bad)); error(XERR_ARGUMENT);
    bad=native; bad.providerCount=2; assert(!xl6ValueDataSnapshotNativeV1(source,&bad)); error(XERR_ARGUMENT);
    bad=native; bad.sequenceCount=2; assert(!xl6ValueDataSnapshotNativeV1(source,&bad)); error(XERR_ARGUMENT);
    bad=native; bad.providers=NULL; assert(!xl6ValueDataSnapshotNativeV1(source,&bad)); error(XERR_ARGUMENT);
    bad=native; bad.sequenceCount=SIZE_MAX; assert(!xl6ValueDataSnapshotNativeV1(source,&bad)); error(XERR_ARGUMENT);
    xvaluegraphsequencev1 collision={ROOT_ID,XVALUE_INT_MAP};
    assert(!xl6ValueDataProjectNativeV1(source,NULL,NULL,&collision,1,&native)); error(XERR_ARGUMENT);
    ptr raw=(ptr)(uintptr_t)1; xvalue* foreign=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(foreign && xrtValueTypeIdBind(foreign,ROOT_ID));
    assert(!xl6ValueDataSnapshotNativeV1(foreign,&native)); error(XERR_TYPE); xrtValueRelease(foreign);
    xvalue* wire=xrtValueIntMap(); assert(wire && xrtValueTypeIdBind(wire,ROOT_ID));
    assert(!xl6ValueDataSnapshotNativeV1(wire,&native)); error(XERR_TYPE); xrtValueRelease(wire);
    xvalue* unknown=create(1,false); assert(unknown);
    assert(!xl6ValueDeepCloneNativeV1(unknown,&native)); error(XERR_STATE); xrtValueRelease(unknown);
    XL6_ListLayoutV1 t=layout(1); xvalue* cursor=xl6ListCursorCreateV1(source,&t,ROOT_ID+1000,false,false,0); assert(cursor);
    assert(!xl6ValueDeepCloneNativeV1(cursor,&native)); error(XERR_STATE); xrtValueRelease(cursor);
    XL6_ListGraphViewV1 pin={0}; ptr owner=NULL,user=NULL; const xvaluehandleops* ops=NULL;
    assert(xrtValueGetHandle(source,&owner,&ops,&user) && xl6ListGraphBeginV1(ops,owner,user,ROOT_ID,&pin)==1);
    assert(!xl6ValueDeepCloneNativeV1(source,&native)); error(XERR_STATE); xl6ListGraphEndV1(&pin); xrtValueRelease(source);
}
static unsigned codecCalls;
static bool codecNull;
static int codecCleanupFailure;
static xvalue* failEncode(const xrttype* type,const void* slot,ptr context)
{
    ++codecCalls; xvalue* result=xl6StringConverterV1()->FromTyped(type,slot,context);
    if (codecNull) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL}); else xrtSetErrorKind(XERR_VALUE);
    if (codecCleanupFailure) xl6TestListGraphBoundaryFailureV1(0,codecCleanupFailure);
    return result;
}
static bool decode(const xvalue* value,const xrttype* type,ptr output,ptr context)
{ return xl6StringConverterV1()->ToTyped(value,type,output,context); }
static void codecErrors(void)
{
    static const xvalueconverter codec={.ToTyped=decode,.FromTyped=failEncode};
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()}; XL6_ValueGraphPlanV1 native=plan(providers,NULL,0);
    for (unsigned absent=0;absent<2;++absent) for (int cleanup=-1;cleanup<=1;++cleanup) {
        XL6_ListLayoutV1 t=layout(1); xvalue* source=create(1,false); assert(source); fill(source,1);
        source=xl6ListProjectionTakeV1(source,&t,&codec); assert(source); codecNull=absent; codecCalls=0; codecCleanupFailure=cleanup;
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        assert(!xl6ValueDataSnapshotNativeV1(source,&native) && codecCalls==1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_VALUE); xrtErrorFree(outcome.error);
        codecCleanupFailure=0; codecCalls=0; xvalue* deep=xl6ValueDeepCloneNativeV1(source,&native); assert(deep && !codecCalls);
        assert(xl6ListCountV1(source,&t)==2 && xl6ListCountV1(deep,&t)==2); xrtValueRelease(deep); xrtValueRelease(source);
    }
}
static unsigned providerMode,providerCalls;
static int faultCopy(const xvaluehandleops* ops,ptr raw,ptr user,uint64_t identity,xvaluegraphnextv1* next,
    xvalue** output,bool data,bool sequence)
{
    ++providerCalls;
    int status=xl6ListGraphProviderV1()->Copy(ops,raw,user,identity,next,output,data,providerMode==3 ? false : sequence);
    if (providerMode==1 && status==1) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
    if (providerMode==2 && status==1) return 0;
    return status;
}
static void providerErrors(void)
{
    static const XL6_ValueGraphProviderV1 provider={faultCopy}; const XL6_ValueGraphProviderV1* providers[]={&provider};
    XL6_ValueGraphSequenceV1 rule={ROOT_ID,&provider}; XL6_ValueGraphPlanV1 native=plan(providers,NULL,0),sequence=plan(providers,&rule,1);
    xvalue* source=create(1,true); assert(source); fill(source,1);
    for (unsigned mode=1;mode<=3;++mode) {
        providerMode=mode; providerCalls=0; xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        assert(!xl6ValueDataSnapshotNativeV1(source,mode==3 ? &sequence : &native) && providerCalls==1);
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(mode==1 ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==(mode==2 ? XERR_STATE : XERR_TYPE));
        xrtErrorFree(outcome.error); observed(1,source,true,false);
    }
    providerMode=0; xrtValueRelease(source);
}
static bool entryAbsent;
static int entryObject(uint64_t identity,xvaluegraphnextv1* next,xvalue** output,ptr user)
{
    (void)identity; (void)user;
    const xvalue* child=xrtValueGraphObjectGetV1(next,XRT_STR_LITERAL("one")); assert(child);
    xl6TestListGraphBoundaryFailureV1(entryAbsent ? -1 : 1,0);
    *output=xrtValueGraphNextV1(next,child); return *output ? 1 : -1;
}
static void entryErrors(void)
{
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()}; XL6_ValueGraphPlanV1 native=plan(providers,NULL,0);
    xvalue* source=create(1,true); assert(source); fill(source,1);
    xvalue* root=xrtValueObject(); assert(root && xrtValueObjectSet(root,XRT_STR_LITERAL("one"),source));
    for (unsigned absent=0;absent<2;++absent) for (unsigned nested=0;nested<2;++nested) {
        xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope));
        entryAbsent=absent;
        if (nested) assert(!xl6ValueDataProjectNativeV1(root,entryObject,NULL,NULL,0,&native));
        else {
            xl6TestListGraphBoundaryFailureV1(absent ? -1 : 1,0);
            assert(!xl6ValueDeepCloneNativeV1(source,&native));
        }
        xl6_error_result outcome=xl6ErrorScopeEnd(&scope); assert(!outcome.ok);
        assert(absent ? !outcome.error : outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY); xrtErrorFree(outcome.error);
        observed(1,source,true,false);
    }
    xrtValueRelease(root); xrtValueRelease(source);
}
void xl6TestListGraphVectorsV1(const char* mode)
{
    if (!strcmp(mode,"graph-profiles")) profiles();
    else if (!strcmp(mode,"graph-mixed")) mixedVectors();
    else if (!strcmp(mode,"graph-cycles")) cycles();
    else if (!strcmp(mode,"graph-plans")) plans();
    else if (!strcmp(mode,"graph-codec-errors")) codecErrors();
    else if (!strcmp(mode,"graph-provider-errors")) providerErrors();
    else { assert(!strcmp(mode,"graph-entry-errors")); entryErrors(); }
}
bool xl6TestListGraphFaultV1(const char* operation,size_t point)
{
    if (strncmp(operation,"graph-",6)) return false;
    const XL6_ValueGraphProviderV1* providers[]={xl6ListGraphProviderV1()};
    XL6_ValueGraphSequenceV1 rule={ROOT_ID,providers[0]}; XL6_ValueGraphPlanV1 native=plan(providers,NULL,0),sequence=plan(providers,&rule,1);
    bool mixedSource=!strcmp(operation,"graph-project") || !strcmp(operation,"graph-mixed-deep") || !strcmp(operation,"graph-mixed-data");
    xvalue* source=mixedSource ? mixed(false) : create(1,true),*result=NULL; assert(source);
    if (!mixedSource) fill(source,1);
    else if (strcmp(operation,"graph-project")) assert(xrtValueObjectRemove(source,XRT_STR_LITERAL("hidden")));
    xl6_error_scope scope={0}; assert(xl6ErrorScopeBegin(&scope)); assert(xrtMemDebugFailAfter(point));
    bool deep=!strcmp(operation,"graph-deep") || !strcmp(operation,"graph-mixed-deep");
    if (!strcmp(operation,"graph-project") || !strcmp(operation,"graph-mixed-data")) result=project(source,providers);
    else if (deep) result=xl6ValueDeepCloneNativeV1(source,&native);
    else result=xl6ValueDataSnapshotNativeV1(source,!strcmp(operation,"graph-sequence") ? &sequence : &native);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&scope);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    if (hit) assert(outcome.error && xrtErrorKind(outcome.error)==XERR_MEMORY);
    if (result) { if (mixedSource) mixedCheck(source,result,deep,false); else observed(1,result,deep,!strcmp(operation,"graph-sequence")); }
    if (!mixedSource) observed(1,source,true,false);
    xrtValueRelease(result); xrtValueRelease(source); xrtErrorFree(outcome.error);
    printf("native List %s point=%zu hit=%d complete-prefix balanced\n",operation,point,hit); return true;
}
