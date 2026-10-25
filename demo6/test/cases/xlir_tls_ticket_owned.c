#include "include/xlc.h"
#include "src/runtime/xl6_tls_session_internal.h"
#include "src/runtime/xl6_tls_ticket_result.h"
#include "src/runtime/xl6_array_value_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef xvalue*(*Unary)(xvalue*);
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static xvalue* call(Unary function,xvalue* argument)
{
    xl6_foreign_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));
    xvalue* result=function(argument);xl6_foreign_error_result status=xl6ForeignCallEnd(&scope);
    assert(status.ok&&result);xrtErrorFree(status.error);return result;
}
static void* native(xvalue* object,const XL6_NativeHandlePolicyV1* policy)
{
    xvalue* field=xrtValueObjectGet(object,(xstrview){"handle",6});void *raw=NULL,*user=NULL;const xvaluehandleops* ops=NULL;
    assert(field&&xrtValueGetHandle(field,&raw,&ops,&user)&&raw&&!user&&ops==policy->Ops);return raw;
}
static size_t count(XL6_TlsSessionV1* client)
{
    XL6_TlsSessionScopeV1 scope={0};assert(xl6TlsSessionEnterInternalV1(client,true,&scope));
    size_t result=xrtTlsClientResumeCount(client->native);xl6TlsSessionLeaveInternalV1(&scope);return result;
}
static void discard(XL6_TlsSessionV1* client)
{
    XL6_TlsSessionScopeV1 scope={0};assert(xl6TlsSessionEnterInternalV1(client,true,&scope));
    xtlsresume* value=xrtTlsClientTakeResume(client->native);assert(value&&!xrtGetError());
    xl6TlsSessionLeaveInternalV1(&scope);xrtTlsResumeRelease(value);
}
int main(int argc,char** argv)
{
    assert(argc==6&&xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    XLC_ContextOptions options=xlcDefaultContextOptions();options.workDir=".";options.tccDllPath=argv[3];options.tccLibPath=argv[4];options.tempDir=argv[5];
    XLC_Context* compiler=xlcCreateContext(&options);assert(compiler&&xlcAddIncludePath(compiler,argv[2])&&xlcAddSourceFile(compiler,argv[1]));
    XL_Module* module=xlcRelocateModule(compiler);
    if(!module)fprintf(stderr,"%s\n",xlcGetLastError(compiler));
    assert(module&&xlRunModuleMain(xlcRuntimeContext(compiler),module,NULL));
    xvalue*(*create)(void)=(xvalue*(*)(void))xlModuleGetSymbol(module,"xl6_export_tls_ticket_pair");
    Unary seed=(Unary)xlModuleGetSymbol(module,"xl6_export_tls_ticket_seed");
    Unary pop[]={(Unary)xlModuleGetSymbol(module,"xl6_export_tls_ticket_pop"),(Unary)xlModuleGetSymbol(module,"xl6_export_tls_ticket_pop_dynamic")};
    assert(create&&seed&&pop[0]&&pop[1]);
    xl6_foreign_error_scope init={0};assert(xl6ForeignCallBegin(&init));xvalue* pair=create();
    xl6_foreign_error_result created=xl6ForeignCallEnd(&init);assert(created.ok&&pair);xrtErrorFree(created.error);
    xvalue* clientObject=xl6ArrayValueReadCopyV1(pair,0);xvalue* serverObject=xl6ArrayValueReadCopyV1(pair,1);assert(clientObject&&serverObject);
    XL6_TlsSessionV1* client=native(clientObject,xl6TlsSessionHandlePolicyV1Get());
    XL6_TlsSessionV1* server=native(serverObject,xl6TlsSessionHandlePolicyV1Get());
    for(size_t i=0;i<4;++i){xrtValueRelease(call(seed,pair));xrtValueRelease(call(pop[i%2],clientObject));}
    size_t failures=0,successes=0;
    for(size_t mode=0;mode<2;++mode)for(size_t point=0;point<128;++point){
        assert(count(client)==0);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        xrtValueRelease(call(seed,pair));assert(count(client)==1);
        xl6_foreign_error_scope scope={0};assert(xl6ForeignCallBegin(&scope));assert(xrtMemDebugFailAfter(point));
        xvalue* result=pop[mode](clientObject);xrtMemDebugFailClear();xl6_foreign_error_result status=xl6ForeignCallEnd(&scope);
        if(!status.ok){++failures;assert(!result&&count(client)==1);}
        else{
            ++successes;assert(result&&count(client)==0);
            XL6_TlsTicketResultV1* record=native(result,xl6TlsTicketResultHandlePolicyV1Get());
            xtlsresumeinfo info={0};assert(record->status==XTLS_OK&&record->resume&&xrtTlsResumeInfo(record->resume,&info));
            assert(info.Ticket.Size==8&&!memcmp(info.Ticket.Data,"ticket\0\xff",8));
        }
        xrtErrorFree(status.error);xrtClearError();xrtValueRelease(result);
        if(count(client))discard(client);
        balanced(&before);
    }
    assert(failures&&successes);
    xrtValueRelease(call(seed,pair));xvalue* result=call(pop[0],clientObject);
    xlDestroyModule(module);xlcDestroyContext(compiler);assert(!xlRuntimeDrainRetiringChecked(0).ready);
    XL6_TlsTicketResultV1* record=native(result,xl6TlsTicketResultHandlePolicyV1Get());
    assert(xl6TlsTicketResultStatusV1(record)==XTLS_OK);void* credential=xl6TlsTicketResultResumeV1(record);assert(credential);xrtTlsResumeRelease(credential);
    xl6TlsSessionDisposeV1(client);xl6TlsSessionDisposeV1(server);
    xrtValueRelease(clientObject);xrtValueRelease(serverObject);xrtValueRelease(pair);
    assert(!xlRuntimeDrainRetiringChecked(0).ready);xrtValueRelease(result);
    XL_RuntimeDrainResult retired=xlRuntimeDrainRetiringChecked(-1);assert(retired.ready&&retired.ok&&!xrtGetError());
    printf("TLS ticket ownership: real typed/dynamic exports, 256 OOM prefixes never lose a queued ticket, %zu failures/%zu successes, Close-safe result and balanced memory: ok\n",failures,successes);return 0;
}
