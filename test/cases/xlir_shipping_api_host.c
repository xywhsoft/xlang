/* Public API only: link actual shipping libraries, never compiler/provider
 * source objects. Run inside an isolated copy of the three real products. */
#include "xlc.h"
#include <stdio.h>
#include <string.h>

int main(int argc,char** argv)
{
    if(argc!=4)return 2;
    char temporary[8192];
    int size=snprintf(temporary,sizeof(temporary),"%s.temps",argv[3]);
    if(size<0 || (size_t)size>=sizeof(temporary))return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();options.tempDir=temporary;
    XLC_Context* compiler=xlcCreateContext(&options);if(!compiler)return 3;
    int ok=xlcAddSourceFile(compiler,argv[1]) && xlcCompile(compiler);
    if(ok){
        if(!strcmp(argv[2],"module")){
            XL_Module* module=xlcRelocateModule(compiler);
            ok=module && xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL);
            xlDestroyModule(module);
        }else if(!strcmp(argv[2],"run"))ok=xlcRun(compiler,0,NULL);
        else if(!strcmp(argv[2],"cgen"))ok=xlcEmitC(compiler,argv[3]);
        else if(!strcmp(argv[2],"typed"))ok=xlcEmitIr(compiler,argv[3],1);
        else if(!strcmp(argv[2],"lowered"))ok=xlcEmitLoweredIr(compiler,argv[3],1);
        else if(!strcmp(argv[2],"exe"))ok=xlcSetSaveTemps(compiler,1) && xlcEmitExe(compiler,argv[3]);
        else ok=0;
    }
    int code=ok ? xlcGetLastExitCode(compiler) : 1;
    if(!ok)fprintf(stderr,"shipping API %s failed: %s\n",argv[2],
        xlcGetLastError(compiler) ? xlcGetLastError(compiler) : "without diagnostic");
    xlcDestroyContext(compiler);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(-1);
    if(!drained.ready || !drained.ok){fputs("shipping API checked retirement failed\n",stderr);if(!code)code=1;}
    return code;
}
