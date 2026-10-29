/* Real public compiler/host module: no fabricated ownership resolver or
 * generated destructor replacement. All graph roots originate in xlang. */
#include "xlc.h"
#include "xrt_decl.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static unsigned selected, family;
static _Atomic unsigned droppedMask;
static xvalue* escaped;
static bool liveAllocation(const xmemdebugallocation* allocation, void* unused)
{
    (void)unused;
    fprintf(stderr,"live %zu at %p %s:%u\n",allocation->Size,allocation->Address,
        allocation->File ? allocation->File : "?",allocation->Line);
    return true;
}
static void capture(xvalue* value)
{
    assert(value && !escaped);
    escaped = selected == 3 ? xrtValueDeepClone(value) : xrtValueRetain(value);
    assert(escaped && (selected != 3 || escaped != value) && !xrtGetError());
}
static void dropped(int64_t id,int64_t a,int64_t b,int64_t c,int64_t futures,int64_t arrays,int64_t callbacks)
{
    unsigned index = id>=7 && id<=12 ? (unsigned)(id-7) : id==42 ? 6u : id==90 ? 7u : 99u;
    assert(index<8);
    int64_t counts[]={a,b,c,futures,arrays,callbacks};
    unsigned populated=index<6 ? index : family;
    for (unsigned i=0;i<6;++i) {
        int64_t expected=index==7 ? 0 : i==populated ? (index==6 || index==4 ? 2 : 1) : 0;
        if (counts[i]!=expected)
            fprintf(stderr,"id=%lld family=%u slot=%u count=%lld expected=%lld\n",
                (long long)id,family,i,(long long)counts[i],(long long)expected);
        assert(counts[i]==expected);
    }
    xrtownershipscope freeze={0};
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze));
    unsigned previous=atomic_fetch_or_explicit(&droppedMask,1u<<index,memory_order_release);
    assert(!(previous & (1u<<index)));
}
static void transaction(const char* headers,const char* temporary,const char* runtime,
                        const char* libraries,const char* fixture)
{
#ifdef XLIR_GRAPH_EMBEDDED_VFS
    (void)headers; (void)libraries;
    assert(xlRuntimeInit());
    XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir="."; options.tempDir=temporary;
    options.tccDllPath=runtime; options.tccLibPath=NULL;
    XL_Context* context=xlCreateContext(&options); assert(context);
    assert(xlRegisterCFunction(context,"queue_graph_capture",(void*)capture));
    assert(xlRegisterCFunction(context,"queue_graph_dropped",(void*)dropped));
    XL_Module* module=xlCompileScriptFileToModule(context,fixture);
    if (!module) { fprintf(stderr,"%s\n",xlGetLastError(context)); abort(); }
#else
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir="."; options.tempDir=temporary;
    options.tccDllPath=runtime; options.tccLibPath=libraries;
    XLC_Context* compiler=xlcCreateContext(&options); assert(compiler);
    assert(xlcAddIncludePath(compiler,headers));
    assert(xlcRegisterCFunction(compiler,"queue_graph_capture",(void*)capture));
    assert(xlcRegisterCFunction(compiler,"queue_graph_dropped",(void*)dropped));
    assert(xlcAddSourceFile(compiler,fixture));
    if (!xlcCompile(compiler)) { fprintf(stderr,"%s\n",xlcGetLastError(compiler)); abort(); }
    XL_Module* module=xlcRelocateModule(compiler); assert(module);
    XL_Context* context=xlcRuntimeContext(compiler);
#endif
    assert(xlActivateModule(context,module,NULL));
    typedef int64_t (*Setup)(int64_t,int64_t);
    Setup setup; void* symbol=xlModuleGetSymbol(module,"queue_graph_setup");
    memcpy(&setup,&symbol,sizeof(setup)); assert(setup);
    escaped=NULL; atomic_store_explicit(&droppedMask,0,memory_order_relaxed);
    assert(setup(selected,family)==42 && !xrtGetError());
    assert(!atomic_load_explicit(&droppedMask,memory_order_acquire) && (escaped!=NULL)==(selected!=0));
    xvalue* weak=escaped ? xrtValueWeakRef(escaped) : NULL;
    assert(!escaped || weak);
    xlDestroyModule(module);
    if (selected) {
        /* This native Retain or separately cloned Value shell is a genuine
         * external graph root. No finalizer or global may retire under it. */
        assert(!atomic_load_explicit(&droppedMask,memory_order_acquire));
        xrtValueRelease(escaped); escaped=NULL;
    }
#ifdef XLIR_GRAPH_EMBEDDED_VFS
    xlDestroyContext(context);
#else
    xlcDestroyContext(compiler);
#endif
    xlRuntimeUnit();
    if (atomic_load_explicit(&droppedMask,memory_order_acquire)!=(selected ? 255u : 63u) || xrtGetError()) {
        fprintf(stderr,"retirement mode=%u family=%u mask=%u expected=%u kind=%d error=%s\n",
            selected,family,atomic_load_explicit(&droppedMask,memory_order_acquire),selected ? 255u : 63u,
            (int)xrtErrorKind(xrtGetError()),xrtGetError() ? xrtErrorMessage(xrtGetError()) : "none");
        xvalue* anchor=weak ? xrtValueWeakRefLock(weak) : NULL;
        if (anchor && xrtValueType(anchor)!=XVALUE_NULL) {
            xrtownershipref root=xrtValueOwnership(anchor);
            xrtownershipscope freeze={0}; xrtownershipsnapshot* snapshot=NULL;
            assert(xrtOwnershipFreezeTryBegin(&freeze));
            bool ok=xrtOwnershipSnapshotCreate(&root,1,NULL,0,NULL,NULL,&snapshot);
            fprintf(stderr,"snapshot ok=%d kind=%d anchor=%p module=%p\n",ok,(int)xrtErrorKind(xrtGetError()),(void*)anchor,(void*)module);
            if (ok) for (size_t at=0;at<xrtOwnershipSnapshotNodeCount(snapshot);++at) {
                xrtownershipnode node; assert(xrtOwnershipSnapshotNode(snapshot,at,&node));
                fprintf(stderr,"node=%zu data=%p ops=%p strong=%zu internal=%zu reachable=%d%s\n",at,
                    node.Reference.Data,(void*)node.Reference.Ops,node.StrongCount,node.InternalCount,node.Reachable,
                    node.Reference.Data==module ? " module" : "");
            }
            xrtOwnershipSnapshotDestroy(snapshot); assert(xrtOwnershipScopeEnd(&freeze));
        }
        xrtValueRelease(anchor);
    }
    xrtValueRelease(weak);
    assert(atomic_load_explicit(&droppedMask,memory_order_acquire)==(selected ? 255u : 63u) && !xrtGetError());
}
int main(int argc,char** argv)
{
    assert(argc==8);
    selected=(unsigned)strtoul(argv[1],NULL,10); family=(unsigned)strtoul(argv[2],NULL,10);
    assert(selected<4 && family<6 && (selected || !family));
    assert(xrtMemDebugEnable(true));
    transaction(argv[3],argv[4],argv[5],argv[6],argv[7]);
    transaction(argv[3],argv[4],argv[5],argv[6],argv[7]);
    for (unsigned round=0;round<25;++round) {
        xmemdebugsnapshot before,after; xrtMemDebugSnapshot(&before);
        transaction(argv[3],argv[4],argv[5],argv[6],argv[7]);
        xrtMemDebugSnapshot(&after);
        bool balanced=before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
            after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
            before.UseAfterFreeCount==after.UseAfterFreeCount;
        if (!balanced) xrtMemDebugVisitLive(liveAllocation,NULL);
        assert(balanced);
    }
    printf("queue language graph mode=%u family=%u: 25 actual compile/JIT/retire lifetimes, "
           "six cycle families, external roots and intact once-only finalizers; balanced\n",selected,family);
    return 0;
}
