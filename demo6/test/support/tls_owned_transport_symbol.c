/* Real SDK prototype and TCC registration only; no socket is created. */
#include "xl.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv)
{
    assert(argc==6);XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir=argv[1];options.tccDllPath=argv[2];options.tccLibPath=argv[3];options.tempDir=argv[5];
    XL_Context* context=xlCreateContext(&options);assert(context && xlAddIncludePath(context,argv[4]));
    const char* source=
        "#include <__xl_runtime/xruntime_decl.h>\n"
        "int main(int argc,char** argv){(void)argc;(void)argv;\n"
        " xnetstream* (*volatile factory)(xnetengine*,const xnetaddr*,uint64,const xnetstreamconfig*,ptr,const xnetstreameventsownershipv1*)=xrtNetStreamConnectOwnedV1;\n"
        " return factory ? 0 : 7;\n}\n";
    int ok=xlRunCSourceString(context,source,0,NULL);
    if(!ok)fprintf(stderr,"typed native symbol execution failed: %s\n",xlGetLastError(context));
    assert(ok && xlGetLastExitCode(context)==0);xlDestroyContext(context);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);assert(drain.ready && drain.ok);
    puts("TLS owned transport symbol: genuine SDK type, source-native TCC binding, C module run and checked drain");return 0;
}
