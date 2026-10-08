/* Real public compilation and module retirement. No generated destructor or
 * ownership resolver is replaced; all owning graph edges originate in xlang. */
#include "include/xlc.h"
#include "release/include/xrt_decl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static unsigned selected,family;
static _Atomic unsigned droppedMask;
static xvalue* escaped;
static void liveAllocation(const xmemdebugallocation* allocation,void* unused)
{
    (void)unused;
    fprintf(stderr,"live %zu at %p %s:%u\n",allocation->Size,allocation->Address,
        allocation->File ? allocation->File : "?",allocation->Line);
}
static bool visitLive(const xmemdebugallocation* allocation,void* context)
{ liveAllocation(allocation,context); return true; }
static void capture(xvalue* value)
{
    assert(value && !escaped);
    escaped=xrtValueRetain(value); assert(escaped && !xrtGetError());
}
static void dropped(int64_t id,int64_t values,int64_t arrays,int64_t second)
{
    unsigned index=id>=7 && id<=11 ? (unsigned)(id-7) : id==42 ? 5u : id==90 ? 6u : 99u;
    assert(index<7);
    unsigned shape=index<5 ? index : family;
    int64_t counts[]={values,arrays,second};
    int64_t expected[3]={0};
    if (index!=6) {
        if (shape==2) expected[1]=index==5 ? 2 : 1;
        else {
            expected[0]=(shape==1 ? 2 : 1)+(index==5 ? 1 : 0);
            if (shape==3) expected[2]=expected[0];
            if (shape==4) expected[2]=1;
        }
    }
    for (unsigned at=0;at<3;++at) {
        if (counts[at]!=expected[at]) fprintf(stderr,"id=%lld family=%u slot=%u count=%lld expected=%lld\n",
            (long long)id,family,at,(long long)counts[at],(long long)expected[at]);
        assert(counts[at]==expected[at]);
    }
    xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    unsigned prior=atomic_fetch_or_explicit(&droppedMask,1u<<index,memory_order_release);
    assert(!(prior & (1u<<index)));
}
static void transaction(const char* headers,const char* temporary,const char* runtime,
    const char* libraries,const char* fixture)
{
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir="."; options.tempDir=temporary;
    options.tccDllPath=runtime; options.tccLibPath=libraries;
    XLC_Context* compiler=xlcCreateContext(&options); assert(compiler);
    assert(xlcAddIncludePath(compiler,headers));
    assert(xlcRegisterCFunction(compiler,"tree_graph_capture",(void*)capture));
    assert(xlcRegisterCFunction(compiler,"tree_graph_dropped",(void*)dropped));
    assert(xlcAddSourceFile(compiler,fixture));
    if (!xlcCompile(compiler)) { fprintf(stderr,"%s\n",xlcGetLastError(compiler)); abort(); }
    XL_Module* module=xlcRelocateModule(compiler); assert(module);
    XL_Context* context=xlcRuntimeContext(compiler); assert(xlActivateModule(context,module,NULL));
    typedef int64_t (*Setup)(int64_t,int64_t);
    Setup setup; void* symbol=xlModuleGetSymbol(module,"tree_graph_setup");
    memcpy(&setup,&symbol,sizeof(setup)); assert(setup);
    escaped=NULL; atomic_store_explicit(&droppedMask,0,memory_order_relaxed);
    assert(setup(selected,family)==42 && !xrtGetError());
    assert(!atomic_load_explicit(&droppedMask,memory_order_acquire) && (escaped!=NULL)==(selected!=0));
    xlDestroyModule(module);
    if (selected) {
        assert(!atomic_load_explicit(&droppedMask,memory_order_acquire));
        xrtValueRelease(escaped); escaped=NULL;
    }
    xlcDestroyContext(compiler); xlRuntimeUnit();
    unsigned actual=atomic_load_explicit(&droppedMask,memory_order_acquire),expected=selected ? 127u : 31u;
    if (actual!=expected || xrtGetError()) fprintf(stderr,"tree retirement mode=%u family=%u mask=%u expected=%u kind=%d error=%s\n",
        selected,family,actual,expected,(int)xrtErrorKind(xrtGetError()),
        xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
    assert(actual==expected && !xrtGetError());
}
int main(int argc,char** argv)
{
    assert(argc==8);
    selected=(unsigned)strtoul(argv[1],NULL,10); family=(unsigned)strtoul(argv[2],NULL,10);
    assert(selected<4 && family<5 && (selected || !family));
    assert(xrtMemDebugEnable(true));
    transaction(argv[3],argv[4],argv[5],argv[6],argv[7]);
    transaction(argv[3],argv[4],argv[5],argv[6],argv[7]);
    for (unsigned round=0;round<12;++round) {
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        transaction(argv[3],argv[4],argv[5],argv[6],argv[7]); xrtMemDebugSnapshot(&after);
        bool balanced=before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
            before.UseAfterFreeCount==after.UseAfterFreeCount;
        if (!balanced) xrtMemDebugVisitLive(visitLive,NULL);
        assert(balanced);
    }
    printf("tree language graph mode=%u family=%u: 12 measured actual compile/JIT/retire lifetimes, "
        "five cycle families, external roots and intact once-only finalizers; balanced\n",selected,family);
    return 0;
}
