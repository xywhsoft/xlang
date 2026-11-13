#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#include "services/edit_call.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
static xide_language_client* ownedClients[2];
static void stopClient(xide_language_client* c){for(size_t i=0;i<2;++i)if(ownedClients[i]==c)ownedClients[i]=NULL;xideLanguageClientStop(c);}
static int failed(int line,const char* text){fprintf(stderr,"edit-call:%d:%s\n",line,text);for(size_t i=0;i<2;++i)if(ownedClients[i])stopClient(ownedClients[i]);return 1;}
#define CHECK(v) do{++checks;if(!(v))return failed(__LINE__,#v);}while(0)
static const xvalue* get(const xvalue* v,const char* k){return xrtValueIs(v,XVALUE_OBJECT)?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int put(xvalue* v,const char* k,xvalue* x){return x&&xrtValueObjectSetNew(v,xrtStrView(k),x);}
static int equal(const xvalue* v,const char* s){xstrview raw={0};return xrtValueGetString(v,&raw)&&raw.Size==strlen(s)&&!memcmp(raw.Data,s,raw.Size);}
static int integer(const xvalue* v,int64 expected){int64 n=0;return xrtValueIs(v,XVALUE_INT)&&xrtValueGetInt(v,&n)&&n==expected;}
typedef struct peer {int error,okay,seen;} peer;
static int replyPeer(const xvalue* root,void* raw)
{
 peer* p=raw;const xvalue* params=get(root,"params"),*ext=get(params,"xlang");xvalue* response=xrtValueObject();int okay=response&&put(response,"jsonrpc",xrtValueString(xrtStrView("2.0")))&&put(response,"id",xrtValueDeepClone(get(root,"id")));
 p->seen=equal(get(root,"method"),XIDE_RENAME_CANDIDATE_METHOD)&&equal(get(ext,"token"),"ffffffffffffffff")&&equal(get(params,"newName"),"新的_😀")&&xrtValueCount(get(ext,"standaloneRoots"))==2&&integer(get(ext,"schema"),1)&&integer(get(ext,"scopeCount"),2)&&integer(get(get(ext,"inputState"),"documents"),1)&&equal(get(get(params,"textDocument"),"uri"),"file:///F:/call/main.xl")&&integer(get(get(params,"textDocument"),"version"),1)&&integer(get(get(params,"position"),"line"),1)&&integer(get(get(params,"position"),"character"),5)&&equal(get(get(ext,"catalog"),"packagePath"),"")&&xrtValueCount(get(get(ext,"catalog"),"projects"))==0;
 if(p->error){xvalue* error=xrtValueObject();okay=okay&&error&&put(error,"code",xrtValueInt(0))&&put(error,"message",xrtValueString(xrtStrView("")))&&put(response,"error",error);}
 else{
  xvalue *result=xrtValueObject(),*binding=xrtValueObject(),*candidate=xrtValueObject(),*guards=xrtValueArray();okay=okay&&result&&binding&&candidate&&guards;
  if(okay)okay=put(binding,"method",xrtValueString(xrtStrView(XIDE_RENAME_CANDIDATE_METHOD)))&&put(binding,"token",xrtValueDeepClone(get(ext,"token")))&&put(binding,"inputDigest",xrtValueDeepClone(get(get(ext,"inputState"),"digest")))&&put(binding,"documents",xrtValueDeepClone(get(get(ext,"inputState"),"documents")));
  /* Intentionally NOT a native source proof: routing must remain distinct from
   * later full proof/coverage/workspace-edit admission. */
  if(okay)okay=xrtValueArrayAppendNew(guards,xrtValueObject())&&put(candidate,"schema",xrtValueInt(1))&&put(candidate,"scopeCount",xrtValueDeepClone(get(ext,"scopeCount")))&&put(candidate,"workspaceEdit",xrtValueObject());
  if(okay){okay=put(candidate,"inputGuards",guards);guards=NULL;}if(okay)okay=put(result,"schema",xrtValueInt(1));if(okay){okay=put(result,"binding",binding);binding=NULL;}if(okay){okay=put(result,"candidate",candidate);candidate=NULL;}if(okay){okay=put(response,"result",result);result=NULL;}xrtValueRelease(result);xrtValueRelease(binding);xrtValueRelease(candidate);xrtValueRelease(guards);
 }
 if(!p->seen){okay=0;}size_t length=0;char* frame=okay?xideLspFrame(response,&length,NULL):NULL;xrtValueRelease(response);if(!frame)return 0;size_t offset=0;while(offset<length){DWORD written=0;if(!WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),frame+offset,(DWORD)(length-offset),&written,NULL)||!written){free(frame);return 0;}offset+=written;}free(frame);p->okay=1;return 1;
}
static int peerMain(int error)
{peer p={.error=error};xide_lsp_stream stream={0};char bytes[4096];DWORD count;while(!p.okay&&ReadFile(GetStdHandle(STD_INPUT_HANDLE),bytes,sizeof(bytes),&count,NULL)&&count)if(!xideLspStreamFeed(&stream,bytes,count,replyPeer,&p,NULL))break;xideLspStreamClear(&stream);return p.okay?0:1;}
static int modelAt(void* raw,size_t i,xide_edit_request_model* out){if(i)return 0;*out=*(xide_edit_request_model*)raw;return 1;}
static int emit(void* raw,const char* method,const xvalue* params){(void)raw;(void)method;(void)params;return 1;}
static int stop(void* raw){(void)raw;return 1;}
static xide_edit_request* makeRequest(xls_sync* sync,xvalue* catalog,xide_edit_request_model* model)
{xide_file path={.path="F:/call/loose.xl"};xide_files files={&path,1,1};return xideEditRequestCreate(UINT64_MAX,7,sync,catalog,&files,1,modelAt,model,NULL);}
static xvalue* next(xide_language_client* c,size_t* bytes)
{ULONGLONG until=GetTickCount64()+10000;while(GetTickCount64()<until){xvalue* root=xideLanguageClientNextSized(c,bytes);if(root)return root;if(xideLanguageClientState(c,NULL,0)>=XIDE_LS_STOPPED)return NULL;Sleep(1);}return NULL;}
int main(int argc,char** argv)
{
 if(argc>=2&&!strcmp(argv[1],"--peer")){return peerMain(argc>=3&&!strcmp(argv[2],"error"));}if(argc!=2)return 2;
 const char *arguments[]={"--peer"};xide_language_client_spec spec={argv[0],argv[1],arguments,1};xide_language_client* client=xideLanguageClientStartProcess(&spec,NULL);ownedClients[0]=client;CHECK(client);uint64_t connection=xideLanguageClientIdentity(client);CHECK(connection);
 xls_sync* sync=xlsSyncCreateMonotonic(2);char* uri=xideLspPathToUri("F:/call/main.xl",NULL);const char* body="//😀\r\nfunc main(...) as int\n return 0\nend\n";xls_sync_document doc={uri,body,strlen(body),11,7};CHECK(sync&&uri&&xlsSyncUpdate(sync,&doc,1,emit,NULL));xvalue* catalog=xrtJsonParse(xrtStrView("{\"schema\":1,\"projects\":[],\"selectedProject\":\"\",\"packagePath\":\"\"}"));CHECK(catalog);xide_edit_request_model model={11,7,1,0,0,"F:/call/main.xl","xlang"};xide_edit_request* request=makeRequest(sync,catalog,&model);CHECK(request);enum xide_edit_call_error why;
 CHECK(!xideEditCallCreate(NULL,connection,uri,1,5,xrtStrView("x"),NULL,NULL,&why)&&why==XIDE_EDIT_CALL_ARGUMENT);CHECK(!xideEditCallCreate(request,0,uri,1,5,xrtStrView("x"),NULL,NULL,&why));CHECK(!xideEditCallCreate(request,connection,uri,0,3,xrtStrView("x"),NULL,NULL,&why));CHECK(!xideEditCallCreate(request,connection,uri,99,0,xrtStrView("x"),NULL,NULL,&why));CHECK(!xideEditCallCreate(request,connection,"file:///F:/call/other.xl",1,0,xrtStrView("x"),NULL,NULL,&why));
 const char* invalid[]={"","a\nb","a\rb","\xc0\xaf"};for(size_t i=0;i<4;++i)CHECK(!xideEditCallCreate(request,connection,uri,1,5,xrtStrView(invalid[i]),NULL,NULL,&why));char nul[]={'a',0,'b'};CHECK(!xideEditCallCreate(request,connection,uri,1,5,(xstrview){nul,3},NULL,NULL,&why));CHECK(!xideEditCallCreate(request,connection,uri,1,5,xrtStrView("x"),stop,NULL,&why)&&why==XIDE_EDIT_CALL_STALE&&!xideEditRequestInvalidated(request));
 model.revision=8;xide_edit_request* bad=makeRequest(sync,catalog,&model);CHECK(bad&&!xideEditCallCreate(bad,connection,uri,1,5,xrtStrView("x"),NULL,NULL,&why));xideEditRequestRelease(bad);model.revision=7;
 char name[]="新的_😀";xide_edit_call* call=xideEditCallCreate(request,connection,uri,1,5,xrtStrView(name),NULL,NULL,&why);CHECK(call&&why==XIDE_EDIT_CALL_OK&&xideEditCallState(call)==XIDE_EDIT_CALL_PREPARED&&xideEditCallRequest(call)==request&&xideEditCallConnection(call)==connection&&xideRenameScopeCount(xideEditCallScope(call))==2);xide_edit_call_intent intent={0};CHECK(xideEditCallIntent(call,&intent)&&intent.identity==11&&intent.revision==7&&intent.version==1&&intent.position.line==1&&intent.position.character==5&&intent.position.offset==13&&!strcmp(intent.uri,uri)&&intent.newName.Size==strlen(name));name[0]='X';CHECK(!memcmp(intent.newName.Data,"新的_😀",intent.newName.Size));
 CHECK(!xideEditCallCreate(request,connection,uri,1,5,xrtStrView("other"),NULL,NULL,&why)&&why==XIDE_EDIT_CALL_REPLAY);
 xvalue* unsent=xrtValueNull();CHECK(unsent&&!xideEditCallReply(call,connection,unsent,1,&why)&&why==XIDE_EDIT_CALL_REPLAY);xrtValueRelease(unsent);
 xide_language_client* other=xideLanguageClientStartProcess(&spec,NULL);ownedClients[1]=other;CHECK(other&&xideLanguageClientIdentity(other)!=connection);CHECK(!xideEditCallSend(call,other)&&xideEditCallState(call)==XIDE_EDIT_CALL_PREPARED);CHECK(xideEditCallSend(call,client)&&xideEditCallState(call)==XIDE_EDIT_CALL_QUEUED&&!xideEditCallSend(call,client));size_t bytes=0;xvalue* root=next(client,&bytes);CHECK(root&&bytes);
 CHECK(!xideEditCallReply(call,xideLanguageClientIdentity(other),root,bytes,&why)&&why==XIDE_EDIT_CALL_PEER);CHECK(!xideEditCallReply(call,connection,root,0,&why)&&why==XIDE_EDIT_CALL_ARGUMENT);CHECK(!xideEditCallReply(call,connection,root,XIDE_LSP_BODY_BYTES+1u,&why));
 const char* fields[]={"jsonrpc","id","method","token","inputDigest","documents","scopeCount","schema"};
 for(size_t i=0;i<8;++i){xvalue* changed=xrtValueDeepClone(root);CHECK(changed);xvalue* parent=changed;if(i>=2&&i<=5)parent=(xvalue*)get(get(changed,"result"),"binding");if(i==6)parent=(xvalue*)get(get(changed,"result"),"candidate");if(i==7)parent=(xvalue*)get(changed,"result");CHECK(put(parent,fields[i],i==1||i==5||i==6||i==7?xrtValueInt(99):xrtValueString(xrtStrView("wrong"))));CHECK(!xideEditCallReply(call,connection,changed,bytes,&why)&&why==XIDE_EDIT_CALL_SHAPE&&xideEditCallState(call)==XIDE_EDIT_CALL_QUEUED);xrtValueRelease(changed);}
 xvalue* changed=xrtValueDeepClone(root);CHECK(changed&&put(changed,"id",xrtJsonParse(xrtStrView("7.0")))&&!xideEditCallReply(call,connection,changed,bytes,&why));xrtValueRelease(changed);
 xvalue* duplicate=xrtValueRetain(root);xide_edit_reply* reply=xideEditCallReply(call,connection,root,bytes,&why);CHECK(reply&&why==XIDE_EDIT_CALL_OK&&xideEditReplyCurrent(reply)&&xideEditReplyCandidate(reply)&&!xideEditReplyError(reply,NULL,NULL)&&xideEditCallState(call)==XIDE_EDIT_CALL_REPLIED);CHECK(!xideEditCallReply(call,connection,duplicate,bytes,&why)&&why==XIDE_EDIT_CALL_REPLAY);xrtValueRelease(duplicate);CHECK(xrtValueCount(xrtValueArrayGet(get(xideEditReplyCandidate(reply),"inputGuards"),0))==0); /* raw routing is not source approval */
 xideEditCallCancel(call);CHECK(!xideEditReplyCurrent(reply)&&xideEditRequestInvalidated(request));xideEditReplyRelease(reply);xideEditCallRelease(call);xideEditRequestRelease(request);stopClient(other);stopClient(client);
 const char* errorArgs[]={"--peer","error"};spec.arguments=errorArgs;spec.argumentCount=2;client=xideLanguageClientStartProcess(&spec,NULL);ownedClients[0]=client;CHECK(client&&xideLanguageClientIdentity(client)>connection);request=makeRequest(sync,catalog,&model);CHECK(request);call=xideEditCallCreate(request,xideLanguageClientIdentity(client),uri,1,5,xrtStrView("新的_😀"),NULL,NULL,&why);CHECK(call&&xideEditCallSend(call,client));root=next(client,&bytes);CHECK(root);reply=xideEditCallReply(call,xideLanguageClientIdentity(client),root,bytes,&why);int code=123;xstrview message={0};CHECK(reply&&xideEditReplyError(reply,&code,&message)&&code==0&&!message.Size&&!xideEditReplyCandidate(reply));xideEditRequestInvalidate(request,XIDE_EDIT_REQUEST_CONNECTION);CHECK(!xideEditReplyCurrent(reply));stopClient(client);xideEditCallRelease(call);xideEditRequestRelease(request);CHECK(xideEditReplyError(reply,&code,&message));xideEditReplyRelease(reply);xlsSyncDestroy(sync);xrtValueRelease(catalog);free(uri);
 printf("PASS edit call: %u checks; actual stdio/parser/queue, complete request packet, UTF16 and captured intent, channel/ID/token/input binding, one call/reply, caller-owned rejection and independent cancelled/error lifetime; candidate remains untrusted\n",checks);return 0;
}
