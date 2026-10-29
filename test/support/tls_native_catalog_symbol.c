/* Actual source-native registration and declaration SDK, without sockets. */
#include "xl.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv)
{
    assert(argc==6);XL_ContextOptions options=xlDefaultContextOptions();
    options.workDir=argv[1];options.tccDllPath=argv[2];options.tccLibPath=argv[3];options.tempDir=argv[5];
    XL_Context* context=xlCreateContext(&options);assert(context&&xlAddIncludePath(context,argv[4]));
    const char* source=
        "#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <__xl_runtime/xl6_ownership_domains.h>\n"
        "int main(int argc,char** argv){(void)argc;(void)argv;\n"
        "const XL6_OwnershipDomainV1* (*volatile a)(void)=xl6OwnershipDomainNetDialBaseNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile b)(void)=xl6OwnershipDomainNetProxyDialBaseNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile c)(void)=xl6OwnershipDomainTlsStreamNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile d)(void)=xl6OwnershipDomainTlsStreamFutureNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile e)(void)=xl6OwnershipDomainTlsListenerNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile f)(void)=xl6OwnershipDomainTlsListenerFutureNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile g)(void)=xl6OwnershipDomainTlsDialNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile h)(void)=xl6OwnershipDomainTlsDialFutureNativeV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile i)(void)=xl6OwnershipDomainTlsDialProxyNativeV1Get;\n"
        "bool (*volatile j)(XL6_OwnershipCatalog*,const xtlsstreamownershipv1*)=xl6OwnershipCatalogAddTlsStreamEventsV1;\n"
        "bool (*volatile k)(XL6_OwnershipCatalog*,const xtlslistenerownershipv1*)=xl6OwnershipCatalogAddTlsListenerEventsV1;\n"
        "bool (*volatile l)(XL6_OwnershipCatalog*,const xtlsdialownershipv1*)=xl6OwnershipCatalogAddTlsDialDoneV1;\n"
        "if(!a||!b||!c||!d||!e||!f||!g||!h||!i||!j||!k||!l)return 3;\n"
        "const XL6_OwnershipDomainV1* domains[]={a(),b(),c(),d(),e(),f(),g(),h(),i()};\n"
        "XL6_OwnershipCatalog* catalog=xl6OwnershipCatalogCreateV1(domains,9);if(!catalog)return 4;\n"
        "const xrtownershipadapterv1* adapter=NULL;const xrtownershippreparationv1* preparation=NULL;const XL6_OwnershipFinishV1* completion=NULL;\n"
        "bool admitted=xl6OwnershipCatalogResolveV1((xrtownershipref){0},catalog,&adapter,&preparation,&completion);\n"
        "xl6OwnershipCatalogDestroyV1(catalog);return admitted||adapter||preparation||completion?5:0;\n}\n";
    int ok=xlRunCSourceString(context,source,0,NULL);
    if(!ok)fprintf(stderr,"TLS resident catalog registration failed: %s\n",xlGetLastError(context));
    assert(ok&&xlGetLastExitCode(context)==0);xlDestroyContext(context);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);assert(drain.ready&&drain.ok);
    puts("TLS native catalog: twelve exact SDK prototypes; actual TCC bindings, sealed catalog and checked drain");
    return 0;
}
