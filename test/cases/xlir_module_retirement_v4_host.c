#define XL_STATIC_EMBED
#define XL_ENABLE_EXE_COMPILER 1
#include "src/api/xl_api.c"
#include "xlir_module_retirement_v4_hooks.h"
#include <assert.h>
#include <stdio.h>
static void collected(void)
{ xlApiLock();xlCollectRetiringModulesUnlocked();xlApiUnlock(); }
static void memory_state(void* address,DWORD state)
{ MEMORY_BASIC_INFORMATION memory;assert(VirtualQuery(address,&memory,sizeof(memory))==sizeof(memory) && memory.State==state); }
static XL_Module* compile(XL_Context* context,unsigned invalid)
{
    char source[256];snprintf(source,sizeof(source),"#define CONTRACT_BAD %u\n#include \"test/cases/xlir_module_retirement_v4_artifact.c\"\n",invalid);
    XL_Module* result=xlCompileCSourceStringToModule(context,source);
    if(!result && !invalid)fprintf(stderr,"V4 fixture compile: %s\n",xlGetLastError(context));
    return result;
}
static void cycle(XL_Context* context,unsigned selected)
{
    XL_Module* module=compile(context,0);assert(module && module->retirementStep);
    RetirementHooks hooks={0};hooks.status=XL6_MODULE_RETIRE_BUSY;
    hooks.ownLease=selected==3 || selected==6;hooks.failInit=selected==2;
    void (*bind)(RetirementHooks*);void* symbol=xlModuleGetSymbol(module,"retirement_bind_hooks");
    memcpy(&bind,&symbol,sizeof(bind));assert(bind);bind(&hooks);
    void* entry=xlModuleGetSymbol(module,"xl_script_main_checked");assert(entry);memory_state(entry,MEM_COMMIT);
    if(selected==1){
        assert(xlRunModuleMain(context,module,NULL) && xlGetLastExitCode(context)==42);
        assert(module->activation==XL_MODULE_DEACTIVATION_PENDING && !module->retirementFailed);
    } else if(selected==2){
        assert(!xlActivateModule(context,module,NULL));
        assert(module->activation==XL_MODULE_DEACTIVATION_PENDING && hooks.finalizers==1);
        assert(!xlActivateModule(context,module,NULL) && hooks.initializations==1);
    } else assert(xlActivateModule(context,module,NULL));
    if(selected==0){
        assert(!xlDeactivateModule(context,module) && module->activation==XL_MODULE_DEACTIVATION_PENDING);
        assert(!module->retirementFailed && hooks.finalizers==1 && !xlActivateModule(context,module,NULL));
        hooks.status=XL6_MODULE_RETIRE_READY;assert(xlDeactivateModule(context,module));
        assert(hooks.finalizers==1 && hooks.completions==1 && xlActivateModule(context,module,NULL));
        assert(hooks.initializations==2);hooks.status=XL6_MODULE_RETIRE_BUSY;
    }
    if(selected==4 || selected==6)hooks.status=XL6_MODULE_RETIRE_FAILED;
    if(selected==5)hooks.status=99;
    xrtClearError();xlClearLastError(context);
    xlDestroyModule(module); /* Sole handle is consumed. The registry now owns it. */
    assert(xl__retiringModules==module);memory_state(entry,MEM_COMMIT);
    if(selected>=4){assert(module->retirementFailed);if(selected!=5)assert(!module->retirementError);}
    else assert(!module->retirementFailed);
    if(hooks.ownLease)assert(module->ownershipPlan && module->ownershipPhase==3 && module->snapshotRefs && module->codeLeases);
    for(unsigned n=0;n<8;++n){
        collected();memory_state(entry,MEM_COMMIT);assert(hooks.legacyCalls==0);
        assert(hooks.finalizers==(selected==0?2u:1u));
    }
    xrtClearError();hooks.status=XL6_MODULE_RETIRE_READY;collected();
    memory_state(entry,MEM_FREE);assert(!xl__retiringModules && !hooks.legacyCalls);
    assert(hooks.completions==(selected==0?2u:1u));
    xrtClearError();xlClearLastError(context);
}
int main(int argc,char** argv)
{
    assert(argc==2 && xrtMemDebugEnable(true));
    XL_ContextOptions options=xlDefaultContextOptions();char root[2048],runtime[2300],libs[2300];
    assert(GetCurrentDirectoryA(sizeof(root),root));
    snprintf(runtime,sizeof(runtime),"%s/release/xl.dll",root);snprintf(libs,sizeof(libs),"%s/res/tcc/win64/x86_64",root);
    options.workDir=root;options.tccDllPath=runtime;options.tccLibPath=libs;
    XL_Context* context=xlCreateContext(&options);assert(context);
    assert(xlAddIncludePath(context,argv[1]) && xlAddIncludePath(context,root));
    /* Warm the real compiler provider before comparing module-owned memory. */
    for(unsigned n=0;n<2;++n){XL_Module* warm=compile(context,1);assert(!warm);xlClearLastError(context);xrtClearError();}
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    for(unsigned n=0;n<10;++n){
        for(unsigned selected=0;selected<7;++selected)cycle(context,selected);
        for(unsigned invalid=1;invalid<=7;++invalid){
            assert(!compile(context,invalid) && strstr(xlGetLastError(context),"invalid module retirement V4 contract"));
            xlClearLastError(context);xrtClearError();
        }
    }
    xrtMemDebugSnapshot(&after);
    if(before.LiveCount!=after.LiveCount || before.LiveBytes!=after.LiveBytes)
        fprintf(stderr,"V4 memory delta: %lld objects, %lld bytes\n",(long long)after.LiveCount-(long long)before.LiveCount,(long long)after.LiveBytes-(long long)before.LiveBytes);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    xlDestroyContext(context);
    puts("Module retirement V4: 70 real TCC relocations/unmaps, 70 malformed contracts refused, 20 pinned graph plans, one-shot/failed-init/BUSY/diagnostic-free-failure/invalid-status/reinit; no bool fallback, one finalizer per activation, zero live delta");
    return 0;
}
