/* Real TCC language callbacks, fixed valid in-memory TLS, and Close/OOM.
 * No sockets or security test vectors. All native config borrows are scoped. */
#include "include/xlc.h"
#include "src/runtime/xl6_tls_session_internal.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const uint8 wireProtocols[]={0,7,2,'h','2',3,'x',0,255};
static const xtlsserverresumerequest request={
    {(const uint8*)"localhost",9},{wireProtocols,sizeof(wireProtocols)},
    {(const uint8*)"ticket\0\xff",8},UINT32_MAX};
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static void* payload(xvalue* object,const XL6_NativeHandlePolicyV1* expected)
{
    xvalue* field=xrtValueObjectGet(object,(xstrview){"handle",6});void* result=NULL;
    const xvaluehandleops* ops=NULL;void* user=NULL;
    assert(field&&xrtValueGetHandle(field,&result,&ops,&user)&&result&&ops==expected->Ops&&!user);return result;
}
/* The fixture intentionally saves its first owned request. Initialize that
 * persistent capture before measuring replacement/rollback allocations. */
static void warm(const XL6_TlsServerContextV1* context,unsigned mode)
{
    xl6_error_scope scope={0};assert(xl6ErrorScopeBegin(&scope));xtlsresume* output=NULL;
    bool ok=context->resumeLookup.ops->Lookup(context->resumeLookup.data,&request,&output);
    xl6_error_result result=xl6ErrorScopeEnd(&scope);
    assert(ok==(mode!=2)&&result.ok==(mode!=2)&&(output!=NULL)==(mode==0));
    xrtClearError();xrtErrorFree(result.error);xrtTlsResumeRelease(output);
}
static void invoke(const XL6_TlsServerContextV1* context,unsigned mode,bool oom,size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    if(oom)assert(xrtMemDebugFailAfter(point));
    xl6_error_scope scope={0};bool begun=xl6ErrorScopeBegin(&scope);xtlsresume* output=NULL;
    bool ok=begun&&context->resumeLookup.ops->Lookup(context->resumeLookup.data,&request,&output);
    xl6_error_result result=begun?xl6ErrorScopeEnd(&scope):(xl6_error_result){false,xrtTakeError()};
    bool hit=oom&&xrtMemDebugFailTriggered();if(oom)xrtMemDebugFailClear();
    if(hit)assert(!ok&&!result.ok); /* Allocation failure is never a cache miss. */
    if(!hit){assert(ok==(mode!=2)&&result.ok==(mode!=2));assert((output!=NULL)==(mode==0));}
    if(output){
        xtlsresumeinfo info;assert(xrtTlsResumeInfo(output,&info));
        assert(info.Ticket.Size==8&&!memcmp(info.Ticket.Data,"ticket\0\xff",8));
        assert(info.Secret.Size==32&&!memcmp(info.Secret.Data,"01234567890123456789012345678901",32));
        assert(info.Protocol.Size==2&&!memcmp(info.Protocol.Data,"h2",2));
    }
    xrtClearError();xrtErrorFree(result.error);xrtTlsResumeRelease(output);
    assert(!xrtGetError());balanced(&before);
}
static void move(xtlssession* from,xtlssession* to)
{
    xnetspan span={0};
    while(xrtTlsSessionSendSize(from)){
        assert(xrtTlsSessionSendFront(from,&span));size_t size=span.Size<317?span.Size:317;
        assert(size&&xrtTlsSessionFeed(to,span.Data,size)==XTLS_OK&&xrtTlsSessionSendConsume(from,size));
    }
}
static void handshake(const XL6_TlsServerContextV1* serverContext,const XL6_TlsClientContextV1* clientContext,bool resumed)
{
    xtlsresumeconfig credential={0};credential.Version=XTLS_VERSION_13;credential.Cipher=XTLS_AES_128_GCM_SHA256;
    credential.Ticket=request.Ticket;credential.Secret=(xbytesview){(const uint8*)"01234567890123456789012345678901",32};
    credential.ServerName=(xstrview){"localhost",9};credential.Protocol=(xbytesview){(const uint8*)"h2",2};
    credential.Lifetime=120;credential.AgeAdd=1;credential.IssuedAt=xrtNow();
    xtlsresume* seed=xrtTlsResumeCreate(&credential);assert(seed);
    const xstrview protocols[]={{"h2",2},{"x\0\xff",3}};
    xtlsclientconfig config;xrtTlsClientConfigInit(&config);config.Context=clientContext->parameters;
    config.Verifier=clientContext->verifier.value;config.ServerName=config.VerifyName=credential.ServerName;
    config.Protocols=protocols;config.ProtocolCount=2;config.Resume=seed;
    xtlssession* client=xrtTlsClientCreate(&config,NULL);xrtTlsResumeRelease(seed);assert(client);
    XL6_TlsSessionV1* server=xl6TlsSessionServerCreateV1((void*)serverContext);assert(server);
    for(unsigned i=0;i<2048;++i){
        xtlsresult a=xrtTlsClientDrive(client);int64_t b=xl6TlsSessionDriveV1(server);
        if(a==XTLS_ERROR||b==XTLS_ERROR){fprintf(stderr,"handshake: %s\n",xrtGetError()?xrtErrorMessage(xrtGetError()):"without diagnostic");assert(0);}
        XL6_TlsSessionScopeV1 scope={0};assert(xl6TlsSessionEnterInternalV1(server,true,&scope));
        move(client,server->native);move(server->native,client);
        bool ready=xrtTlsSessionState(client)==XTLS_STATE_READY&&xrtTlsSessionState(server->native)==XTLS_STATE_READY;
        if(ready){
            assert(xrtTlsClientResumed(client)==resumed&&xrtTlsServerResumed(server->native)==resumed);
            assert(!server->selector.data&&!server->resumeLookup.data);xl6TlsSessionLeaveInternalV1(&scope);break;
        }
        xl6TlsSessionLeaveInternalV1(&scope);assert(i+1<2048);
    }
    xl6TlsSessionDestroyV1(server);xrtTlsSessionDestroy(client);assert(!xrtGetError());
}
int main(int argc,char** argv)
{
    assert(argc==6);assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";
    options.tccDllPath=argv[3];options.tccLibPath=argv[4];options.tempDir=argv[5];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler&&xlcAddIncludePath(compiler,argv[2])&&xlcAddSourceFile(compiler,argv[1]));
    XL_Module* module=xlcRelocateModule(compiler);
    if(!module)fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    assert(module&&xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL));
    xvalue*(*factory)(int64_t)=(xvalue*(*)(int64_t))xlModuleGetSymbol(module,"xl6_export_tls_resume_lookup_context");
    xvalue*(*clientFactory)(void)=(xvalue*(*)(void))xlModuleGetSymbol(module,"xl6_export_tls_resume_lookup_client");assert(factory&&clientFactory);
    xvalue* owners[3]={0};XL6_TlsServerContextV1* contexts[3]={0};
    for(unsigned mode=0;mode<3;++mode){
        xl6_foreign_error_scope call={0};assert(xl6ForeignCallBegin(&call));owners[mode]=factory(mode);
        xl6_foreign_error_result result=xl6ForeignCallEnd(&call);assert(result.ok&&owners[mode]);xrtErrorFree(result.error);
        contexts[mode]=payload(owners[mode],xl6TlsServerContextHandlePolicyV1Get());warm(contexts[mode],mode);invoke(contexts[mode],mode,false,0);
    }
    xl6_foreign_error_scope call={0};assert(xl6ForeignCallBegin(&call));xvalue* clientOwner=clientFactory();
    xl6_foreign_error_result result=xl6ForeignCallEnd(&call);assert(result.ok&&clientOwner);xrtErrorFree(result.error);
    XL6_TlsClientContextV1* clientContext=payload(clientOwner,xl6TlsClientContextHandlePolicyV1Get());
    handshake(contexts[0],clientContext,true);handshake(contexts[1],clientContext,false);xrtValueRelease(clientOwner);
    for(size_t point=0;point<128;++point)invoke(contexts[0],0,true,point);
    xlDestroyModule(module);xlcDestroyContext(compiler);
    assert(!xlRuntimeDrainRetiringChecked(0).ready);
    for(unsigned mode=0;mode<3;++mode)invoke(contexts[mode],mode,false,0);
    for(size_t point=0;point<128;++point)invoke(contexts[0],0,true,point);
    for(unsigned mode=0;mode<3;++mode)xrtValueRelease(owners[mode]);
    XL_RuntimeDrainResult drained=xlRuntimeDrainRetiringChecked(-1);assert(drained.ready&&drained.ok&&!xrtGetError());
    puts("TLS resume lookup actual language: owned temporary result, hit/miss handshakes, checked throw, callback after Close, 256 OOM prefixes: ok");return 0;
}
