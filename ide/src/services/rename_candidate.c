#include "services/rename_candidate.h"
#ifdef XLS_COMPILER_XRT
#include "analysis/pool_idle.h"
#include "analysis/identifier.h"
#include "protocol/input_state.h"
#include "platform/path.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
struct xide_rename_candidates {xtaskpool* pool;xide_project_references* references;};
struct xide_rename_candidate_query {xfuture* future;xfuturewatch watch;void (*wake)(void*);void* user;int initialized,linked;};
typedef struct candidate_input {xide_rename_candidates* service;int id,standard,documentChanges,prepare;xvalue *requestId,*params,*catalogValue;xide_analysis_catalog* catalog;xls_analysis_document docs[64];size_t count;} candidate_input;
typedef struct candidate_output {char* frame;size_t bytes;int code;char error[512];} candidate_output;
static const xvalue* get(const xvalue* v,const char* k){return xrtValueType(v)==XVALUE_OBJECT?xrtValueObjectGet(v,xrtStrView(k)):NULL;}
static int put(xvalue* v,const char* k,xvalue* item){return item && xrtValueObjectSetNew(v,xrtStrView(k),item);}
static int object(const xvalue* v,size_t n){return v && xrtValueType(v)==XVALUE_OBJECT && xrtValueCount(v)==n;}
static const char* text(const xvalue* v,size_t limit){xstrview s={0};return v && xrtValueGetString(v,&s) && s.Size<=limit && s.Data && !memchr(s.Data,0,s.Size) && xrtUtf8Valid(s,NULL)?s.Data:NULL;}
static int number(const xvalue* v,int64_t* n){return v && xrtValueType(v)==XVALUE_INT && xrtValueGetInt(v,n);}
static int integer(const xvalue* v,int* n){int64_t value=0;if(!number(v,&value) || value<INT_MIN || value>INT_MAX)return 0;*n=(int)value;return 1;}
static int stopped(void* p){return xrtCancelRequested(p);}
static void wakeEvent(void* p){xrtEventSet(p);}
static int fail(candidate_output* out,int code,const char* message){out->code=code;snprintf(out->error,sizeof(out->error),"%s",message);return 0;}
static int hex(const char* s,size_t n){if(!s || strlen(s)!=n)return 0;for(size_t i=0;i<n;++i)if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f')))return 0;return 1;}
static void freeInput(void* data,void* user)
{(void)user;candidate_input* in=data;for(size_t i=0;i<in->count;++i){xrtFree((char*)in->docs[i].key);xrtFree((char*)in->docs[i].path);xlsTextRelease(in->docs[i].text);}xrtValueRelease(in->requestId);xrtValueRelease(in->params);xrtValueRelease(in->catalogValue);xideAnalysisCatalogDestroy(in->catalog);xrtFree(in);}
static void freeOutput(void* data,void* user){(void)user;candidate_output* out=data;free(out->frame);xrtFree(out);}
static xide_rename_scope* prepareScope(candidate_input* in,const xvalue* extension,xcancel* cancel,candidate_output* out)
{
 xls_context* files[64]={0};xide_rename_scope* scope=NULL;size_t count=0;
 const xvalue* roots=get(extension,"standaloneRoots");int claimed=0;
 if(xrtValueType(roots)!=XVALUE_ARRAY || xrtValueCount(roots)>64 || !integer(get(extension,"scopeCount"),&claimed) || claimed<=0 || claimed>64){fail(out,-32602,"Invalid rename root/scope capacity.");goto done;}
 for(size_t i=0;i<xrtValueCount(roots);++i){
  if(stopped(cancel))goto done;
  const char* path=text(xrtValueArrayGet(roots,i),262144);uint64_t owners=0;
  wchar_t* key=path && xrtPathIsAbs(path) && xidePathHasExtension(path,".xl")?xidePathNativeWide(path):NULL;
  int valid=key && xideAnalysisCatalogOwnersNative(in->catalog,key,&owners) && !owners;free(key);
  if(!valid){fail(out,-32602,"Standalone rename roots must be absolute unowned xlang files.");goto done;}
  for(size_t j=0;j<count;++j)if(xidePathLexicalEqual(path,xlsContextSpec(files[j])->entry)){fail(out,-32602,"Duplicate standalone rename root.");goto done;}
  files[count]=xideAnalysisCatalogFile(in->catalog,path);if(!files[count]){fail(out,-32603,"Cannot prepare standalone rename context.");goto done;}++count;
 }
 enum xide_rename_scope_error why;
 scope=xideRenameScopeCreate(in->catalog,files,count,0,stopped,cancel,&why);
 if(!scope || xideRenameScopeCount(scope)!=(size_t)claimed){fail(out,-32602,"Rename scope does not match the acknowledged configuration and roots.");xideRenameScopeRelease(scope);scope=NULL;goto done;}
 for(size_t i=0;i<in->count;++i){
  uint64_t owners=0;if(stopped(cancel))goto invalid;
  if(!xideRenameScopeOwners(scope,in->docs[i].path,&owners) || !owners){fail(out,-32602,"Rename scope omits an open xlang document.");goto invalid;}
 }
 goto done;
invalid:xideRenameScopeRelease(scope);scope=NULL;
done:for(size_t i=0;i<count;++i)xlsContextRelease(files[i]);return scope;
}
/* Standard requests use the captured server catalog and open standalone files,
 * never a client-supplied receipt or an inferred directory traversal. */
static int standardCatalog(candidate_input* in,candidate_output* out)
{
 if(in->catalog)return 1;
 xvalue* value=xrtValueObject();char* package=xrtEnvGet("XLANG_PACKAGE_PATH");
 int okay=value && put(value,"schema",xrtValueInt(1)) && put(value,"selectedProject",xrtValueString(xrtStrView(""))) && put(value,"packagePath",package ? xrtValueString(xrtStrView(package)) : xrtValueNull()) && put(value,"projects",xrtValueArray());
 xrtFree(package);char error[512]={0};if(okay)in->catalog=xideAnalysisCatalogCreate(value,error,sizeof(error));xrtValueRelease(value);
 return in->catalog ? 1 : fail(out,-32603,*error ? error : "Cannot prepare standalone analysis catalog.");
}
static xide_rename_scope* standardScope(candidate_input* in,xcancel* cancel,candidate_output* out)
{
 xls_context* files[64]={0};size_t count=0;xide_rename_scope* scope=NULL;
 for(size_t i=0;i<in->count;++i){
  if(stopped(cancel))goto done;
  const char* path=in->docs[i].path;uint64_t owners=0;wchar_t* key=xidePathNativeWide(path);
  int okay=key && xideAnalysisCatalogOwnersNative(in->catalog,key,&owners);free(key);
  if(!okay || !xrtPathIsAbs(path) || !xidePathHasExtension(path,".xl")){fail(out,-32602,"Invalid captured xlang source path.");goto done;}
  if(owners)continue;
  for(size_t j=0;j<count;++j)if(xidePathLexicalEqual(path,xlsContextSpec(files[j])->entry)){fail(out,-32602,"Duplicate standalone source identity.");goto done;}
  files[count]=xideAnalysisCatalogFile(in->catalog,path);if(!files[count]){fail(out,-32603,"Cannot prepare standalone source context.");goto done;}++count;
 }
 enum xide_rename_scope_error why;scope=xideRenameScopeCreate(in->catalog,files,count,0,stopped,cancel,&why);
 if(!scope)fail(out,-32803,"Cannot construct complete bounded rename scope.");
done:for(size_t i=0;i<count;++i)xlsContextRelease(files[i]);return scope;
}
static xvalue* preparePosition(xide_lsp_position p)
{xvalue* value=xrtValueObject();if(value && put(value,"line",xrtValueInt(p.line)) && put(value,"character",xrtValueInt(p.character)))return value;xrtValueRelease(value);return NULL;}
static xvalue* prepareResult(xide_reference_query* query,xcancel* cancel)
{
 xide_rename_target* target=xideProjectReferencesRetainPreparedRenameTarget(query);xide_rename_target_info info;
 xvalue *out=NULL,*range=NULL;int okay=target && !stopped(cancel) && xideRenameTargetInfo(target,&info) && info.name.Size && info.name.Size<=65535;
 if(okay){out=xrtValueObject();range=xrtValueObject();okay=out && range && put(range,"start",preparePosition(info.start)) && put(range,"end",preparePosition(info.end));}
 if(okay){okay=put(out,"range",range);range=NULL;}
 if(okay)okay=put(out,"placeholder",xrtValueString(info.name));
 xrtValueRelease(range);xideRenameTargetRelease(target);if(!okay || stopped(cancel)){xrtValueRelease(out);out=NULL;}return out;
}
static xtaskoutcome execute(xcancel* cancel,void* data,xtaskvalue* value)
{
 candidate_input* in=data;candidate_output* out=xrtCalloc(1,sizeof(*out));if(!out)return XTASK_FAILED;
 xide_rename_scope* scope=NULL;xide_reference_query* query=NULL;xevent* event=NULL;xcancelwatch* watch=NULL;
 xvalue *candidate=NULL,*result=NULL,*binding=NULL,*reply=NULL;
 const xvalue *p=in->params,*extension=get(p,"xlang"),*document=get(p,"textDocument"),*position=get(p,"position"),*receipt=get(extension,"inputState");
 const char *uri=text(get(document,"uri"),262144),*name=text(get(p,"newName"),4096),*token=text(get(extension,"token"),16),*claimed=text(get(receipt,"digest"),64);
 int version=0,line=0,character=0;int64_t schema=0,documents=0;char digest[65];size_t selected=SIZE_MAX;
 if(stopped(cancel))goto done;
 if(!in->standard && (!object(p,4) || !object(document,2) || !object(position,2) || !object(extension,6) || !object(receipt,2) || !number(get(extension,"schema"),&schema) || schema!=1 || !uri || !name || !*name || !xlsIdentifierValid(name,strlen(name)) || !hex(token,16) || !strcmp(token,"0000000000000000") || !hex(claimed,64) || !integer(get(document,"version"),&version) || !integer(get(position,"line"),&line) || !integer(get(position,"character"),&character) || line<0 || character<0 || !number(get(receipt,"documents"),&documents) || documents!=(int64_t)in->count)){fail(out,-32602,"Invalid bound native rename intent.");goto done;}
 if(in->standard && (xrtValueType(p)!=XVALUE_OBJECT || xrtValueType(document)!=XVALUE_OBJECT || xrtValueType(position)!=XVALUE_OBJECT || !uri || (!in->prepare && (!name || !*name || !xlsIdentifierValid(name,strlen(name)))) || !integer(get(position,"line"),&line) || !integer(get(position,"character"),&character) || line<0 || character<0 || (get(p,"workDoneToken") && xrtValueType(get(p,"workDoneToken"))!=XVALUE_INT && !text(get(p,"workDoneToken"),65535)))){fail(out,-32602,"Invalid standard rename parameters.");goto done;}
 if(!in->standard && !xrtValueEqual(get(extension,"catalog"),in->catalogValue)){fail(out,-32801,"Rename catalog differs from the acknowledged configuration.");goto done;}
 xls_input_state_item items[64];
 for(size_t i=0;i<in->count;++i){
  if(stopped(cancel))goto done;
  const xls_analysis_document* d=in->docs+i;
  if(d->revision>UINT32_MAX || !xlsInputStateUri(d->key,items[i].uri)){fail(out,-32602,"Invalid captured document identity.");goto done;}
  items[i].version=(int32_t)((int64_t)d->revision+INT_MIN);
  if(!strcmp(d->key,uri)){if(selected!=SIZE_MAX){fail(out,-32602,"Duplicate captured document URI.");goto done;}selected=i;}
 }
 if(!in->standard && (!xlsInputStateDigest(items,in->count,digest) || strcmp(digest,claimed))){fail(out,-32801,"Rename input receipt differs from the online snapshot.");goto done;}
 if(selected==SIZE_MAX || (!in->standard && (int64_t)in->docs[selected].revision+INT_MIN!=version)){fail(out,-32801,"Rename needs the exact current open document version.");goto done;}
 size_t offset=0;if(!xlsTextOffset(in->docs[selected].text,(xls_position){(size_t)line,(size_t)character},&offset)){fail(out,-32602,"Rename cursor is not a scalar UTF16 position.");goto done;}
 if(in->standard && !standardCatalog(in,out))goto done;
 scope=in->standard ? standardScope(in,cancel,out) : prepareScope(in,extension,cancel,out);if(!scope)goto done;
 const xls_context* context=NULL;xls_context* standalone=NULL;int state=xideAnalysisCatalogFind(in->catalog,in->docs[selected].path,&context);
 if(state==XIDE_ANALYSIS_UNOWNED){standalone=xideAnalysisCatalogFile(in->catalog,in->docs[selected].path);context=standalone;}
 if((state!=XIDE_ANALYSIS_PROJECT && state!=XIDE_ANALYSIS_UNOWNED) || !context){xlsContextRelease(standalone);fail(out,-32602,"Rename needs an unambiguous acknowledged source owner.");goto done;}
 query=in->prepare ? xideProjectReferencesSubmitPrepareRenameAt(in->service->references,scope,(xls_context*)context,in->docs,in->count,uri,in->docs[selected].path,line,character) : xideProjectReferencesSubmitRenameAt(in->service->references,scope,(xls_context*)context,in->docs,in->count,uri,in->docs[selected].path,line,character,xrtStrView(name));xlsContextRelease(standalone);
 event=xrtEventCreate(false,false);if(!query || !event || !xideProjectReferencesSetWake(query,wakeEvent,event)){fail(out,-32603,"Cannot start complete native rename analysis.");goto done;}
 watch=xrtCancelWatch(cancel,wakeEvent,event);
 char error[512]={0};enum xide_reference_query_state status;
 while((status=xideProjectReferencesResult(query,NULL,NULL,error,sizeof(error)))==XIDE_REFERENCES_PENDING && !stopped(cancel))xrtEventWaitFor(event,XIDE_XRT_TIMEOUT_MS(10));
 if(stopped(cancel))goto done;
 if(status!=XIDE_REFERENCES_READY){fail(out,status==XIDE_REFERENCES_CANCELLED?-32800:-32803,*error?error:"Native rename analysis did not complete.");goto done;}
 xide_model_error why={0};
 if(in->standard){
  if(in->prepare)result=prepareResult(query,cancel);
  else{xide_rename_plan* plan=xideProjectReferencesRetainRenamePlan(query);result=xideRenamePlanWorkspaceEditFormat(plan,in->documentChanges,0,stopped,cancel);xideRenamePlanRelease(plan);}
  reply=xrtValueObject();int okay=result && reply && put(reply,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && put(reply,"id",xrtValueRetain(in->requestId));
  if(okay){okay=put(reply,"result",result);result=NULL;}
  if(okay && !stopped(cancel))out->frame=xideLspFrame(reply,&out->bytes,&why);
  if(!out->frame)fail(out,-32603,"Cannot frame the bounded standard rename reply.");
  else if(!(in->prepare ? xideProjectReferencesPrepareRenameValidate(query,stopped,cancel,&why) : xideProjectReferencesRenameValidate(query,stopped,cancel,&why))){free(out->frame);out->frame=NULL;out->bytes=0;fail(out,-32801,"Rename inputs changed after response framing.");}
  goto done;
 }
 candidate=xideProjectReferencesRenameResult(query,0,stopped,cancel,&why);
 if(!candidate){fail(out,-32803,*why.message?why.message:"Native rename proof export failed.");goto done;}
 binding=xrtValueObject();result=xrtValueObject();reply=xrtValueObject();
 int okay=binding && result && reply && put(binding,"method",xrtValueString(xrtStrView(XIDE_NATIVE_RENAME_METHOD))) && put(binding,"token",xrtValueString(xrtStrView(token))) && put(binding,"inputDigest",xrtValueString(xrtStrView(digest))) && put(binding,"documents",xrtValueInt((int64_t)in->count)) && put(result,"schema",xrtValueInt(1));
 if(okay){okay=put(result,"binding",binding);binding=NULL;}if(okay){okay=put(result,"candidate",candidate);candidate=NULL;}
 if(okay)okay=put(reply,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && put(reply,"id",xrtValueInt(in->id));
 if(okay){okay=put(reply,"result",result);result=NULL;}
 if(okay && !stopped(cancel))out->frame=xideLspFrame(reply,&out->bytes,&why);
 if(!out->frame)fail(out,-32603,"Cannot frame the bounded native rename reply.");
 else if(!xideProjectReferencesRenameValidate(query,stopped,cancel,&why)){free(out->frame);out->frame=NULL;out->bytes=0;fail(out,-32801,"Native rename inputs changed after response framing.");}
done:
 if(watch)xrtCancelUnwatch(watch);
 if(query){if(out->code || stopped(cancel))xideProjectReferencesCancel(query);xideProjectReferencesQueryDestroy(query);}if(event)xrtEventDestroy(event);
 xideRenameScopeRelease(scope);xrtValueRelease(candidate);xrtValueRelease(binding);xrtValueRelease(result);xrtValueRelease(reply);
 if(stopped(cancel)){freeOutput(out,NULL);return XTASK_CANCELLED;}
 if(!out->frame && !out->code)fail(out,-32603,"Native rename orchestration failed.");
 *value=(xtaskvalue){out,freeOutput,NULL};return XTASK_SUCCESS;
}
xide_rename_candidates* xideRenameCandidatesCreate(xide_project_references* references)
{if(!references)return NULL;xide_rename_candidates* s=xrtCalloc(1,sizeof(*s));if(!s)return NULL;s->references=references;xtaskpoolconfig config={.Threads=1,.QueueLimit=4};s->pool=xrtTaskPoolCreate(&config);if(!s->pool){xrtFree(s);return NULL;}return s;}
void xideRenameCandidatesDestroy(xide_rename_candidates* s){if(s){xrtTaskPoolCancel(s->pool);xrtTaskPoolDestroy(s->pool);xrtFree(s);}}
int xideRenameCandidatesIdle(xide_rename_candidates* s){return !s || xlsRuntimeTaskPoolIdle(s->pool);}
static xide_rename_candidate_query* submit(xide_rename_candidates* s,int id,const xvalue* requestId,int documentChanges,int prepare,const xvalue* params,const xvalue* catalogValue,xide_analysis_catalog* catalog,const xls_analysis_document* docs,size_t count)
{
 int standard=requestId!=NULL;if(!s || (!standard && (id<=0 || !catalogValue || !catalog)) || !params || !docs || !count || count>64 || (documentChanges!=0 && documentChanges!=1) || (prepare!=0 && prepare!=1) || (prepare && !standard))return NULL;
 if(standard && xrtValueType(requestId)!=XVALUE_INT && !text(requestId,65535))return NULL;
 candidate_input* in=xrtCalloc(1,sizeof(*in));xide_rename_candidate_query* q=xrtCalloc(1,sizeof(*q));if(!in || !q){xrtFree(in);xrtFree(q);return NULL;}
 in->service=s;in->id=id;in->standard=standard;in->documentChanges=documentChanges;in->prepare=prepare;in->requestId=xrtValueRetain(requestId);in->params=xrtValueRetain(params);in->catalogValue=xrtValueRetain(catalogValue);in->catalog=xideAnalysisCatalogRetain(catalog);size_t bytes=0;int okay=in->params && (standard ? in->requestId && (!catalog || in->catalog) : in->catalogValue && in->catalog);
 for(size_t i=0;okay && i<count;++i){
  const xls_analysis_document* d=docs+i;size_t a=d->key?strlen(d->key):0,b=d->path?strlen(d->path):0;
  okay=a && b && a<=262144 && b<=262144 && a+b+2<=2u*1024u*1024u-bytes && d->text && d->identity && d->revision<=UINT32_MAX;
  if(!okay)break;
  in->docs[i]=*d;in->docs[i].key=xrtStrDup(d->key);in->docs[i].path=xrtStrDup(d->path);in->docs[i].text=xlsTextRetain(d->text);++in->count;bytes+=a+b+2;okay=in->docs[i].key && in->docs[i].path && in->docs[i].text;
 }
 xtaskargs args={.Destroy=freeInput};if(okay)q->future=xrtTaskSubmit(s->pool,execute,in,&args);if(!q->future){freeInput(in,NULL);xrtFree(q);return NULL;}return q;
}
xide_rename_candidate_query* xideRenameCandidateSubmit(xide_rename_candidates* s,int id,const xvalue* params,const xvalue* catalogValue,xide_analysis_catalog* catalog,const xls_analysis_document* docs,size_t count)
{return submit(s,id,NULL,1,0,params,catalogValue,catalog,docs,count);}
xide_rename_candidate_query* xideRenameStandardSubmit(xide_rename_candidates* s,const xvalue* id,int documentChanges,const xvalue* params,xide_analysis_catalog* catalog,const xls_analysis_document* docs,size_t count)
{return id ? submit(s,0,id,documentChanges,0,params,NULL,catalog,docs,count) : NULL;}
xide_rename_candidate_query* xideRenamePrepareSubmit(xide_rename_candidates* s,const xvalue* id,const xvalue* params,xide_analysis_catalog* catalog,const xls_analysis_document* docs,size_t count)
{return id ? submit(s,0,id,1,1,params,NULL,catalog,docs,count) : NULL;}
static void wakeQuery(void* p){xide_rename_candidate_query* q=p;q->wake(q->user);}
int xideRenameCandidateSetWake(xide_rename_candidate_query* q,void (*wake)(void*),void* user)
{if(!q || !wake || q->initialized)return 0;q->wake=wake;q->user=user;if(!xrtFutureWatchInit(&q->watch,wakeQuery,NULL,q))return 0;q->initialized=1;xfuturewatchresult result=xrtFutureWatchAdd(q->future,&q->watch);if(result==XFUTURE_WATCH_ERROR)return 0;q->linked=result==XFUTURE_WATCH_PENDING;if(result==XFUTURE_WATCH_READY)wake(user);return 1;}
void xideRenameCandidateCancel(xide_rename_candidate_query* q){if(q)xrtFutureCancel(q->future);}
void xideRenameCandidateDestroy(xide_rename_candidate_query* q){if(q){if(q->linked)xrtFutureWatchRemove(q->future,&q->watch);xrtFutureDestroy(q->future);xrtFree(q);}}
int xideRenameCandidateResult(xide_rename_candidate_query* q,int* code,char* error,size_t capacity)
{
 if(code)*code=0;
 if(error && capacity)*error=0;
 if(!q)return -1;
 xfuturestate state=xrtFutureState(q->future);if(state==XFUTURE_PENDING)return 0;
 const candidate_output* out=state==XFUTURE_RESOLVED?xrtFutureValue(q->future):NULL;
 if(out && out->frame && !out->code)return 1;
 if(code)*code=out?out->code:state==XFUTURE_CANCELLED?-32800:-32603;
 if(error && capacity)snprintf(error,capacity,"%s",out?out->error:state==XFUTURE_CANCELLED?"Rename request cancelled.":"Rename orchestration failed.");
 return -1;
}
char* xideRenameCandidateTakeFrame(xide_rename_candidate_query* q,size_t* bytes)
{if(bytes)*bytes=0;if(!bytes || xideRenameCandidateResult(q,NULL,NULL,0)!=1)return NULL;candidate_output* out=xrtFutureValue(q->future);char* frame=out->frame;*bytes=out->bytes;out->frame=NULL;out->bytes=0;return frame;}
#endif
