/* Actual API/TCC binding, copied resident callbacks, current-thread module
 * count/trace and foreign activity refusal. No native restore Cgen claim. */
#include "xl.h"
#include "src/api/xl_module_state.h"
#include "src/runtime/xl6_code_lease.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
static XL_Module* executing;
static const XL6_OwnershipReadBoundaryV1* copied;
static int64_t (*entry)(void);
static xatomic64 ready = {0}, proceed = {0};
static bool parking;
static size_t reads, observations;
static void* toolAddress;
static size_t toolSize;
static bool captureTool;
static bool live(const xmemdebugallocation* allocation, void* ignored)
{
    (void)ignored;
    if (captureTool) { assert(!toolAddress); toolAddress = allocation->Address; toolSize = allocation->Size; }
    else assert(allocation->Address == toolAddress && allocation->Size == toolSize);
    return true;
}
static bool visit(xrtownershipref ref,void* context) { if (ref.Data) ++*(size_t*)context; return true; }
static void inspect(bool foreign)
{
    XL6_OwnershipReadScopeV1 read = {0}, nested = {0}; xrtownershipscope freeze = {0};
    assert(copied && copied->Begin(&read) == XL6_OWNERSHIP_READ_READY_V1);
    assert(copied->Begin(&nested) == XL6_OWNERSHIP_READ_BUSY_V1 && !nested.self && !nested.thread);
    assert(xrtOwnershipFreezeTryBegin(&freeze));
    xrtownershipref module = {executing,executing->codeOwnerGraph.ops};
    assert(copied->Admits(&read,module) == !foreign);
    size_t count = 0, edges = 0;
    if (foreign) assert(!module.Ops->Count(module.Data,&count));
    else {
        assert(module.Ops->Count(module.Data,&count) && count == 1u + executing->activeCalls +
            executing->codeLeases + executing->preparationRefs + executing->snapshotRefs);
        assert(module.Ops->Trace(module.Data,visit,&edges) && edges == 1);
        XL6_OwnershipReadScopeV1 copy = read; assert(!copied->Admits(&copy,module));
    }
    assert(xrtOwnershipScopeEnd(&freeze)); copied->End(&read);
    assert(!read.self && !read.thread); ++reads;
}
void read_binding_probe(void)
{
    assert(executing->activeCalls == 0);
    assert(xl6CodeOwnerScopeActive(&executing->codeOwnerGraph.base)); inspect(false);
    if (parking) {
        xrtAtomic64Store(&ready,1,XMEMORY_RELEASE);
        while (!xrtAtomic64Load(&proceed,XMEMORY_ACQUIRE)) xrtThreadYield();
    }
}
void read_binding_observe(const XL6_OwnershipReadBoundaryV1* boundary)
{
    const XL6_OwnershipReadBoundaryV1* actual = xlModuleOwnershipReadBoundaryV1();
    assert(boundary && boundary->size == sizeof(*actual) && boundary->Begin == actual->Begin &&
        boundary->Admits == actual->Admits && boundary->End == actual->End); ++observations;
}
static void invoke(void)
{
    xl6_foreign_error_scope scope = {0}; assert(xl6ForeignCallBegin(&scope));
    int64_t n = entry(); xl6_foreign_error_result outcome = xl6ForeignCallEnd(&scope);
    assert(n == 17 && outcome.ok && !outcome.error);
}
static int worker(void* ignored) { (void)ignored; invoke(); return 0; }
static void nativeBindings(XL_Context* context)
{
    static const char body[] =
        "#include <__xl_runtime/xl6_ownership_read_abi.h>\n"
        "extern void read_binding_observe(const XL6_OwnershipReadBoundaryV1*);\n"
        "static bool trace(XL6_ModuleOwnershipVisit v,void* c){(void)v;(void)c;return true;}\n"
        "static int finish(void){return 1;}\n"
        "#if !defined(OMIT_OWNER)\n"
        "int xl_script_bind_code_owner(const XL6_CodeOwner* o){(void)o;return 1;}\n"
        "#endif\n"
        "bool xl_script_module_ownership_trace(XL6_ModuleOwnershipVisit v,void* c){return trace(v,c);}\n"
        "#if !defined(OMIT_READ)\n"
        "const XL6_ModuleReadContractV1* xl_script_module_ownership_read_v1(void){\n"
        "static const XL6_ModuleReadContractV1 a={sizeof(a),0x584C364D524431ULL,xl_script_module_ownership_trace};return &a;}\n"
        "#endif\n"
        "#if !defined(OMIT_RETIRE)\n"
        "const XL6_ModuleRetirementV1* xl_script_module_retirement_v1(void){\n"
        "static const XL6_ModuleRetirementV1 a={sizeof(a),0x584C364D525431ULL,xl_script_module_ownership_trace,finish};return &a;}\n"
        "#endif\n"
        "int xl_script_bind_ownership_read_v1(const XL6_OwnershipReadBoundaryV1* b){read_binding_observe(b);return ACCEPT;}\n";
    const char* prefix[] = {"#define ACCEPT 0\n", "#define ACCEPT 1\n#define OMIT_OWNER\n",
        "#define ACCEPT 1\n#define OMIT_READ\n", "#define ACCEPT 1\n#define OMIT_RETIRE\n", "#define ACCEPT 1\n"};
    assert(xlRegisterCFunction(context,"read_binding_observe",(void*)read_binding_observe));
    for (size_t i = 0; i < 5; ++i) {
        char text[4096]; int n = snprintf(text,sizeof(text),"%s%s",prefix[i],body);
        assert(n > 0 && (size_t)n < sizeof(text)); size_t before = observations;
        XL_Module* module = xlCompileCSourceStringToModule(context,text);
        if (i < 4) { assert(!module && xlGetLastError(context)); }
        else { assert(module); xlDestroyModule(module); }
        assert(observations == before + (i == 0 || i == 4)); xrtClearError();
    }
}
int main(int argc,char** argv)
{
    assert(argc == 7); XL_ContextOptions options = xlDefaultContextOptions();
    options.workDir = "."; options.tccDllPath = argv[1]; options.tccLibPath = argv[2]; options.tempDir = argv[3];
    xl6ProductMemoryBegin(SIZE_MAX);
    XL_Context* warm = xlCreateContext(&options); assert(warm && xlAddIncludePath(warm,argv[4]));
    XL_Module* tool = xlCompileCSourceStringToModule(warm,"int read_bind_tool_probe(void){return 42;}\n"); assert(tool);
    int (*warmProbe)(void) = (int (*)(void))xlModuleGetSymbol(tool,"read_bind_tool_probe"); assert(warmProbe() == 42);
    xlDestroyModule(tool); xlDestroyContext(warm); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot baseline; xrtMemDebugSnapshot(&baseline); assert(baseline.LiveCount == 1);
    captureTool = true; assert(xrtMemDebugVisitLive(live,NULL)); captureTool = false;
    XL_Context* context = xlCreateContext(&options); assert(context && xlAddIncludePath(context,argv[4]));
    assert(xlRegisterCFunction(context,"read_binding_probe",(void*)read_binding_probe));
    // The source compiler produced this C without any native lookup replacement.
    executing = xlCompileCSourceFileToModule(context,argv[5]);
    if (!executing) fprintf(stderr,"%s\n",xlGetLastError(context) ? xlGetLastError(context) : "no diagnostic");
    assert(executing && executing->ownershipReadContract && executing->retirementContract);
    const XL6_OwnershipReadBoundaryV1* (*get)(void) =
        (const XL6_OwnershipReadBoundaryV1* (*)(void))xlModuleGetSymbol(executing,"read_binding_bound_cap"); assert(get);
    copied = get(); assert(copied && "resident read boundary was bound"); read_binding_observe(copied);
    assert(copied != xlModuleOwnershipReadBoundaryV1());
    int (*bind)(const XL6_OwnershipReadBoundaryV1*) = (int (*)(const XL6_OwnershipReadBoundaryV1*))
        xlModuleGetSymbol(executing,"xl_script_bind_ownership_read_v1");
    assert(bind); xl6ProductMemoryBegin(0); assert(!bind(copied)); assert(!xl6ProductMemoryEnd());
    entry = (int64_t (*)(void))xlModuleGetSymbol(executing,"read_binding_entry"); assert(entry); invoke();
    parking = true; xthread* thread = xrtThreadCreate(worker,NULL,0); assert(thread);
    while (!xrtAtomic64Load(&ready,XMEMORY_ACQUIRE)) xrtThreadYield();
    inspect(true); xrtAtomic64Store(&proceed,1,XMEMORY_RELEASE);
    assert(xrtThreadWait(thread) == XWAIT_OK && xrtThreadExitCode(thread) == 0); xrtThreadDestroy(thread); parking = false;
    invoke(); xlDestroyModule(executing); executing = NULL; copied = NULL;
    XL_Module* pruned = xlCompileCSourceFileToModule(context,argv[6]); assert(pruned);
    assert(!xlModuleGetSymbol(pruned,"xl_script_bind_ownership_read_v1")); xlDestroyModule(pruned);
    nativeBindings(context);
    // Compile original xlang as well: automatic binding before initialization.
    const char* script = "class AutoRead\n derive value\n int n=29\nend\n"
        "AutoRead result=json.parse::<AutoRead>(\"{}\")\nassert result.n==29\n";
    assert(xlRunScriptString(context,script));
    xlDestroyContext(context); xlRuntimeUnit(); xrtClearError();
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount == baseline.LiveCount && after.LiveBytes == baseline.LiveBytes);
    assert(after.AllocCount - baseline.AllocCount == after.FreeCount - baseline.FreeCount);
    assert(!after.DoubleFreeCount && !after.InvalidFreeCount && !after.UseAfterFreeCount);
    assert(xrtMemDebugVisitLive(live,NULL));
    assert(reads == 4 && observations == 3);
    printf("actual TCC read binding: 4 read boundaries, 5 native binding vectors, foreign refusal, exact tool-only heap baseline\n");
    return 0;
}
