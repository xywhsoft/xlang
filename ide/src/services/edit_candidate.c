#include "services/edit_candidate.h"
#include "services/edit_candidate_internal.h"
#include "platform/file_io.h"
#include "platform/text_file.h"
#include "services/reference_document.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define CANDIDATE_BYTES (256u*1024u*1024u)
struct xide_edit_candidate {volatile int32 references;xide_edit_call* call;xide_rename_evidence* evidence;xide_workspace_edit* plan;};
static const xvalue* get(const xvalue* v,const char* k){return xideReferenceDocumentField(v,k);}
static const char* string(const xvalue* v){return xideReferenceDocumentString(v,262144);}
static int object(const xvalue* v,size_t n){return xrtValueIs(v,XVALUE_OBJECT) && xrtValueCount(v)==n;}
static int integer(const xvalue* v,int* n){int64 value=0;if(!xrtValueIs(v,XVALUE_INT) || !xrtValueGetInt(v,&value) || value<INT_MIN || value>INT_MAX)return 0;*n=(int)value;return 1;}
static int failed(xide_model_error* e,const char* s){if(e){e->code=XIDE_MODEL_INVALID;snprintf(e->message,sizeof(e->message),"%s",s);}return 0;}
static int cancelled(xide_edit_call* call,int (*stop)(void*),void* user){return xideEditRequestInvalidated(xideEditCallRequest(call)) || xideEditCallState(call)!=XIDE_EDIT_CALL_REPLIED || (stop && stop(user));}
typedef struct candidate_stop {xide_edit_call* call;int (*stop)(void*);void* user;} candidate_stop;
static int stopped(void* user){candidate_stop* p=user;return cancelled(p->call,p->stop,p->user);}
static int sameReport(const xvalue* a,const xvalue* b)
{if(!xrtValueIs(a,XVALUE_OBJECT) || !xrtValueIs(b,XVALUE_OBJECT) || xrtValueCount(a)!=xrtValueCount(b))return 0;for(size_t i=0;i<xrtValueCount(a);++i){xstrview key={0};const xvalue* v=xrtValueObjectAt(a,i,&key);if(key.Size==4 && !memcmp(key.Data,"path",4))continue;if(!xrtValueEqual(v,xrtValueObjectGet(b,key)))return 0;}return 1;}
static int sameBytes(const char* a,const char* b,size_t n,xide_edit_call* call,int (*stop)(void*),void* user)
{for(size_t at=0;at<n;){if(cancelled(call,stop,user))return 0;size_t part=n-at>65536?65536:n-at;if(memcmp(a+at,b+at,part))return 0;at+=part;}return 1;}
static int rawMatches(const unsigned char* raw,size_t size,const xvalue* proof,xide_edit_call* call,int (*stop)(void*),void* user)
{
 const char* expected=string(get(proof,"rawSha256"));if(!xideReferenceDocumentHex(get(proof,"rawSha256")))return 0;xsha256 state;unsigned char hash[32];xrtSha256Init(&state);
 for(size_t at=0;at<size;){if(cancelled(call,stop,user))return 0;size_t part=size-at>65536?65536:size-at;if(!xrtSha256Update(&state,raw+at,part))return 0;at+=part;}
 if(!xrtSha256Final(&state,hash))return 0;
 static const char digits[]="0123456789abcdef";for(size_t i=0;i<32;++i)if(digits[hash[i]>>4]!=expected[2*i] || digits[hash[i]&15]!=expected[2*i+1])return 0;return 1;
}
static int position(const xvalue* v)
{int line=0,column=0;return object(v,2) && integer(get(v,"line"),&line) && integer(get(v,"character"),&column) && line>=0 && column>=0;}
static int edits(const xvalue* rows,xstrview name,xide_edit_call* call,int (*stop)(void*),void* user)
{
 if(!xrtValueIs(rows,XVALUE_ARRAY) || !xrtValueCount(rows) || xrtValueCount(rows)>65536)return 0;
 for(size_t i=0;i<xrtValueCount(rows);++i){const xvalue* row=xrtValueArrayGet(rows,i);const xvalue* range=get(row,"range");xstrview replacement={0};
  if(cancelled(call,stop,user) || !object(row,2) || !object(range,2) || !position(get(range,"start")) || !position(get(range,"end")) || !xrtValueGetString(get(row,"newText"),&replacement) || replacement.Size!=name.Size || memcmp(replacement.Data,name.Data,name.Size))return 0;
 }return 1;
}
xide_edit_candidate* xideEditCandidateRetain(xide_edit_candidate* c){return c&&xrtRefRetain(&c->references)>0?c:NULL;}
void xideEditCandidateRelease(xide_edit_candidate* c){if(c&&xrtRefRelease(&c->references)==0){xideWorkspaceEditRelease(c->plan);xideRenameEvidenceRelease(c->evidence);xideEditCallRelease(c->call);xrtFree(c);}}
size_t xideEditCandidateSourceCount(const xide_edit_candidate* c){return c?xideRenameEvidenceFiles(c->evidence):0;}
size_t xideEditCandidateStorage(const xide_edit_candidate* c){xide_workspace_edit_info i;return c&&xideWorkspaceEditInfo(c->plan,&i)?sizeof(*c)+i.bytes+xideRenameEvidenceBytes(c->evidence):0;}
const xvalue* xideEditCandidateSourceAt(const xide_edit_candidate* c,size_t i){return c?xideRenameEvidenceFileAt(c->evidence,i):NULL;}
const xvalue* xideEditCandidateSource(const xide_edit_candidate* c,const char* path){return c?xideRenameEvidenceFile(c->evidence,path,NULL):NULL;}
const xide_workspace_edit* xideEditCandidatePlan(const xide_edit_candidate* c){return c?c->plan:NULL;}
xide_workspace_edit* xideEditCandidateRetainPlan(const xide_edit_candidate* c){return c?xideWorkspaceEditRetain(c->plan):NULL;}
const xide_edit_call* xideEditCandidateCall(const xide_edit_candidate* c){return c?c->call:NULL;}
int xideEditCandidateCurrent(const xide_edit_candidate* c){return c && !cancelled(c->call,NULL,NULL) && xideRenameEvidenceCurrent(c->evidence);}
int xideEditCandidateValidate(xide_edit_candidate* c,int (*stop)(void*),void* user,xide_model_error* error)
{candidate_stop state={c?c->call:NULL,stop,user};return c && !cancelled(c->call,stop,user) && xideRenameEvidenceValidate(c->evidence,stopped,&state,error) && !cancelled(c->call,stop,user);}
int xideEditCandidateValidateOwned(xide_edit_candidate* c,const xvalue* const* reports,size_t count,int (*stop)(void*),void* user,xide_model_error* error)
{candidate_stop state={c?c->call:NULL,stop,user};return c&&!cancelled(c->call,stop,user)&&xideRenameEvidenceValidateOwned(c->evidence,reports,count,stopped,&state,error)&&!cancelled(c->call,stop,user);}
xide_edit_candidate* xideEditCandidatePrepare(xide_edit_call* call,const xide_edit_reply* reply,size_t budget,int (*stop)(void*),void* user,xide_model_error* error)
{
 xide_edit_candidate* result=NULL;xide_language_proof_overlay overlays[64]={0};size_t overlayCount=0,targetCount=0,rawTotal=0;char* overlayPaths[64]={0};
 xide_workspace_edit_input* inputs=NULL;char** paths=NULL;const xls_sync* sync=xideEditRequestSync(xideEditCallRequest(call));xide_edit_call_intent intent={0};
 const xvalue* candidate=xideEditReplyCandidate(reply);const xvalue* edit=get(candidate,"workspaceEdit");const xvalue* changes=get(edit,"documentChanges");
 budget=budget?budget:CANDIDATE_BYTES;
 int schema=0,scopes=0;candidate_stop stopping={call,stop,user};
 if(!call || !reply || xideEditReplyCall(reply)!=call || !xideEditReplyCurrent(reply) || !xideEditCallIntent(call,&intent) || budget>CANDIDATE_BYTES || budget<=sizeof(*result) || cancelled(call,stop,user) || !object(candidate,4) || !integer(get(candidate,"schema"),&schema) || schema!=1 || !integer(get(candidate,"scopeCount"),&scopes) || scopes<1 || (size_t)scopes!=xideRenameScopeCount(xideEditCallScope(call)) || !object(edit,1) || !xrtValueIs(changes,XVALUE_ARRAY) || !(targetCount=xrtValueCount(changes)) || targetCount>512)goto failure;
 xls_sync_binding binding={0};
 for(size_t i=0;xlsSyncBindingAt(sync,i,&binding);++i){
  if(overlayCount>=64 || cancelled(call,stop,user))goto failure;
  overlayPaths[overlayCount]=xideLspUriToPath(binding.uri,NULL);xls_text* text=xlsSyncRetainText(sync,binding.uri,binding.version);
  overlays[overlayCount]=(xide_language_proof_overlay){overlayPaths[overlayCount],text};++overlayCount;if(!text || !overlays[overlayCount-1].path)goto failure;
 }
 result=xrtCalloc(1,sizeof(*result));if(!result)goto failure;result->references=1;result->call=xideEditCallRetain(call);
 result->evidence=xideRenameEvidenceCreate((xide_rename_scope*)xideEditCallScope(call),get(candidate,"inputGuards"),overlays,overlayCount,budget,stopped,&stopping,error);if(!result->evidence)goto failure;
 const xide_edit_request* request=xideEditCallRequest(call);
 for(size_t i=0;i<xideEditRequestModelCount(request);++i){
  if(cancelled(call,stop,user))goto failure;
  xide_edit_request_model model={0};if(!xideEditRequestModelAt(request,i,&model))goto failure;if(!model.path || !*model.path)continue;
  if(!xrtPathIsAbs(model.path))goto failure;
  int synced=0;
  for(size_t j=0;xlsSyncBindingAt(sync,j,&binding);++j)if(binding.identity==model.identity && binding.revision==model.revision && model.open && model.language && !strcmp(model.language,"xlang") && xidePathLexicalEqual(model.path,overlays[j].path)){synced=1;break;}
  if((!synced || model.external) && xideRenameEvidenceTouches(result->evidence,model.path))goto failure;
 }
 inputs=xrtCalloc(targetCount,sizeof(*inputs));paths=xrtCalloc(targetCount,sizeof(*paths));if(!inputs || !paths)goto failure;
 for(size_t i=0;i<targetCount;++i){
  if(cancelled(call,stop,user))goto failure;
  const xvalue* row=xrtValueArrayGet(changes,i),*document=get(row,"textDocument"),*extension=get(row,"xlang"),*proof=get(extension,"document");const char* uri=string(get(document,"uri"));int rowSchema=0;
  if(!object(row,3) || !object(document,2) || !object(extension,2) || !integer(get(extension,"schema"),&rowSchema) || rowSchema!=1 || !uri || !edits(get(row,"edits"),intent.newName,call,stop,user))goto failure;
  inputs[i].uri=uri;paths[i]=xideLspUriToPath(uri,NULL);uint64_t owners=0;if(!paths[i] || !xidePathHasExtension(paths[i],".xl") || !xideRenameScopeOwners(xideEditCallScope(call),paths[i],&owners) || !owners)goto failure;
  const xvalue* original=xideRenameEvidenceFile(result->evidence,paths[i],NULL);if(!original || !xideReferenceDocumentValid(proof,paths[i]))goto failure;
  size_t online=SIZE_MAX;for(size_t j=0;j<overlayCount;++j)if(xidePathLexicalEqual(paths[i],overlays[j].path)){online=j;break;}
  const char* origin=string(get(original,"origin")),*declared=string(get(proof,"origin"));if(!origin || !declared || strcmp(origin,declared))goto failure;
  if(online!=SIZE_MAX){
   if(strcmp(origin,"overlay") || !xlsSyncBindingAt(sync,online,&binding))goto failure;
   inputs[i]=(xide_workspace_edit_input){binding.uri,xlsTextRetain((xls_text*)overlays[online].text),binding.identity,binding.revision,1,binding.version};
   int version=0;if(!integer(get(document,"version"),&version) || version!=binding.version)goto failure;
  }else{
   if(strcmp(origin,"disk") || !xrtValueIs(get(document,"version"),XVALUE_NULL) || !sameReport(original,get(proof,"input")))goto failure;
   int rawSize=0;if(!integer(get(original,"rawSize"),&rawSize) || rawSize<0 || rawSize>8*1024*1024 || (size_t)rawSize>64u*1024u*1024u-rawTotal)goto failure;rawTotal+=(size_t)rawSize;
   size_t bytes=0;unsigned char* raw=xideIoReadAllLimit(paths[i],8u*1024u*1024u,&bytes);xide_text_file decoded={0};
   int okay=raw && bytes==(size_t)rawSize && rawMatches(raw,bytes,original,call,stop,user) && xideTextFileDecode(raw,bytes,&decoded,error);xrtFree(raw);
   if(okay)inputs[i].text=xlsTextCreate(decoded.text,strlen(decoded.text),8u*1024u*1024u,NULL);
   xideTextFileClear(&decoded);if(!okay || !inputs[i].text)goto failure;
  }
 }
 size_t known=xideRenameEvidenceBytes(result->evidence);if(known>=budget-sizeof(*result))goto failure;
 enum xide_workspace_edit_error why;result->plan=xideWorkspaceEditPrepare(edit,inputs,targetCount,1,budget-known-sizeof(*result),stopped,&stopping,&why);xide_workspace_edit_info info;
 if(!result->plan || !xideWorkspaceEditInfo(result->plan,&info) || info.documents!=targetCount || !info.edits || info.changedDocuments!=info.documents || info.annotations || info.needsConfirmation)goto failure;
 const char* oldName=NULL;size_t oldBytes=0;int cursor=0;
 for(size_t i=0;i<info.documents;++i){
  xide_workspace_edit_document document;if(!xideWorkspaceEditDocumentAt(result->plan,i,&document) || !document.count)goto failure;
  for(size_t j=0;j<document.count;++j){xide_workspace_text_edit change;if(cancelled(call,stop,user) || !xideWorkspaceEditAt(result->plan,document.first+j,&change) || change.end.offset<=change.start.offset)goto failure;
   const char* selected=xlsTextData(document.before)+change.start.offset;size_t length=(size_t)(change.end.offset-change.start.offset);
   if(!oldName){oldName=selected;oldBytes=length;}if(length!=oldBytes || !sameBytes(oldName,selected,length,call,stop,user))goto failure;
   if(document.hasVersion && document.identity==intent.identity && document.revision==intent.revision && document.version==intent.version && intent.position.offset>=change.start.offset && intent.position.offset<change.end.offset)cursor=1;
  }
 }
 if(!cursor || !xideEditCandidateValidate(result,stop,user,error))goto failure;
 goto done;
failure:xideEditCandidateRelease(result);result=NULL;failed(error,"Native rename data admission rejected source coverage, model/intent binding, prepared edits, capacity, cancellation or changed inputs.");
done:
 if(inputs)for(size_t i=0;i<targetCount;++i)xlsTextRelease(inputs[i].text);
 if(paths)for(size_t i=0;i<targetCount;++i)free(paths[i]);
 xrtFree(inputs);xrtFree(paths);
 for(size_t i=0;i<overlayCount;++i){xlsTextRelease((xls_text*)overlays[i].text);free(overlayPaths[i]);}return result;
}
