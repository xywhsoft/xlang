/* Exercise the existing nine actual resident leaf factories/codecs. */
#define XL6_IMPORT_FIXTURE_MAIN xl6SingleImportFixtureMain
#include "test/cases/stdlib_array_import_native_host.c"
#undef XL6_IMPORT_FIXTURE_MAIN
#include "src/runtime/xl6_array_import_chain.h"
#include "src/api/xl_foreign_error.h"

static unsigned cleanupCalls;
static void failingDrop(void* pointer,void* context)
{
    (void)pointer; (void)context; ++cleanupCalls;
    xrtSetErrorKind(XERR_ARGUMENT); xl6ForeignCallbackFail();
}
static const xvaluehandleops failingOps={NULL,failingDrop,NULL,NULL};

static size_t plan(xvalue** seeds,bool nullable)
{
    seeds[1]=seed(true);
#if XL6_FACTORY_PROFILE == 4
    seeds[2]=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,3001,4001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,4001);
    assert(seeds[2]);
    xvalue* alias=xl6ArrayEraseV1(seeds[2],3001,4001,1,sizeof(int64_t),xrtTypeInt64(),NULL);
    assert(alias==seeds[2]); xrtValueRelease(alias);
#endif
    assert(seeds[1]);
    seeds[0]=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,9001,nullable ? 9002 : 1001,
        xrtValueRetain(seeds[1]),nullable); assert(seeds[0]);
    xvalue* projection=xl6ArrayEraseV1(seeds[0],9001,nullable ? 9002 : 1001,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    assert(projection==seeds[0]); xrtValueRelease(projection);
    return XL6_FACTORY_PROFILE==4 ? 3 : 2;
}
static xvalue* leafWire(void)
{
#if XL6_FACTORY_PROFILE == 4
    xvalue* root=xrtValueArray(),*leaf=xrtValueArray(); assert(root && leaf);
    assert(xrtValueArrayAppendNew(leaf,xrtValueInt(65)));
    assert(xrtValueArrayAppend(root,leaf) && xrtValueArrayAppend(root,leaf));
    xrtValueRelease(leaf); return root;
#else
    return wire(false);
#endif
}
static xvalue* sourceWire(size_t width,bool nullable)
{
    xvalue* root=xrtValueArray(),*child=leafWire(); assert(root && child);
    for (size_t i=0;i<width;++i) assert(xrtValueArrayAppend(root,child));
    if (nullable) assert(xrtValueArrayAppend(root,xrtValueNull()));
    xrtValueRelease(child); return root;
}
static xvalue* sourceWide(size_t width)
{
    xvalue* root=xrtValueArray(); assert(root);
    for (size_t i=0;i<width;++i) {
        xvalue* child=leafWire(); assert(child);
        assert(xrtValueArrayAppend(root,child) && xrtValueArrayAppend(root,child));
        xrtValueRelease(child);
    }
    return root;
}
static void wideObserved(const xvalue* value,size_t width)
{
    assert(xl6ArrayErasedSizeV1(value)==(int64_t)(width*2));
    xvalue* previous=NULL;
    for (size_t i=0;i<width;++i) {
        xvalue* a=xl6ArrayErasedReadV1(value,(int64_t)(i*2)),*b=xl6ArrayErasedReadV1(value,(int64_t)(i*2+1));
        assert(a && a==b && a!=previous);
        xrtValueRelease(previous); previous=a; xrtValueRelease(b);
    }
    xrtValueRelease(previous);
}
#if XL6_FACTORY_PROFILE == 2
static void deepPlan(xvalue** seeds)
{
    seeds[64]=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,10064,3001,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I8,false,3001); assert(seeds[64]);
    xvalue* alias=xl6ArrayEraseV1(seeds[64],10064,3001,1,sizeof(int64_t),xrtTypeInt64(),NULL);
    assert(alias); xrtValueRelease(alias);
    for (size_t i=64;i>0;--i) {
        seeds[i-1]=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,10000+i-1,10000+i,
            xrtValueRetain(seeds[i]),false); assert(seeds[i-1]);
        alias=xl6ArrayEraseV1(seeds[i-1],10000+i-1,10000+i,3,sizeof(xvalue*),xrtTypeValue(),NULL);
        assert(alias); xrtValueRelease(alias);
    }
}
static xvalue* sourceDeep(void)
{
    xvalue* child=xrtValueArray(); assert(child && xrtValueArrayAppendNew(child,xrtValueInt(65)));
    for (size_t i=0;i<64;++i) {
        xvalue* parent=xrtValueArray(); assert(parent);
        assert(xrtValueArrayAppend(parent,child) && xrtValueArrayAppend(parent,child));
        xrtValueRelease(child); child=parent;
    }
    return child;
}
static void deepObserved(const xvalue* source)
{
    xvalue* current=xrtValueRetain(source); assert(current);
    for (size_t i=0;i<64;++i) {
        assert(xl6ArrayErasedSizeV1(current)==2);
        xvalue* child=xl6ArrayErasedReadV1(current,0),*same=xl6ArrayErasedReadV1(current,1);
        assert(child && same==child); xrtValueRelease(same); xrtValueRelease(current); current=child;
    }
    assert(xl6ArrayErasedSizeV1(current)==1); xvalue* number=xl6ArrayErasedReadV1(current,0); int64_t value;
    assert(number && xrtValueGetInt(number,&value) && value==65); xrtValueRelease(number); xrtValueRelease(current);
}
#endif
static void chainObserved(const xvalue* value,size_t width,bool nullable)
{
    assert(xl6ArrayErasedSizeV1(value)==(int64_t)(width+nullable));
    xvalue* first=xl6ArrayErasedReadV1(value,0); assert(first);
    for (size_t i=1;i<width;++i) {
        xvalue* same=xl6ArrayErasedReadV1(value,(int64_t)i);
        assert(same==first); xrtValueRelease(same);
    }
#if XL6_FACTORY_PROFILE == 4
    xvalue* grand=xl6ArrayErasedReadV1(first,0),*same=xl6ArrayErasedReadV1(first,1);
    assert(grand && same==grand && xl6ArrayErasedSizeV1(grand)==1);
    xvalue* number=xl6ArrayErasedReadV1(grand,0); int64_t actual;
    assert(number && xrtValueGetInt(number,&actual) && actual==65);
    xrtValueRelease(number); xrtValueRelease(grand); xrtValueRelease(same);
#else
    observed(first);
#endif
    xrtValueRelease(first);
    if (nullable) { xvalue* absent=xl6ArrayErasedReadV1(value,(int64_t)width);
        assert(absent && xrtValueType(absent)==XVALUE_NULL); xrtValueRelease(absent); }
}
static void clearPlan(xvalue* const* seeds,size_t count)
{ for (size_t i=0;i<count;++i) assert(!seeds[i]); }
static void chainVectors(void)
{
    xvalue* seeds[3]={0}; size_t count=plan(seeds,true);
    xvalue* source=sourceWire(3,true),*original=xrtValueArrayGet(source,0),*outer=seeds[0];
    xvalue* result=xl6ArrayImportChainTakeV1(seeds,count,source);
    assert(result==outer && xrtValueArrayGet(source,0)==original && xrtValueCount(source)==4);
    clearPlan(seeds,count); chainObserved(result,3,true);
    count=plan(seeds,true); xvalue* again=xl6ArrayImportChainTakeV1(seeds,count,result);
    assert(again); chainObserved(again,3,true);
    xvalue* a=xl6ArrayErasedReadV1(result,0),*b=xl6ArrayErasedReadV1(again,0);
    assert(a==b && a!=original); xrtValueRelease(a); xrtValueRelease(b);
    xrtValueRelease(result); xrtValueRelease(again);
    count=plan(seeds,true); xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    result=xl6ArrayImportChainTakeV1(seeds,count,source);
    assert(result && xrtGetError()==prior); xrtClearError(); xrtValueRelease(result);
    /* Truncation and incorrect child policy fail before input traversal. */
    count=plan(seeds,false); xvalue* remaining=xrtValueRetain(seeds[count-1]);
    assert(!xl6ArrayImportChainTakeV1(seeds,count-1,source)); error(XERR_TYPE);
    xrtValueRelease(seeds[count-1]); seeds[count-1]=NULL; xrtValueRelease(remaining);
#if XL6_FACTORY_PROFILE == 2 || XL6_FACTORY_PROFILE == 4
    size_t badIndex=XL6_FACTORY_PROFILE==4 ? 2 : 1;
    uint64_t badOwner=XL6_FACTORY_PROFILE==4 ? 3001 : 1001,badElement=XL6_FACTORY_PROFILE==4 ? 4001 : 3001;
    count=plan(seeds,false); xrtValueRelease(seeds[badIndex]);
    seeds[badIndex]=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,badOwner,badElement,1,sizeof(int64_t),
        xrtTypeInt64(),XL6_VALUE_ADMIT_I64,false,badElement); assert(seeds[badIndex]);
    xvalue* alias=xl6ArrayEraseV1(seeds[badIndex],badOwner,badElement,1,sizeof(int64_t),xrtTypeInt64(),NULL);
    assert(alias); xrtValueRelease(alias);
    assert(!xl6ArrayImportChainTakeV1(seeds,count,source)); error(XERR_TYPE);
    clearPlan(seeds,count);
#endif
    count=plan(seeds,false); xrtValueRelease(seeds[1]); seeds[1]=NULL;
    xrtSetErrorKind(XERR_MEMORY);
    assert(!xl6ArrayImportChainTakeV1(seeds,count,source)); error(XERR_MEMORY);
    count=plan(seeds,false); xrtValueRelease(seeds[1]); seeds[1]=xrtValueRetain(seeds[0]);
    assert(!xl6ArrayImportChainTakeV1(seeds,count,source)); error(XERR_ARGUMENT);
    count=plan(seeds,false); xvalue* borrowed=seeds[0];
    assert(!xl6ArrayImportChainTakeV1(seeds,count,borrowed)); error(XERR_ARGUMENT);
    assert(!xl6ArrayImportChainTakeV1(NULL,1,source)); error(XERR_ARGUMENT);
    count=plan(seeds,false);
    assert(!xl6ArrayImportChainTakeV1(seeds,66,source)); error(XERR_ARGUMENT);
    for (size_t i=0;i<count;++i) xrtValueRelease(seeds[i]);
    count=plan(seeds,false); XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout;
    const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(seeds[1],&pin,&layout,&converter)==1 && pin);
    assert(!xl6ArrayImportChainTakeV1(seeds,count,source)); error(XERR_STATE);
    xl6SequenceEndV1(pin); clearPlan(seeds,count);
    /* An error late in the input cannot leak a partially converted graph. */
    xrtValueRelease(source); source=sourceWire(3,true);
    count=plan(seeds,false); assert(!xl6ArrayImportChainTakeV1(seeds,count,source)); error(XERR_TYPE);
    assert(xrtValueCount(source)==4); xrtValueRelease(source);
    /* A Stack is a genuine native object, not persistence Array syntax. The
     * endpoint retains a matching Stack; a wire child cannot fabricate it. */
    const xrttype* stackType; unsigned int stackTransfer;
    xvalue* stack=factory(XL6_SEQUENCE_STACK_V1,&stackType,&stackTransfer); assert(stack);
    seeds[0]=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,9001,1001,xrtValueRetain(stack),false);
    assert(seeds[0]);
    xvalue* projection=xl6ArrayEraseV1(seeds[0],9001,1001,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    assert(projection); xrtValueRelease(projection);
    source=xrtValueArray(); assert(source && xrtValueArrayAppend(source,stack));
    result=xl6ArrayImportChainTakeV1(seeds,1,source); assert(result);
    xvalue* kept=xl6ArrayErasedReadV1(result,0); assert(kept==stack);
    xrtValueRelease(kept); xrtValueRelease(result); xrtValueRelease(source);
    seeds[0]=xl6SequenceNestedCreateTakeV2(XL6_SEQUENCE_ARRAY_V1,9001,1001,xrtValueRetain(stack),false);
    assert(seeds[0]); projection=xl6ArrayEraseV1(seeds[0],9001,1001,3,sizeof(xvalue*),xrtTypeValue(),NULL);
    assert(projection); xrtValueRelease(projection);
    source=sourceWire(1,false); assert(!xl6ArrayImportChainTakeV1(seeds,1,source)); error(XERR_TYPE);
    xrtValueRelease(source); xrtValueRelease(stack);
    /* Even a failed malformed plan consumes its owners. A destructor cannot
     * replace TYPE in either the XRT slot or the immediate foreign boundary. */
    source=sourceWire(2,false); void* raw=(void*)(uintptr_t)1;
    seeds[0]=xrtValueHandleTake(&raw,&failingOps,NULL); assert(seeds[0] && !raw);
    unsigned before=cleanupCalls; xl6_error_scope outerScope={0}; assert(xl6ErrorScopeBegin(&outerScope));
    assert(!xl6ArrayImportChainTakeV1(seeds,1,source) && cleanupCalls==before+1);
    xl6_error_result outcome=xl6ErrorScopeEnd(&outerScope);
    assert(!outcome.ok && outcome.error && xrtErrorKind(outcome.error)==XERR_TYPE);
    xrtErrorFree(outcome.error); xrtClearError(); xrtValueRelease(source);
}
static void chainFault(const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    bool wide=!strcmp(operation,"wide"),deep=!strcmp(operation,"deep");
    xvalue* source=NULL,*seeds[65]={0}; size_t count=0;
#if XL6_FACTORY_PROFILE == 2
    if (deep) { source=sourceDeep(); deepPlan(seeds); count=65; }
    else
#endif
    { assert(!deep); source=wide ? sourceWide(24) : sourceWire(32,true); count=plan(seeds,!wide); }
    xvalue* first=xrtValueArrayGet(source,0);
    assert(xrtMemDebugFailAfter(point)); xvalue* result=xl6ArrayImportChainTakeV1(seeds,count,source);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else {
        assert(result && !xrtGetError());
#if XL6_FACTORY_PROFILE == 2
        if (deep) deepObserved(result); else
#endif
        if (wide) wideObserved(result,24); else chainObserved(result,32,true);
    }
    clearPlan(seeds,count);
    assert(xrtValueCount(source)==(deep ? 2u : wide ? 48u : 33u) && xrtValueArrayGet(source,0)==first);
    xrtValueRelease(result); xrtValueRelease(source); balanced(&before);
    printf("array-chain profile=%u operation=%s point=%zu hit=%u balanced\n",XL6_FACTORY_PROFILE,operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) { chainFault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (size_t i=0;i<25;++i) chainVectors();
    balanced(&before); printf("array-chain profile=%u: 25 complete graph cycles balanced\n",XL6_FACTORY_PROFILE); return 0;
}
