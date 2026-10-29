/* Public compiler product only. Its TCC/VFS resolve every generated/native
 * include; this host adds no source/header/library fallback or provider. */
#include "xlc.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char** argv)
{
    if(argc<5 || (strcmp(argv[3],"run") && strcmp(argv[3],"module") && strcmp(argv[3],"check")))return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=".";options.tempDir=argv[4];options.tccDllPath=argv[1];
    XLC_Context* compiler=xlcCreateContext(&options);
    if(!compiler){fputs("product fixture context creation failed\n",stderr);return 3;}
    int ok=xlcAddSourceFile(compiler,argv[2]),code=1;
    for(int i=5;ok && i<argc;++i)ok=xlcAddCSourceFile(compiler,argv[i]);
    if(ok)ok=xlcCompile(compiler);
    if(ok && !strcmp(argv[3],"run")){
        ok=xlcRun(compiler,0,NULL);if(ok)code=xlcGetLastExitCode(compiler);
    }else if(ok && !strcmp(argv[3],"module")){
        XL_Module* module=xlcRelocateModule(compiler);ok=module!=NULL;
        if(ok){ok=xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL);if(ok)code=xlGetLastExitCode(xlcRuntimeContext(compiler));}
        if(module)xlDestroyModule(module);
    }else if(ok)code=0;
    if(!ok){
        const char* message=xlcGetLastError(compiler);
        if(!message || !*message)message=xlGetLastError(xlcRuntimeContext(compiler));
        fprintf(stderr,"product fixture %s failed: %s\n",argv[3],message && *message?message:"without diagnostic");
    }
    xlcDestroyContext(compiler);
    if(ok && !code)printf("product fixture %s complete\n",argv[3]);
    return ok?code:1;
}
