#include "services/edit_call.h"
#include "platform/path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct xide_edit_call {
 volatile int32 references;xatomic32 state;xide_edit_request* request;xide_rename_scope* scope;
 uint64_t connection;xide_language_batch* batch;char token[17],digest[65];size_t documents;
 xide_edit_call_intent intent;
};
struct xide_edit_reply {xide_edit_call* call;xvalue* root;int error,code;xstrview message;};
const xide_edit_call* xideEditReplyCall(const xide_edit_reply* p){return p?p->call:NULL;}
static const xvalue* get(const xvalue* v,const char* k){return xrtValueIs(v,XVALUE_OBJECT)?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int put(xvalue* v,const char* k,xvalue* item){return item&&xrtValueObjectSetNew(v,xrtStrView(k),item);}
static int eq(const xvalue* v,const char* s){xstrview raw={0};size_t n=strlen(s);return xrtValueGetString(v,&raw)&&raw.Size==n&&(!n||!memcmp(raw.Data,s,n));}
static int number(const xvalue* v,int64 expected){int64 value=0;return xrtValueIs(v,XVALUE_INT)&&xrtValueGetInt(v,&value)&&value==expected;}
static int stopped(const xide_edit_request* p,int(*cancelled)(void*),void* user){return xideEditRequestInvalidated(p)||(cancelled&&cancelled(user));}
xide_edit_call* xideEditCallRetain(xide_edit_call* p){return p&&xrtRefRetain(&p->references)>0?p:NULL;}
void xideEditCallRelease(xide_edit_call* p){if(p&&xrtRefRelease(&p->references)==0){xideLanguageBatchDestroy(p->batch);xideRenameScopeRelease(p->scope);xideEditRequestRelease(p->request);xrtFree(p);}}
const xide_edit_request* xideEditCallRequest(const xide_edit_call* p){return p?p->request:NULL;}
const xide_rename_scope* xideEditCallScope(const xide_edit_call* p){return p?p->scope:NULL;}
uint64_t xideEditCallConnection(const xide_edit_call* p){return p?p->connection:0;}
int xideEditCallIntent(const xide_edit_call* p,xide_edit_call_intent* out){if(out)*out=(xide_edit_call_intent){0};if(!p||!out)return 0;*out=p->intent;return 1;}
enum xide_edit_call_state xideEditCallState(const xide_edit_call* p){return p?(enum xide_edit_call_state)xrtAtomic32Load(&p->state,XMEMORY_ACQUIRE):XIDE_EDIT_CALL_CANCELLED;}
static xvalue* parameters(xide_edit_call* call,const char* uri,int version,int line,int character,xstrview name)
{
 xvalue *root=xrtValueObject(),*extension=xrtValueObject(),*receipt=xrtValueObject(),*files=xrtValueArray(),*document=xrtValueObject(),*position=xrtValueObject();int okay=root&&extension&&receipt&&files&&document&&position;
 for(size_t i=0;okay&&i<xideRenameScopeCount(call->scope);++i){const xls_context_spec* spec=xlsContextSpec(xideRenameScopeAt(call->scope,i));if(spec->kind==XLS_CONTEXT_XLANG)okay=xrtValueArrayAppendNew(files,xrtValueString(xrtStrView(spec->entry)));}
 if(okay)okay=put(receipt,"digest",xrtValueString(xrtStrView(call->digest)))&&put(receipt,"documents",xrtValueInt((int64)call->documents));
 if(okay)okay=put(extension,"schema",xrtValueInt(1))&&put(extension,"token",xrtValueString(xrtStrView(call->token)))&&put(extension,"scopeCount",xrtValueInt((int64)xideRenameScopeCount(call->scope)))&&put(extension,"catalog",xrtValueRetain((xvalue*)xideEditRequestCatalog(call->request)));
 if(okay){okay=put(extension,"inputState",receipt);receipt=NULL;}if(okay){okay=put(extension,"standaloneRoots",files);files=NULL;}
 if(okay)okay=put(document,"uri",xrtValueString(xrtStrView(uri)))&&put(document,"version",xrtValueInt(version))&&put(position,"line",xrtValueInt(line))&&put(position,"character",xrtValueInt(character));
 if(okay){okay=put(root,"textDocument",document);document=NULL;}if(okay){okay=put(root,"position",position);position=NULL;}if(okay){okay=put(root,"xlang",extension);extension=NULL;}
 if(okay){okay=put(root,"newName",xrtValueString(name));}xrtValueRelease(extension);xrtValueRelease(receipt);xrtValueRelease(files);xrtValueRelease(document);xrtValueRelease(position);if(!okay){xrtValueRelease(root);root=NULL;}return root;
}
xide_edit_call* xideEditCallCreate(xide_edit_request* request,uint64_t connection,const char* uri,int line,int character,xstrview name,int(*cancelled)(void*),void* user,enum xide_edit_call_error* why)
{
 enum xide_edit_call_error error=XIDE_EDIT_CALL_ARGUMENT;xide_edit_call* call=NULL;xls_text* text=NULL;char* path=NULL;xvalue* params=NULL;xls_sync_binding selected={0};
 if(!request||!connection||!uri||!*uri||strlen(uri)>262144||line<0||character<0||!name.Data||!name.Size||name.Size>4096||memchr(name.Data,0,name.Size)||memchr(name.Data,'\r',name.Size)||memchr(name.Data,'\n',name.Size)||!xrtUtf8Valid(name,NULL))goto done;
 if(stopped(request,cancelled,user)){error=XIDE_EDIT_CALL_STALE;goto done;}
 for(size_t i=0;xlsSyncBindingAt(xideEditRequestSync(request),i,&selected);++i)if(!strcmp(selected.uri,uri))break;
 if(!selected.uri||strcmp(selected.uri,uri))goto done;
 path=xideLspUriToPath(uri,NULL);if(!path||!xidePathHasExtension(path,".xl"))goto done;
 int bound=0;for(size_t i=0;i<xideEditRequestModelCount(request);++i){xide_edit_request_model m={0};if(!xideEditRequestModelAt(request,i,&m))goto done;if(m.identity==selected.identity&&m.revision==selected.revision&&m.open&&m.path&&m.language&&!strcmp(m.language,"xlang")&&xidePathLexicalEqual(m.path,path)){bound=1;break;}}
 if(!bound)goto done;
 text=xlsSyncRetainText(xideEditRequestSync(request),uri,selected.version);size_t offset=0;if(!text||!xlsTextOffset(text,(xls_position){(size_t)line,(size_t)character},&offset))goto done;
 size_t uriBytes=strlen(uri)+1;error=XIDE_EDIT_CALL_MEMORY;call=xrtCalloc(1,sizeof(*call)+uriBytes+name.Size+1);if(!call)goto done;call->references=1;xrtAtomic32Init(&call->state,0);call->connection=connection;call->request=xideEditRequestRetain(request);if(!call->request)goto done;
 char* own=(char*)(call+1);memcpy(own,uri,uriBytes);call->intent.uri=own;own+=uriBytes;memcpy(own,name.Data,name.Size);own[name.Size]=0;call->intent.newName=(xstrview){own,name.Size};call->intent.position=(xide_lsp_position){line,character,(int)offset};call->intent.version=selected.version;call->intent.identity=selected.identity;call->intent.revision=selected.revision;
 enum xide_rename_scope_error cause;call->scope=xideEditRequestScope(request,0,cancelled,user,&cause);if(!call->scope){error=cause==XIDE_RENAME_SCOPE_CANCELLED?XIDE_EDIT_CALL_STALE:XIDE_EDIT_CALL_ARGUMENT;goto done;}
 uint64_t owners=0;if(!xideRenameScopeOwners(call->scope,path,&owners)||!owners){error=XIDE_EDIT_CALL_ARGUMENT;goto done;}
 snprintf(call->token,sizeof(call->token),"%016llx",(unsigned long long)xideEditRequestToken(request));if(!xlsSyncInputState(xideEditRequestSync(request),call->digest,&call->documents))goto done;
 params=parameters(call,call->intent.uri,selected.version,line,character,call->intent.newName);call->batch=params?xideLanguageBatchCreate():NULL;
 if(!call->batch||!xideLanguageBatchAppendRequest(call->batch,XIDE_RENAME_CANDIDATE_METHOD,params,xideEditRequestId(request)))goto done;
 if(stopped(request,cancelled,user)){error=XIDE_EDIT_CALL_STALE;goto done;}
 if(!xideEditRequestClaimCall(request)){error=XIDE_EDIT_CALL_REPLAY;goto done;}error=XIDE_EDIT_CALL_OK;
done:
 free(path);xlsTextRelease(text);xrtValueRelease(params);if(error){xideEditCallRelease(call);call=NULL;}if(why)*why=error;return call;
}
int xideEditCallSend(xide_edit_call* call,xide_language_client* client)
{if(!call||!call->batch||xideEditRequestInvalidated(call->request))return 0;if(!xideLanguageBatchSendBound(client,call->batch,call->connection,&call->state))return 0;call->batch=NULL;return 1;}
void xideEditCallCancel(xide_edit_call* call){if(call){xrtAtomic32Exchange(&call->state,XIDE_EDIT_CALL_CANCELLED,XMEMORY_ACQ_REL);xideEditRequestInvalidate(call->request,XIDE_EDIT_REQUEST_CANCELLED);}}
xide_edit_reply* xideEditCallReply(xide_edit_call* call,uint64_t peer,xvalue* root,size_t bytes,enum xide_edit_call_error* why)
{
 enum xide_edit_call_error error=XIDE_EDIT_CALL_ARGUMENT;xide_edit_reply* reply=NULL;
 if(!call||!root||!bytes||bytes>XIDE_LSP_BODY_BYTES)goto done;
 if(peer!=call->connection){error=XIDE_EDIT_CALL_PEER;goto done;}
 if(xideEditRequestInvalidated(call->request)||xideEditCallState(call)==XIDE_EDIT_CALL_CANCELLED){error=XIDE_EDIT_CALL_STALE;goto done;}
 if(xideEditCallState(call)!=XIDE_EDIT_CALL_QUEUED){error=XIDE_EDIT_CALL_REPLAY;goto done;}
 error=XIDE_EDIT_CALL_SHAPE;if(!xrtValueIs(root,XVALUE_OBJECT)||xrtValueCount(root)!=3||!eq(get(root,"jsonrpc"),"2.0")||!number(get(root,"id"),xideEditRequestId(call->request)))goto done;
 const xvalue *result=get(root,"result"),*remote=get(root,"error");int isError=0,code=0;xstrview message={0};
 if(remote){int64 value=0;if(result||!xrtValueIs(remote,XVALUE_OBJECT)||(xrtValueCount(remote)!=2&&xrtValueCount(remote)!=3)||!xrtValueIs(get(remote,"code"),XVALUE_INT)||!xrtValueGetInt(get(remote,"code"),&value)||value<INT32_MIN||value>INT32_MAX||!xrtValueGetString(get(remote,"message"),&message)||message.Size>65536||(message.Size&&(!message.Data||memchr(message.Data,0,message.Size)))||(message.Size&&!xrtUtf8Valid(message,NULL))||(xrtValueCount(remote)==3&&!get(remote,"data")))goto done;isError=1;code=(int)value;}
 else{
  const xvalue *binding=get(result,"binding"),*candidate=get(result,"candidate");
  if(!xrtValueIs(result,XVALUE_OBJECT)||xrtValueCount(result)!=3||!number(get(result,"schema"),1)||!xrtValueIs(binding,XVALUE_OBJECT)||xrtValueCount(binding)!=4||!eq(get(binding,"method"),XIDE_RENAME_CANDIDATE_METHOD)||!eq(get(binding,"token"),call->token)||!eq(get(binding,"inputDigest"),call->digest)||!number(get(binding,"documents"),(int64)call->documents)||!xrtValueIs(candidate,XVALUE_OBJECT)||xrtValueCount(candidate)!=4||!number(get(candidate,"schema"),1)||!number(get(candidate,"scopeCount"),(int64)xideRenameScopeCount(call->scope))||!xrtValueIs(get(candidate,"workspaceEdit"),XVALUE_OBJECT)||!xrtValueIs(get(candidate,"inputGuards"),XVALUE_ARRAY)||!xrtValueCount(get(candidate,"inputGuards"))||xrtValueCount(get(candidate,"inputGuards"))>65536)goto done;
 }
 error=XIDE_EDIT_CALL_MEMORY;reply=xrtCalloc(1,sizeof(*reply));if(!reply)goto done;reply->call=xideEditCallRetain(call);if(!reply->call)goto done;
 uint32 pending=XIDE_EDIT_CALL_QUEUED;if(!xrtAtomic32CompareExchange(&call->state,&pending,XIDE_EDIT_CALL_REPLIED,XMEMORY_ACQ_REL,XMEMORY_ACQUIRE)){error=XIDE_EDIT_CALL_REPLAY;goto done;}
 if(xideEditRequestInvalidated(call->request)){error=XIDE_EDIT_CALL_STALE;goto done;}
 reply->root=root;reply->error=isError;reply->code=code;reply->message=message;error=XIDE_EDIT_CALL_OK;
done:
 if(error){xideEditReplyRelease(reply);reply=NULL;}if(why)*why=error;return reply;
}
int xideEditReplyCurrent(const xide_edit_reply* reply){return reply&&reply->call&&!xideEditRequestInvalidated(reply->call->request)&&xideEditCallState(reply->call)==XIDE_EDIT_CALL_REPLIED;}
int xideEditReplyError(const xide_edit_reply* reply,int* code,xstrview* message){if(code)*code=0;if(message)*message=(xstrview){0};if(!reply||!reply->error)return 0;if(code)*code=reply->code;if(message)*message=reply->message;return 1;}
const xvalue* xideEditReplyCandidate(const xide_edit_reply* reply){return reply&&!reply->error?get(get(reply->root,"result"),"candidate"):NULL;}
void xideEditReplyRelease(xide_edit_reply* reply){if(reply){xrtValueRelease(reply->root);xideEditCallRelease(reply->call);xrtFree(reply);}}
