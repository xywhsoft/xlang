#define main legacyEditCallMain
#include "edit_call_test.c"
#undef main
static int refuseSend,forbidEncoding;static unsigned blockedSends,encodedOnOwner;static DWORD owner;
int __real_xideLanguageBatchSendBound(xide_language_client*,xide_language_batch*,uint64_t,xatomic32*);
int __wrap_xideLanguageBatchSendBound(xide_language_client* c,xide_language_batch* b,uint64_t peer,xatomic32* sent)
{if(refuseSend){refuseSend=0;++blockedSends;return 0;}return __real_xideLanguageBatchSendBound(c,b,peer,sent);}
char* __real_xideLspFrame(const xvalue*,size_t*,xide_model_error*);
char* __wrap_xideLspFrame(const xvalue* v,size_t* bytes,xide_model_error* e)
{if(forbidEncoding&&GetCurrentThreadId()==owner){++encodedOnOwner;return NULL;}return __real_xideLspFrame(v,bytes,e);}
typedef struct cancel_peer {int requests,cancels,done,okay;int64 id;} cancel_peer;
static int cancellationPeer(const xvalue* root,void* data)
{
 cancel_peer* p=data;
 if(equal(get(root,"method"),XIDE_RENAME_CANDIDATE_METHOD)){int64 id=0;if(!xrtValueIs(get(root,"id"),XVALUE_INT)||!xrtValueGetInt(get(root,"id"),&id)||id!=7)return 0;p->id=id;++p->requests;return 1;}
 if(equal(get(root,"method"),"$/cancelRequest")){if(get(root,"id")||!integer(get(get(root,"params"),"id"),p->id)||p->requests!=1)return 0;++p->cancels;return 1;}
 if(!equal(get(root,"method"),"test/finish"))return 0;
 p->okay=p->requests==p->cancels&&p->requests<=1;
 xvalue* response=xrtValueObject(),*result=xrtValueObject();int okay=response&&result&&put(response,"jsonrpc",xrtValueString(xrtStrView("2.0")))&&put(response,"id",xrtValueInt(55))&&put(result,"requests",xrtValueInt(p->requests))&&put(result,"cancels",xrtValueInt(p->cancels))&&put(result,"okay",xrtValueBool(p->okay));
 if(okay){okay=put(response,"result",result);result=NULL;}size_t bytes=0;char* frame=okay?xideLspFrame(response,&bytes,NULL):NULL;xrtValueRelease(response);xrtValueRelease(result);DWORD count=0;okay=frame&&WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),frame,(DWORD)bytes,&count,NULL)&&count==bytes;free(frame);p->done=okay;return okay;
}
static int cancelPeerMain(void)
{cancel_peer p={0};xide_lsp_stream stream={0};char raw[4096];DWORD n;while(!p.done&&ReadFile(GetStdHandle(STD_INPUT_HANDLE),raw,sizeof(raw),&n,NULL)&&n)if(!xideLspStreamFeed(&stream,raw,n,cancellationPeer,&p,NULL))break;xideLspStreamClear(&stream);return p.done&&p.okay?0:1;}
static int finish(xide_language_client* c,int expected)
{xvalue* notification=xrtJsonParse(xrtStrView("{\"jsonrpc\":\"2.0\",\"method\":\"test/finish\"}"));int okay=notification&&xideLanguageClientSend(c,notification,NULL);xrtValueRelease(notification);size_t bytes=0;xvalue* result=okay?next(c,&bytes):NULL;okay=result&&integer(get(get(result,"result"),"requests"),expected)&&integer(get(get(result,"result"),"cancels"),expected);bool valid=false;okay=okay&&xrtValueGetBool(get(get(result,"result"),"okay"),&valid)&&valid;xrtValueRelease(result);return okay;}
int main(int argc,char** argv)
{
 if(argc==2&&!strcmp(argv[1],"--cancel-peer"))return cancelPeerMain();
 if(argc!=2)return 2;
 owner=GetCurrentThreadId();
 const char* args[]={"--cancel-peer"};xide_language_client_spec spec={argv[0],argv[1],args,1};xide_language_client *a=xideLanguageClientStartProcess(&spec,NULL),*b=xideLanguageClientStartProcess(&spec,NULL);ownedClients[0]=a;ownedClients[1]=b;CHECK(a&&b);
 xls_sync* sync=xlsSyncCreateMonotonic(2);char* uri=xideLspPathToUri("F:/call/main.xl",NULL);const char* body="//馃榾\r\nfunc main(...) as int\n return 0\nend\n";xls_sync_document document={uri,body,strlen(body),11,7};CHECK(sync&&uri&&xlsSyncUpdate(sync,&document,1,emit,NULL));xvalue* catalog=xrtJsonParse(xrtStrView("{\"schema\":1,\"projects\":[],\"selectedProject\":\"\",\"packagePath\":\"\"}"));xide_edit_request_model model={11,7,1,0,0,"F:/call/main.xl","xlang"};xide_edit_request* request=makeRequest(sync,catalog,&model);CHECK(request);
 xide_edit_call* call=xideEditCallCreate(request,xideLanguageClientIdentity(a),uri,1,5,xrtStrView("renamed"),NULL,NULL,NULL);CHECK(call&&xideEditCallSend(call,a));
 forbidEncoding=1;CHECK(!xideEditCallCancelSend(call,b)&&xideEditCallState(call)==XIDE_EDIT_CALL_CANCELLED&&xideEditRequestInvalidated(request));refuseSend=1;CHECK(!xideEditCallCancelSend(call,a)&&blockedSends==1);CHECK(xideEditCallCancelSend(call,a)&&xideEditCallCancelSend(call,a)&&!xideEditCallCancelSend(call,b)&&!encodedOnOwner);forbidEncoding=0;CHECK(finish(a,1)&&finish(b,0));xideEditCallRelease(call);xideEditRequestRelease(request);stopClient(a);stopClient(b);
 a=xideLanguageClientStartProcess(&spec,NULL);ownedClients[0]=a;CHECK(a);request=makeRequest(sync,catalog,&model);CHECK(request);call=xideEditCallCreate(request,xideLanguageClientIdentity(a),uri,1,5,xrtStrView("unsent"),NULL,NULL,NULL);CHECK(call&&xideEditCallCancelSend(call,a)&&xideEditCallCancelSend(call,a)&&!xideEditCallSend(call,a)&&finish(a,0));xideEditCallRelease(call);xideEditRequestRelease(request);stopClient(a);xlsSyncDestroy(sync);xrtValueRelease(catalog);free(uri);printf("PASS edit cancellation: %u checks; original-connection typed ID and order, exactly once, foreign connection isolation, unsent suppression, retry after backpressure and no owner encoding\n",checks);return 0;
}
