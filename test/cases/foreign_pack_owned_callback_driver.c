/* Public-API driver shared by both OS gates. All compiler objects come from
 * the same verified source cohort; this is not a shipping compiler substitute. */
#include "xlc.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    if(argc!=9)return 2;
    char temporary[8192],header[8192];
    int n=snprintf(temporary,sizeof(temporary),"%s.temps",argv[4]);
    int h=snprintf(header,sizeof(header),"%s.h",argv[4]);
    if(n<0 || (size_t)n>=sizeof(temporary) || h<0 || (size_t)h>=sizeof(header))return 3;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=".";options.tempDir=temporary;options.tccDllPath=argv[5];options.tccLibPath=argv[6];
    XLC_Context* compiler=xlcCreateContext(&options);if(!compiler)return 4;
    int ok=xlcAddIncludePath(compiler,argv[7]) && xlcAddIncludePath(compiler,argv[8]) &&
        xlcAddSourceFile(compiler,argv[1]);
    if(ok && strcmp(argv[2],"-"))ok=xlcAddCSourceFile(compiler,argv[2]);
    if(ok)ok=xlcCompile(compiler);
    if(ok){
        if(!strcmp(argv[3],"run"))ok=xlcRun(compiler,0,NULL);
        else if(!strcmp(argv[3],"exe"))ok=xlcSetSaveTemps(compiler,1) && xlcEmitExe(compiler,argv[4]);
        else if(!strcmp(argv[3],"dll"))ok=xlcEmitSharedLibrary(compiler,argv[4]);
        else if(!strcmp(argv[3],"emit"))ok=xlcEmitC(compiler,argv[4]) && xlcEmitHeader(compiler,header,"PACK_GROUP_PUBLIC");
        else if(!strcmp(argv[3],"ir"))ok=xlcEmitLoweredIr(compiler,argv[4],1);
        else ok=0;
    }
    int code=ok ? xlcGetLastExitCode(compiler):1;
    if(!ok)fprintf(stderr,"pack group public %s failed: %s\n",argv[3],xlcGetLastError(compiler));
    xlcDestroyContext(compiler);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(-1);
    if(!drained.ready || !drained.ok)return 5;
    return code;
}
