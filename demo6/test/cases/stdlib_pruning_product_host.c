/* Public product emission only; ordinary TCC/VFS and no source fallback. */
#include "xlc.h"
#include <stdio.h>
int main(int argc,char** argv)
{
    if(argc<5)return 2;
    XLC_ContextOptions options=xlcDefaultContextOptions();
    options.workDir=".";options.tempDir=argv[3];options.tccDllPath=argv[1];options.saveTemps=1;
    XLC_Context* compiler=xlcCreateContext(&options);if(!compiler)return 3;
    int ok=xlcAddSourceFile(compiler,argv[2]);
    for(int i=5;ok&&i<argc;++i)ok=xlcAddCSourceFile(compiler,argv[i]);
    if(ok)ok=xlcCompile(compiler)&&xlcEmitExe(compiler,argv[4]);
    if(!ok)fprintf(stderr,"pruning product emission failed: %s\n",xlcGetLastError(compiler));
    xlcDestroyContext(compiler);return ok?0:1;
}
