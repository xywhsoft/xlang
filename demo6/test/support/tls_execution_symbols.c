/* Declaration SDK + actual resident TCC registration; creates no sockets. */
#include "xl.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char** argv)
{
    assert(argc==6);XL_ContextOptions options=xlDefaultContextOptions();options.workDir=argv[1];options.tccDllPath=argv[2];
    options.tccLibPath=argv[3];options.tempDir=argv[5];XL_Context* context=xlCreateContext(&options);assert(context&&xlAddIncludePath(context,argv[4]));
    const char* source=
        "#include <__xl_runtime/xruntime_decl.h>\n"
        "#include <__xl_runtime/xl6_tls_transport_abi.h>\n"
        "#include <__xl_runtime/xl6_execution_tls.h>\n"
        "#include <__xl_runtime/xl6_ownership_domains.h>\n"
        "#include <__xl_runtime/xl6_native_handle.h>\n"
        "int main(int argc,char** argv){(void)argc;(void)argv;\n"
        "void* (*volatile a)(XL6_ExecutionServicesV1,void*,const void*,void*,xstrview,int64_t)=xl6TlsStreamCreateDefaultV1;\n"
        "void* (*volatile b)(XL6_ExecutionServicesV1,void*,const void*,void*,xstrview,int64_t,xstrview,int64_t,const void*,const void*,int64_t)=xl6TlsStreamCreateV1;\n"
        "void (*volatile c)(void*)=xl6TlsStreamDestroyV1;\n"
        "void (*volatile d)(void*)=xl6TlsStreamCloseV1;\n"
        "void (*volatile e)(void*)=xl6TlsStreamAbortV1;\n"
        "int64_t (*volatile f)(void*)=xl6TlsStreamStateV1;\n"
        "uint64_t (*volatile g)(void*)=xl6TlsStreamAvailableV1;\n"
        "uint64_t (*volatile h)(void*)=xl6TlsStreamPendingV1;\n"
        "void* (*volatile i)(void*)=xl6TlsStreamLocalV1;\n"
        "void* (*volatile j)(void*)=xl6TlsStreamRemoteV1;\n"
        "void* (*volatile k)(XL6_ExecutionServicesV1,void*,const void*,void*)=xl6TlsListenerCreateDefaultV1;\n"
        "void* (*volatile l)(XL6_ExecutionServicesV1,void*,const void*,void*,const void*,int64_t,int64_t)=xl6TlsListenerCreateV1;\n"
        "void (*volatile m)(void*)=xl6TlsListenerDestroyV1;\n"
        "void (*volatile n)(void*)=xl6TlsListenerCloseV1;\n"
        "int64_t (*volatile o)(void*)=xl6TlsListenerStateV1;\n"
        "void* (*volatile p)(void*)=xl6TlsListenerLocalV1;\n"
        "XL6_ExecutionResourceV1* (*volatile q)(XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const xnetaddr*,uint32_t,const struct XL6_TlsClientContextV1*,xstrview,xstrview,const xtlsstreamconfig*,const xnetstreamconfig*)=xl6ExecutionTlsConnectV1;\n"
        "XL6_ExecutionResourceV1* (*volatile r)(XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,const struct XL6_TlsServerContextV1*,const xtlslistenerconfig*)=xl6ExecutionTlsListenV1;\n"
        "XL6_ExecutionResourceV1* (*volatile s)(XL6_ExecutionSourceV1*,XL6_ExecutionResourceV1*,xtlsstream*)=xl6ExecutionTlsAdoptV1;\n"
        "const struct XL6_ExecutionFamilyV1* (*volatile t)(void)=xl6ExecutionTlsStreamFamilyV1Get;\n"
        "const struct XL6_ExecutionFamilyV1* (*volatile u)(void)=xl6ExecutionTlsListenerFamilyV1Get;\n"
        "const xtlsstreamownershipv1* (*volatile v)(void)=xl6ExecutionTlsStreamDataPolicyV1Get;\n"
        "const xtlslistenerownershipv1* (*volatile w)(void)=xl6ExecutionTlsListenerDataPolicyV1Get;\n"
        "const XL6_NativeHandlePolicyV1* (*volatile x)(void)=xl6TlsStreamHandlePolicyV1Get;\n"
        "const XL6_NativeHandlePolicyV1* (*volatile y)(void)=xl6TlsListenerHandlePolicyV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile z)(void)=xl6OwnershipDomainExecutionTlsStreamV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile aa)(void)=xl6OwnershipDomainExecutionTlsListenerV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile ab)(void)=xl6OwnershipDomainTlsStreamLanguageV1Get;\n"
        "const XL6_OwnershipDomainV1* (*volatile ac)(void)=xl6OwnershipDomainTlsListenerLanguageV1Get;\n"
        "if(!a||!b||!c||!d||!e||!f||!g||!h||!i||!j||!k||!l||!m||!n||!o||!p||!q||!r||!s||!t||!u||!v||!w||!x||!y||!z||!aa||!ab||!ac)return 3;\n"
        "if(!t()||!u()||!v()||!w()||!x()||!y()||!z()||!aa()||!ab()||!ac())return 4;\n"
        "return v()->Ops==w()->Ops&&v()->Ops==xl6ExecutionResourceQuiescenceV1Get()->Ops?0:5;\n}\n";
    int ok=xlRunCSourceString(context,source,0,NULL);if(!ok)fprintf(stderr,"TLS execution SDK failed: %s\n",xlGetLastError(context));
    assert(ok&&xlGetLastExitCode(context)==0);xlDestroyContext(context);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);assert(drain.ready&&drain.ok);
    puts("TLS execution SDK: twenty-nine exact prototypes, actual TCC bindings, shared real resource identity, checked drain");return 0;
}
