#define main xl6PrimitiveImportFixtureMain
#include "test/cases/stdlib_array_primitive_admission_native_host.c"
#undef main
#include "src/runtime/xl6_array_import.h"
static xvalue* busySource;
static xvalue* busyTarget;
static unsigned codecReads,mode;
static bool proxyToTyped(const xvalue* value,const xrttype* type,ptr target,ptr context)
{
    assert(!context); Spec s=spec(rules[0]);
    /* The outer conversion has already initialized target. Forward the
     * codec callback, not the whole initializing conversion a second time. */
    return s.converter ? s.converter->ToTyped(value,type,target,s.converter->Context) :
        xrtValueToTyped(value,type,target,NULL);
}
static xvalue* proxyFromTyped(const xrttype* type,const void* source,ptr context)
{
    assert(!context); ++codecReads;
    if (mode==1) {
        assert(!xl6ArrayErasedClearV1(busySource)); error(XERR_STATE);
        assert(!xl6ArrayErasedClearV1(busyTarget)); error(XERR_STATE);
    } else if (mode==2) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    else if (mode==3) return NULL;
    else if (mode==4) return xrtValueBool(false);
    Spec s=spec(rules[0]); return xrtValueFromTyped(type,source,s.converter);
}
static const xvalueconverter proxy={.ToTyped=proxyToTyped,.FromTyped=proxyFromTyped};
static xvalue* preparedSeed(const Spec* s,const xvalueconverter* converter)
{
    xvalue* value=create(s); if (!value) return NULL;
    xvalue* alias=xl6ArrayEraseV1(value,LAYOUT(s),converter); assert(alias==value);
    xrtValueRelease(alias); return value;
}
static xvalue* sourceWire(const Spec* s)
{
    xvalue* good=element(s),*source=xrtValueArray(); assert(good && source);
    assert(xrtValueArrayAppend(source,good) && xrtValueArrayAppend(source,good)); xrtValueRelease(good);
    if (s->nullable) assert(xrtValueArrayAppend(source,xrtValueNull()));
    return source;
}
static void observations(const Spec* s,const xvalue* source)
{
    assert(xl6ArraySizeV1(source,LAYOUT(s),false)==(s->nullable ? 3 : 2));
    xvalue* good=element(s); assert(good); observed(s,source,0,good); observed(s,source,1,good);
    xrtValueRelease(good);
    if (s->nullable) {
        xvalue* absent=xl6ArrayErasedReadV1(source,2); assert(absent && xrtValueType(absent)==XVALUE_NULL); xrtValueRelease(absent);
    }
}
static void importVectors(const Spec* s)
{
    xvalue* wire=sourceWire(s),*source=xl6ArrayImportTakeV1(preparedSeed(s,&proxy),wire); assert(source);
    unsigned prior=codecReads;
    xvalue* same=xl6ArrayImportTakeV1(preparedSeed(s,&proxy),source);
    assert(same && codecReads==prior); /* Native copy does not box each slot. */
    observations(s,same); xrtValueRelease(same);
    prior=codecReads;
    same=xl6ArrayImportTakeV1(preparedSeed(s,s->converter),source); assert(same);
#if PROFILE >= 7
    /* For these custom descriptors the codec really executes; builtin XRT
     * fast paths intentionally precede a custom converter for other types. */
    assert(codecReads>prior);
#else
    (void)prior;
#endif
    observations(s,same); xrtValueRelease(same);
#if PROFILE == 9
    busySource=source;
    mode=1; busyTarget=preparedSeed(s,s->converter); assert(busyTarget);
    same=xl6ArrayImportTakeV1(busyTarget,source); assert(same);
    mode=0; observations(s,same); xrtValueRelease(same); observations(s,source);
    for (unsigned failure=2;failure<=4;++failure) {
        mode=failure; busyTarget=preparedSeed(s,s->converter); assert(busyTarget);
        assert(!xl6ArrayImportTakeV1(busyTarget,source)); error(XERR_TYPE);
        mode=0; observations(s,source);
    }
    busySource=NULL; busyTarget=NULL;
#endif
    mode=0; xrtValueRelease(source); xrtValueRelease(wire);
}
static void importFault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* wire=sourceWire(s),*source=wire;
    if (strcmp(operation,"wire")) { source=xl6ArrayImportTakeV1(preparedSeed(s,&proxy),wire); assert(source); }
    const xvalueconverter* converter=!strcmp(operation,"native") ? &proxy : s->converter;
    xvalue* target=preparedSeed(s,converter); assert(target);
    assert(xrtMemDebugFailAfter(point)); xvalue* result=xl6ArrayImportTakeV1(target,source);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else { assert(result && !xrtGetError()); observations(s,result); }
    if (source!=wire) { observations(s,source); xrtValueRelease(source); }
    assert(xrtValueCount(wire)==(s->nullable ? 3u : 2u)); xrtValueRelease(result); xrtValueRelease(wire);
    balanced(&before); printf("primitive-import profile=%d %s point=%zu hit=%d balanced\n",PROFILE,operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) {
        assert(!strcmp(argv[1],"wire") || !strcmp(argv[1],"native") || !strcmp(argv[1],"fallback"));
        for (size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) { Spec s=spec(rules[i]); importFault(&s,argv[1],(size_t)strtoull(argv[2],NULL,10)); }
        return 0;
    }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (unsigned cycle=0;cycle<100;++cycle)
        for (size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) { Spec s=spec(rules[i]); importVectors(&s); }
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    if (before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes || xrtGetError())
        fprintf(stderr,"import profile=%d live %zu/%zu bytes %zu/%zu alloc/free %zu/%zu error=%d\n",PROFILE,
            before.LiveCount,after.LiveCount,before.LiveBytes,after.LiveBytes,
            after.AllocCount-before.AllocCount,after.FreeCount-before.FreeCount,
            xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : 0);
    balanced(&before); printf("Primitive import profile=%d: 100 exact codec cycles balanced\n",PROFILE); return 0;
}
