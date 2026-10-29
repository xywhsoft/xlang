/* One real compilation per original fixture. Public API only; the optional
 * source package supplies ordinary C assertions to both JIT and standalone
 * AOT. It never changes the source under test or compiler/native providers. */
#include "xlc.h"
#include <stdio.h>
#include <string.h>

static int path(char* result, size_t capacity, const char* directory, const char* name)
{
    int size=snprintf(result,capacity,"%s/%s",directory,name);
    return size>=0 && (size_t)size<capacity;
}

static int phase(int success, XLC_Context* compiler, const char* name)
{
    if (!success) {
        const char* error=xlcGetLastError(compiler);
        fprintf(stderr,"core-host %s failed: %s\n",name,error?error:"no diagnostic");
        return 0;
    }
    printf("core-host: %s\n",name); fflush(stdout); return 1;
}

int main(int argc, char** argv)
{
    if (argc!=9) return 2;
    char temporary[8192], target[8192];
    if (!path(temporary,sizeof(temporary),argv[3],"compiler-temps")) return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=argv[1]; options.tempDir=temporary;
    options.tccDllPath=strcmp(argv[5],"-")?argv[5]:NULL;
    options.tccLibPath=strcmp(argv[6],"-")?argv[6]:NULL;
    XLC_Context* compiler=xlcCreateContext(&options);
    if (!compiler) return 3;
    int ok=phase(xlcAddIncludePath(compiler,argv[4]) && xlcAddSourceFile(compiler,argv[2]) &&
        (!strcmp(argv[7],"-") || xlcAddSourceFile(compiler,argv[7])),compiler,"load");
    if (!strcmp(argv[8],"negative")) {
        int rejected=0;
        if (ok && path(target,sizeof(target),argv[3],"rejected.c")) {
            int compiled=xlcCompile(compiler);
            rejected=(!compiled || !xlcEmitC(compiler,target)) &&
                xlcGetLastError(compiler) && *xlcGetLastError(compiler);
        }
        if (rejected) {
            fprintf(stderr,"expected rejection: %s\n",xlcGetLastError(compiler));
            rejected=path(target,sizeof(target),argv[3],"rejected.h") &&
                !xlcEmitHeader(compiler,target,"REJECTED_CORE_SOURCE");
        }
        xlcDestroyContext(compiler);
        XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(30000);
        if (rejected && drain.ready && drain.ok) { puts("core-host: checked-rejection"); return 0; }
        return 1;
    }
    if (ok) ok=phase(xlcCompile(compiler),compiler,"compile");
    if (ok) ok=path(target,sizeof(target),argv[3],"module.typed.json") &&
        phase(xlcEmitIr(compiler,target,1),compiler,"typed");
    if (ok) ok=path(target,sizeof(target),argv[3],"module.lowered.json") &&
        phase(xlcEmitLoweredIr(compiler,target,1),compiler,"lowered");
    if (ok) ok=path(target,sizeof(target),argv[3],"module.c") &&
        phase(xlcEmitC(compiler,target),compiler,"cgen");
    if (ok) ok=path(target,sizeof(target),argv[3],"module.repeat.c") &&
        phase(xlcEmitC(compiler,target),compiler,"cgen-repeat");
    if (ok) ok=path(target,sizeof(target),argv[3],"module.h") &&
        phase(xlcEmitHeader(compiler,target,"XLIR_CORE_GUARD_SOURCE"),compiler,"header");
    if (ok) ok=path(target,sizeof(target),argv[3],"program.exe") &&
        phase(xlcSetSaveTemps(compiler,1) && xlcEmitExe(compiler,target),compiler,"aot-build");
    if (ok) ok=phase(xlcRun(compiler,0,NULL) && !xlcGetLastExitCode(compiler),compiler,"jit");
    if (ok) {
        XL_Module* module=xlcRelocateModule(compiler);
        ok=phase(module && xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL) &&
            !xlGetLastExitCode(xlcRuntimeContext(compiler)),compiler,"module");
        xlDestroyModule(module);
    }
    xlcDestroyContext(compiler);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(30000);
    if (!drain.ready || !drain.ok) { fputs("core-host checked retirement failed\n",stderr); ok=0; }
    if (ok) puts("core-host: checked-retirement");
    return ok?0:1;
}
