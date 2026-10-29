/* Actual public compiler/runtime, no white-box API inclusion or synthetic
 * module owner. Native graph/OOM gate and Windows TCC MEM_FREE gate provide
 * separate evidence, not instrumentation claims about this shared library. */
#include "xlc.h"
#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned selected, droppedMarkers;
static xvalue* escaped;
static bool liveAllocation(const xmemdebugallocation* allocation, void* unused)
{
    (void)unused;
    fprintf(stderr,"live %zu at %p %s:%u\n",allocation->Size,allocation->Address,
        allocation->File ? allocation->File : "?",allocation->Line); return true;
}
static void capture(xvalue* value)
{
    assert(value && !escaped);
    escaped = selected == 3 ? xrtValueDeepClone(value) : xrtValueRetain(value);
    assert(escaped && (selected != 3 || escaped != value) && !xrtGetError());
}
static void dropped(int64_t id, int64_t count)
{
    assert((id == 7 && count == 1) || (id == 42 && count == 2) || (id == 9 && count == 0));
    xrtownershipscope freeze = {0};
    assert(xrtOwnershipFreezeTryBegin(&freeze) && xrtOwnershipScopeEnd(&freeze)); ++droppedMarkers;
}
static void transaction(const char* headers, const char* temporary, const char* runtime, const char* libraries,
    const char* fixture)
{
#ifdef XLIR_GRAPH_EMBEDDED_VFS
    (void)headers; (void)libraries;
    assert(xlRuntimeInit());
    XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tempDir = temporary; options.tccDllPath = runtime; options.tccLibPath = NULL;
    XL_Context* context = xlCreateContext(&options); assert(context);
    assert(xlRegisterCFunction(context,"stack_graph_capture",(void*)capture));
    assert(xlRegisterCFunction(context,"stack_graph_dropped",(void*)dropped));
    XL_Module* module = xlCompileScriptFileToModule(context,fixture);
    if (!module) { fprintf(stderr,"%s\n",xlGetLastError(context)); abort(); }
#else
    XLC_ContextOptions options = xlcDefaultContextOptions();
    options.workDir = "."; options.tempDir = temporary; options.tccDllPath = runtime; options.tccLibPath = libraries;
    XLC_Context* compiler = xlcCreateContext(&options); assert(compiler);
    assert(xlcAddIncludePath(compiler,headers));
    assert(xlcRegisterCFunction(compiler,"stack_graph_capture",(void*)capture));
    assert(xlcRegisterCFunction(compiler,"stack_graph_dropped",(void*)dropped));
    assert(xlcAddSourceFile(compiler,fixture));
    if (!xlcCompile(compiler)) { fprintf(stderr,"%s\n",xlcGetLastError(compiler)); abort(); }
    XL_Module* module = xlcRelocateModule(compiler); assert(module);
    XL_Context* context = xlcRuntimeContext(compiler);
#endif
    assert(xlActivateModule(context,module,NULL));
    typedef int64_t (*Setup)(int64_t);
    Setup setup; void* symbol = xlModuleGetSymbol(module,"stack_graph_setup"); memcpy(&setup,&symbol,sizeof(setup)); assert(setup);
    escaped = NULL; droppedMarkers = 0; assert(setup(selected) == 42 && !xrtGetError());
    assert(!droppedMarkers && (escaped != NULL) == (selected != 0)); xlDestroyModule(module);
    if (selected) { assert(!droppedMarkers); xrtValueRelease(escaped); escaped = NULL; }
#ifdef XLIR_GRAPH_EMBEDDED_VFS
    xlDestroyContext(context);
#else
    xlcDestroyContext(compiler);
#endif
    xlRuntimeUnit();
    assert(droppedMarkers == (selected ? 3u : 1u) && !xrtGetError());
}
int main(int argc, char** argv)
{
    assert(argc == 6 || argc == 7); selected = (unsigned)strtoul(argv[1],NULL,10); assert(selected < 4);
    const char* fixture=argc==7?argv[6]:"test/cases/stdlib_stack_ownership.xl";
    assert(xrtMemDebugEnable(true));
    /* Warm actual process-level TCC/environment caches before the ledger. */
    transaction(argv[2],argv[3],argv[4],argv[5],fixture); transaction(argv[2],argv[3],argv[4],argv[5],fixture);
    for (unsigned round = 0; round < 25; ++round) {
        xmemdebugsnapshot before, after; xrtMemDebugSnapshot(&before);
        transaction(argv[2],argv[3],argv[4],argv[5],fixture); xrtMemDebugSnapshot(&after);
        if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes ||
            after.AllocCount-before.AllocCount != after.FreeCount-before.FreeCount ||
            before.InvalidFreeCount != after.InvalidFreeCount || before.DoubleFreeCount != after.DoubleFreeCount)
        {
            fprintf(stderr,"public mode=%u round=%u live=%llu->%llu bytes=%llu->%llu alloc=%llu free=%llu invalid=%llu->%llu double=%llu->%llu\n",
                selected,round,(unsigned long long)before.LiveCount,(unsigned long long)after.LiveCount,
                (unsigned long long)before.LiveBytes,(unsigned long long)after.LiveBytes,
                (unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)(after.FreeCount-before.FreeCount),
                (unsigned long long)before.InvalidFreeCount,(unsigned long long)after.InvalidFreeCount,
                (unsigned long long)before.DoubleFreeCount,(unsigned long long)after.DoubleFreeCount);
            xrtMemDebugVisitLive(liveAllocation,NULL);
        }
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount);
    }
    printf("stack public graph mode=%u: 25 actual source compile/JIT/retire lifetimes, strong roots and intact-field finalizers; balanced\n",selected);
    return 0;
}
