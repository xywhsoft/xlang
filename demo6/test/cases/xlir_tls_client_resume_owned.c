#include "include/xlc.h"
#include "src/runtime/xl6_tls_role_context_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static void roundTrip(XL6_TlsClientContextV1* plain,xtlsresume* credential,size_t point,unsigned operation)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    XL6_TlsClientContextV1* bound=xl6TlsClientContextWithResumeV1(plain,credential);assert(bound);
    assert(bound->resume.value==credential&&xl6TlsClientContextHasResumeV1(bound));
    assert(xrtMemDebugFailAfter(point));void* result=NULL;
    if(operation==0)result=xl6TlsClientContextWithResumeV1(plain,credential);
    else if(operation==1)result=xl6TlsClientContextWithParametersV1(bound,bound->parameters);
    else if(operation==2)result=xl6TlsClientContextWithoutResumeV1(bound);
    else result=xl6TlsClientContextWithVerifierV1(bound,&bound->verifier);
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();assert(hit?!result:result!=NULL);xrtClearError();
    if(result){
        bool resumed=operation<2;assert(xl6TlsClientContextHasResumeV1(result)==resumed);
        void* value=xl6TlsClientContextResumeV1(result);assert(resumed?value==credential:value==NULL);
        xrtTlsResumeRelease(value);xl6TlsClientContextDestroyV1(result);
    }
    assert(xl6TlsClientContextHasResumeV1(bound)&&!xl6TlsClientContextHasResumeV1(plain));
    xl6TlsClientContextDestroyV1(bound);balanced(&before);
}
int main(int argc,char** argv)
{
    assert(argc==6&&xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";options.tccDllPath=argv[3];options.tccLibPath=argv[4];options.tempDir=argv[5];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler&&xlcAddIncludePath(compiler,argv[2])&&xlcAddSourceFile(compiler,argv[1]));
    XL_Module* module=xlcRelocateModule(compiler);
    if(!module)fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    assert(module&&xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL));
    xvalue*(*create)(void)=(xvalue*(*)(void))xlModuleGetSymbol(module,"xl6_export_tls_resume_lookup_client");assert(create);
    xl6_foreign_error_scope call={0};assert(xl6ForeignCallBegin(&call));xvalue* owner=create();
    xl6_foreign_error_result outcome=xl6ForeignCallEnd(&call);assert(outcome.ok&&owner);xrtErrorFree(outcome.error);
    xvalue* field=xrtValueObjectGet(owner,(xstrview){"handle",6});void* raw=NULL;const xvaluehandleops* ops=NULL;
    assert(field&&xrtValueGetHandle(field,&raw,&ops,NULL)&&raw&&ops==xl6TlsClientContextHandlePolicyV1Get()->Ops);
    XL6_TlsClientContextV1* plain=raw;
    xtlsresumeconfig config;xrtTlsResumeConfigInit(&config);config.Cipher=XTLS_AES_128_GCM_SHA256;
    config.Ticket=(xbytesview){(const uint8*)"t\0\xff",3};config.Secret=(xbytesview){(const uint8*)"01234567890123456789012345678901",32};
    config.ServerName=(xstrview){"localhost",9};config.Lifetime=120;xtlsresume* credential=xrtTlsResumeCreate(&config);assert(credential);
    xlDestroyModule(module);xlcDestroyContext(compiler);assert(!xlRuntimeDrainRetiringChecked(0).ready);
    for(unsigned operation=0;operation<4;++operation)for(size_t point=0;point<64;++point)roundTrip(plain,credential,point,operation);
    xrtTlsResumeRelease(credential);xrtValueRelease(owner);
    XL_RuntimeDrainResult result=xlRuntimeDrainRetiringChecked(-1);assert(result.ready&&result.ok&&!xrtGetError());
    puts("TLS client resume ownership: real language verifier, clone/clear/trust semantics after Close, 256 OOM prefixes balanced: ok");return 0;
}
