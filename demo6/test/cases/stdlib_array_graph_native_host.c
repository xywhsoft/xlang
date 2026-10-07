#define XL6_IMPORT_FIXTURE_MAIN xl6ArrayGraphImportFixtureMain
#include "test/cases/stdlib_array_import_native_host.c"
#include "src/runtime/xl6_array_graph.h"
#include "src/api/xl_foreign_error.h"
static void graphError(xerrkind expected)
{
    if (!xrtGetError() || xrtErrorKind(xrtGetError())!=expected)
        fprintf(stderr,"graph error: expected %u, actual %u\n",(unsigned)expected,
            xrtGetError() ? (unsigned)xrtErrorKind(xrtGetError()) : 0);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==expected); xrtClearError();
}
#define error graphError
static bool liveGraph(const xmemdebugallocation* allocation,ptr user)
{
    (void)user; fprintf(stderr,"live graph allocation %p size=%zu site=%s:%u\n",allocation->Address,
        allocation->Size,allocation->File ? allocation->File : "?",(unsigned)allocation->Line); return true;
}
static void graphBalanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (after.LiveCount!=before->LiveCount || after.LiveBytes!=before->LiveBytes || xrtGetError()) {
        fprintf(stderr,"graph balance: live=%llu/%llu bytes=%llu/%llu error=%u\n",
            (unsigned long long)after.LiveCount,(unsigned long long)before->LiveCount,
            (unsigned long long)after.LiveBytes,(unsigned long long)before->LiveBytes,
            xrtGetError() ? (unsigned)xrtErrorKind(xrtGetError()) : 0);
        xrtMemDebugVisitLive(liveGraph,NULL);
    }
    balanced(before);
}
#define balanced graphBalanced

static xvalue* graphSource(void)
{
    xvalue* source=wire(false);
#if XL6_FACTORY_PROFILE == 4
    xvalue* child=xrtValueArrayAt(source,0);
    xvalue* alias=xl6ArrayEraseV1(child,3001,4001,1,sizeof(int64_t),xrtTypeInt64(),NULL);
    assert(alias==child); xrtValueRelease(alias);
    xvalue* first=xrtValueInt(1),*second=xrtValueInt(2); assert(first && second);
    assert(xl6ArrayErasedPushV1(child,first) && xl6ArrayErasedPushV1(child,second));
    xrtValueRelease(first); xrtValueRelease(second);
#endif
    xvalue* result=xl6ArrayImportTakeV1(seed(true),source);
    xrtValueRelease(source); assert(result); return result;
}
static xvalue* mixed(xvalue* native, bool wide)
{
    xvalue* result=xrtValueObject(); assert(result);
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("native"),native));
    assert(xrtValueObjectSet(result,XRT_STR_LITERAL("again"),native));
    if (wide) {
        xvalue* values=xrtValueArray(); assert(values);
        for (size_t i=0;i<70;++i) {
            xvalue* child=graphSource();
            assert(xrtValueArrayAppend(values,child) && xrtValueArrayAppend(values,child));
            xrtValueRelease(child);
        }
        assert(xrtValueObjectSetTake(result,XRT_STR_LITERAL("wide"),&values) && !values);
    }
    return result;
}
static void graphCheck(const xvalue* source, const xvalue* result, bool data, bool isMixed, bool wide)
{
    assert(source && result && source!=result);
    const xvalue* before=source; const xvalue* after=result;
    if (isMixed) {
        before=xrtValueObjectGet(source,XRT_STR_LITERAL("native"));
        after=xrtValueObjectGet(result,XRT_STR_LITERAL("native"));
        assert(after==xrtValueObjectGet(result,XRT_STR_LITERAL("again")));
    }
    assert(after && before!=after); observed(before);
    if (data) {
        assert(xrtValueType(after)==XVALUE_ARRAY && xrtValueCount(after)==2);
#if XL6_FACTORY_PROFILE == 4
        const xvalue* child=xrtValueArrayAt(after,0);
        assert(child==xrtValueArrayAt(after,1) && xrtValueType(child)==XVALUE_ARRAY && xrtValueCount(child)==2);
        int64_t number; assert(xrtValueGetInt(xrtValueArrayAt(child,0),&number) && number==1);
#endif
    } else {
        assert(xl6SequenceAdmitsArrayV1(after,1001)); observed(after);
#if XL6_FACTORY_PROFILE == 4
        xvalue* a=xl6ArrayErasedReadV1(before,0),*b=xl6ArrayErasedReadV1(after,0);
        xvalue* alias=xl6ArrayErasedReadV1(after,1); assert(a && b && a!=b && b==alias);
        xvalue* replacement=xrtValueInt(7); assert(replacement && xl6ArrayErasedSetV1(b,0,replacement));
        xvalue* stillOriginal=xl6ArrayErasedReadV1(a,0); int64_t number;
        assert(stillOriginal && xrtValueGetInt(stillOriginal,&number) && number==1);
        xrtValueRelease(stillOriginal); xrtValueRelease(replacement);
        xrtValueRelease(alias); xrtValueRelease(b); xrtValueRelease(a);
#endif
    }
    if (wide) {
        const xvalue* values=xrtValueObjectGet(result,XRT_STR_LITERAL("wide"));
        assert(xrtValueCount(values)==140);
        for (size_t i=0;i<140;i+=2) assert(xrtValueArrayAt(values,i)==xrtValueArrayAt(values,i+1));
    }
}
static void dropOpaque(ptr raw,ptr user) { (void)raw; (void)user; }
static const xvaluehandleops opaqueOps={NULL,dropOpaque,NULL,NULL};
static void vectorsGraph(void)
{
    xvalue* source=graphSource();
    xrtSetErrorKind(XERR_IO); const xerror* incoming=xrtGetError(); assert(incoming);
    xvalue* deep=xl6ValueDeepCloneV1(source); assert(deep && xrtGetError()==incoming); xrtClearError();
    graphCheck(source,deep,false,false,false); xrtValueRelease(deep);
    xvalue* data=xl6ValueDataSnapshotV1(source); assert(data);
    graphCheck(source,data,true,false,false);
#if XL6_FACTORY_PROFILE != 7 && XL6_FACTORY_PROFILE != 8
    size_t length=0; char* text=xrtJsonStringify(data,false,&length); assert(text && length);
    xrtFree(text); text=xrtXlonStringify(data,false,&length); assert(text && length); xrtFree(text);
#endif
    xrtValueRelease(data);
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(source,&pin,&layout,&converter)==1);
    assert(!xl6ValueDeepCloneV1(source)); error(XERR_STATE);
    assert(!xl6ValueDataSnapshotV1(source)); error(XERR_STATE); xl6SequenceEndV1(pin);
    xvalue* root=mixed(source,true);
    deep=xl6ValueDeepCloneV1(root); assert(deep); graphCheck(root,deep,false,true,true); xrtValueRelease(deep);
    data=xl6ValueDataSnapshotV1(root); assert(data); graphCheck(root,data,true,true,true); xrtValueRelease(data);
    xrtValueRelease(root); xrtValueRelease(source);
    /* Foreign opaque pointer is never dereferenced by our sealed adapter. */
    ptr raw=(ptr)(uintptr_t)1;
    xvalue* opaque=xrtValueHandleTake(&raw,&opaqueOps,NULL); assert(opaque && !raw);
    data=xl6ValueDataSnapshotV1(opaque); assert(data==opaque); xrtValueRelease(data);
    assert(!xl6ValueDeepCloneV1(opaque)); error(XERR_UNSUPPORTED); xrtValueRelease(opaque);
}
#if XL6_FACTORY_PROFILE == 0
static void cyclesAndDepth(void)
{
    xvalue* self=seed(true); assert(self && xl6ArrayErasedPushV1(self,self));
    assert(!xl6ValueDeepCloneV1(self)); error(XERR_VALUE);
    assert(!xl6ValueDataSnapshotV1(self)); error(XERR_VALUE);
    assert(xl6ArrayErasedClearV1(self)); xrtValueRelease(self);
    xvalue* nodes[300];
    for (size_t i=0;i<300;++i) {
        nodes[i]=seed(true); assert(nodes[i]);
        if (i) assert(xl6ArrayErasedPushV1(nodes[i-1],nodes[i]));
    }
    assert(!xl6ValueDeepCloneV1(nodes[0])); error(XERR_VALUE);
    assert(!xl6ValueDataSnapshotV1(nodes[0])); error(XERR_VALUE);
    for (size_t i=0;i<300;++i) { assert(xl6ArrayErasedClearV1(nodes[i])); xrtValueRelease(nodes[i]); }
}
#endif
static int faultsGraph(const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    bool data=strstr(operation,"data")!=NULL, wide=strstr(operation,"wide")!=NULL;
    bool isMixed=wide || strstr(operation,"mixed")!=NULL;
    assert(!strcmp(operation,"deep") || !strcmp(operation,"data") || !strcmp(operation,"mixed") ||
        !strcmp(operation,"mixed-data") || !strcmp(operation,"wide") || !strcmp(operation,"wide-data"));
    xvalue* native=graphSource(),*source=isMixed ? mixed(native,wide) : xrtValueRetain(native); assert(source);
    assert(xrtMemDebugFailAfter(point));
    xvalue* result=data ? xl6ValueDataSnapshotV1(source) : xl6ValueDeepCloneV1(source);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else { assert(result); graphCheck(source,result,data,isMixed,wide); }
    observed(native); xrtValueRelease(result); xrtValueRelease(source); xrtValueRelease(native); balanced(&before);
    printf("array-graph profile=%u operation=%s point=%zu hit=%u balanced\n",XL6_FACTORY_PROFILE,operation,point,hit);
    return 0;
}
#ifndef XL6_GRAPH_FIXTURE_MAIN
#define XL6_GRAPH_FIXTURE_MAIN main
#endif
int XL6_GRAPH_FIXTURE_MAIN(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) return faultsGraph(argv[1],(size_t)strtoull(argv[2],NULL,10));
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (size_t i=0;i<25;++i) { vectorsGraph(); balanced(&before); }
#if XL6_FACTORY_PROFILE == 0
    cyclesAndDepth();
#endif
    balanced(&before); printf("array-graph profile=%u: 25 mixed graph cycles balanced\n",XL6_FACTORY_PROFILE);
    return 0;
}
