/* Exact declaration-SDK types and actual source-native TCC registrations.
 * This runs no transport operation and creates no socket. */
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
        "xtlsstream* (*volatile a)(xnetengine*,const xnetaddr*,uint64,const xnetstreamconfig*,const xtlsclientconfig*,const xtlsstreamconfig*,ptr,const xtlsstreamownershipv1*)=xrtTlsStreamConnectOwnedV1;\n"
        "bool (*volatile b)(xnetstream*,xtlssession*,const xtlsstreamconfig*,ptr,const xtlsstreamownershipv1*,xtlsstream**)=xrtTlsStreamAttachOwnedV1;\n"
        "bool (*volatile c)(xnetstream*,const xtlsclientconfig*,const xtlsstreamconfig*,ptr,const xtlsstreamownershipv1*,xtlsstream**)=xrtTlsStreamClientOwnedV1;\n"
        "bool (*volatile d)(xnetstream*,const xtlsserverconfig*,const xtlsstreamconfig*,ptr,const xtlsstreamownershipv1*,xtlsstream**)=xrtTlsStreamAcceptOwnedV1;\n"
        "xrtownershipref (*volatile e)(const xtlsstream*)=xrtTlsStreamOwnership;\n"
        "const xnetruntimeownershipv1* (*volatile f)(void)=xrtTlsStreamRuntimeOwnershipV1Get;\n"
        "const xnetstreameventsownershipv1* (*volatile g)(void)=xrtTlsStreamTransportOwnershipV1Get;\n"
        "const xrtownershipquiescencev1* (*volatile h)(void)=xrtTlsStreamOwnershipQuiescenceV1Get;\n"
        "const xrtownershipadapterv1* (*volatile i)(xrtownershipref,const xtlsstreamownershipv1* const*,size_t,const xrtownershippreparationv1**)=xrtTlsStreamOwnershipAdapterV1;\n"
        "return a&&b&&c&&d&&e&&f&&g&&h&&i&&f()&&g()&&h()?0:7;\n}\n";
    int ok=xlRunCSourceString(context,source,0,NULL);
    if(!ok)fprintf(stderr,"typed TLS ownership execution failed: %s\n",xlGetLastError(context));
    assert(ok && xlGetLastExitCode(context)==0);xlDestroyContext(context);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);assert(drain.ready && drain.ok);
    puts("TLS stream ownership symbols: nine exact SDK prototypes, actual TCC bindings, module run and checked drain");return 0;
}
