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
        "const xfutureproducerownershipv1* (*volatile j)(void)=xrtTlsStreamFutureProducerPolicyV1Get;\n"
        "const xcancelwatchownershipv1* (*volatile k)(void)=xrtTlsStreamFutureCancelPolicyV1Get;\n"
        "const xfuturepayloadownershipv1* (*volatile l)(void)=xrtTlsStreamFutureBytesPayloadPolicyV1Get;\n"
        "const xrtownershipquiescencev1* (*volatile m)(void)=xrtTlsStreamFutureOwnershipQuiescenceV1Get;\n"
        "const xrtownershipadapterv1* (*volatile n)(xrtownershipref,const xrtownershippreparationv1**)=xrtTlsStreamFutureOwnershipAdapterV1;\n"
        "xtlslistener* (*volatile la)(xnetengine*,const xtlslistenerconfig*,ptr,const xtlslistenerownershipv1*)=xrtTlsListenerStartOwnedV1;\n"
        "xrtownershipref (*volatile lb)(const xtlslistener*)=xrtTlsListenerOwnership;\n"
        "const xnetruntimeownershipv1* (*volatile lc)(void)=xrtTlsListenerRuntimeOwnershipV1Get;\n"
        "const xnetlistenereventsownershipv1* (*volatile ld)(void)=xrtTlsListenerTransportOwnershipV1Get;\n"
        "const xtlsstreamownershipv1* (*volatile le)(void)=xrtTlsListenerStreamOwnershipV1Get;\n"
        "const xtlsstreamownershipv1* (*volatile lf)(void)=xrtTlsListenerStreamPushOwnershipV1Get;\n"
        "const xrtownershipquiescencev1* (*volatile lg)(void)=xrtTlsListenerOwnershipQuiescenceV1Get;\n"
        "const xrtownershipadapterv1* (*volatile lh)(xrtownershipref,const xtlslistenerownershipv1* const*,size_t,const xrtownershippreparationv1**)=xrtTlsListenerOwnershipAdapterV1;\n"
        "const xrtownershipquiescencev1* (*volatile li)(void)=xrtTlsListenerStreamRecordOwnershipQuiescenceV1Get;\n"
        "const xrtownershipadapterv1* (*volatile lj)(xrtownershipref,const xrtownershippreparationv1**)=xrtTlsListenerStreamRecordOwnershipAdapterV1;\n"
        "const xfutureproducerownershipv1* (*volatile lk)(void)=xrtTlsListenerFutureProducerPolicyV1Get;\n"
        "const xcancelwatchownershipv1* (*volatile ll)(void)=xrtTlsListenerFutureCancelPolicyV1Get;\n"
        "const xfuturepayloadownershipv1* (*volatile lm)(void)=xrtTlsListenerFutureStreamPayloadPolicyV1Get;\n"
        "const xrtownershipquiescencev1* (*volatile ln)(void)=xrtTlsListenerFutureOwnershipQuiescenceV1Get;\n"
        "const xrtownershipadapterv1* (*volatile lo)(xrtownershipref,const xrtownershippreparationv1**)=xrtTlsListenerFutureOwnershipAdapterV1;\n"
        "xtlsdial* (*volatile da)(xnetengine*,xnetresolver*,cstr,uint16,const xtlsclientconfig*,const xtlsdialconfig*,ptr,const xtlsdialownershipv1*)=xrtTlsDialOwnedV1;\n"
        "xrtownershipref (*volatile db)(const xtlsdial*)=xrtTlsDialOwnership;\n"
        "const xnetruntimeownershipv1* (*volatile dc)(void)=xrtTlsDialRuntimeOwnershipV1Get;\n"
        "const xnettimerownershipv1* (*volatile dd)(void)=xrtTlsDialTimerOwnershipV1Get;\n"
        "const xnetdialownershipv1* (*volatile de)(void)=xrtTlsDialTransportOwnershipV1Get;\n"
        "const xtlsstreamownershipv1* (*volatile df)(void)=xrtTlsDialStreamOwnershipV1Get;\n"
        "const xtlsstreamownershipv1* (*volatile dg)(void)=xrtTlsDialStreamPushOwnershipV1Get;\n"
        "const xrtownershipquiescencev1* (*volatile dh)(void)=xrtTlsDialOwnershipQuiescenceV1Get;\n"
        "const xrtownershipadapterv1* (*volatile di)(xrtownershipref,const xtlsdialownershipv1* const*,size_t,const xrtownershippreparationv1**)=xrtTlsDialOwnershipAdapterV1;\n"
        "return a&&b&&c&&d&&e&&f&&g&&h&&i&&j&&k&&l&&m&&n&&la&&lb&&lc&&ld&&le&&lf&&lg&&lh&&li&&lj&&lk&&ll&&lm&&ln&&lo&&da&&db&&dc&&dd&&de&&df&&dg&&dh&&di&&dc()&&dd()&&de()&&df()&&dg()&&dh()&&f()&&g()&&h()&&j()&&k()&&l()&&m()&&lc()&&ld()&&le()&&lf()&&lg()&&li()&&lk()&&ll()&&lm()&&ln()?0:7;\n}\n";
    int ok=xlRunCSourceString(context,source,0,NULL);
    if(!ok)fprintf(stderr,"typed TLS ownership execution failed: %s\n",xlGetLastError(context));
    assert(ok && xlGetLastExitCode(context)==0);xlDestroyContext(context);
    XL_RuntimeDrainResult drain=xlRuntimeDrainRetiringChecked(-1);assert(drain.ready && drain.ok);
    puts("TLS Dial ownership symbols: thirty-eight exact SDK prototypes, actual TCC bindings, module run and checked drain");return 0;
}
