#include "services/workspace_search_result.h"
#include "services/reference_results.h"
#include "services/language_check.h"
#include "protocol/workspace_symbol_selection.h"
#include "platform/path.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <wchar.h>
static const xvalue* field(const xvalue* o,const char* k){return xrtValueType(o)==XVALUE_OBJECT?xrtValueObjectGet(o,xrtStrView(k)):NULL;}
static int integer(const xvalue* v,int* out){int64 n=0;if(xrtValueType(v)!=XVALUE_INT || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX)return 0;*out=(int)n;return 1;}
static const char* string(const xvalue* v){xstrview s={0};return xrtValueType(v)==XVALUE_STRING && xrtValueGetString(v,&s) && s.Size<=262144 && !memchr(s.Data,0,s.Size) && xrtUtf8Valid(s,NULL)?s.Data:NULL;}
static int stopped(int (*stop)(void*),void* p){return stop && stop(p);}
static void shortText(char* out,size_t capacity,const char* text,size_t scalars)
{size_t used=0,remaining=text?strlen(text):0;while(remaining && scalars--){uint32 scalar=0;size_t n=0;if(xrtUtf8Decode((xstrview){text,remaining<4?remaining:4},&scalar,&n)!=XUTF_OK || !n || n>=capacity-used)break;memcpy(out+used,text,n);used+=n;text+=n;remaining-=n;}out[used]=0;}
static int itemDecode(xide_workspace_search_item* item,const xvalue* symbol,size_t* bytes)
{
 if(!xideWorkspaceSymbolSelectionValid(symbol))return 0;
 item->symbol=symbol;const xvalue* location=field(symbol,"location");item->range=field(location,"range");item->path=xideLspUriToPath(string(field(location,"uri")),NULL);item->key=item->path?xidePathNativeWide(item->path):NULL;
 if(!item->path || !item->key || !integer(field(field(item->range,"start"),"line"),&item->start.line) || !integer(field(field(item->range,"start"),"character"),&item->start.character) || !integer(field(field(item->range,"end"),"line"),&item->end.line) || !integer(field(field(item->range,"end"),"character"),&item->end.character))return 0;
 char name[520],container[264],file[520];shortText(name,sizeof(name),string(field(symbol,"name")),128);shortText(container,sizeof(container),string(field(symbol,"containerName")),64);shortText(file,sizeof(file),xidePathFilename(item->path),128);
 size_t add=strlen(item->path)+1+(wcslen(item->key)+1)*sizeof(wchar_t)+sizeof(name)+sizeof(container)+sizeof(file)+64;
 if(*bytes>32u*1024u*1024u || add>32u*1024u*1024u-*bytes)return 0;
 *bytes+=add;
 item->label=malloc(sizeof(name)+sizeof(container)+sizeof(file)+64);if(!item->label)return 0;
 snprintf(item->label,sizeof(name)+sizeof(container)+sizeof(file)+64,"%s%s%s  ·  %s  (%u:%u)",name,*container?"  —  ":"",container,file,(unsigned)item->start.line+1,(unsigned)item->start.character+1);return 1;
}
void xideWorkspaceSearchResultRelease(xide_workspace_search_result* r)
{if(!r)return;for(size_t i=0;i<r->count;++i){free(r->items[i].path);free(r->items[i].key);free(r->items[i].label);}xrtFree(r->items);xrtValueRelease(r->message);xideTextFileClear(&r->snapshot);xrtFree(r);}
static int matches(const char* text,const xvalue* document)
{int size=0;xstrview expected={0};unsigned char hash[32];char hex[65];if(!text || !integer(field(document,"normalizedSize"),&size) || (size_t)size!=strlen(text) || !xrtValueGetString(field(document,"normalizedSha256"),&expected) || expected.Size!=64 || !xrtSha256(text,(size_t)size,hash))return 0;for(size_t i=0;i<32;++i)snprintf(hex+2*i,3,"%02x",hash[i]);return !memcmp(hex,expected.Data,64);}
xide_workspace_search_result* xideWorkspaceSearchDecode(xvalue* message,const xvalue* selection,const xvalue* text,int version,int (*stop)(void*),void* user)
{
 xide_workspace_search_result* r=xrtCalloc(1,sizeof(*r));if(!r){xrtValueRelease(message);return NULL;}r->message=message;r->navigation=selection!=NULL;r->workerThread=xrtThreadCurrentId();snprintf(r->error,sizeof(r->error),"符号结果无效、已取消或超过资源限制。");
 const char* rpc=string(field(message,"jsonrpc"));const xvalue *result=field(message,"result"),*error=field(message,"error");int id=0;
 if(!rpc || strcmp(rpc,"2.0") || field(message,"method") || !integer(field(message,"id"),&id) || id<=1 || stopped(stop,user))return r;
 if(error){const char* reason=string(field(error,"message"));if(reason)shortText(r->error,sizeof(r->error),reason,120);return r;}if(!result)return r;
 size_t count=selection?1:xrtValueCount(result),bytes=0;
 if(!selection && xrtValueType(result)==XVALUE_NULL){r->valid=1;r->error[0]=0;return r;}
 if((selection && (xrtValueType(result)!=XVALUE_OBJECT || xrtValueCount(result)!=2 || !xrtValueEqual(field(result,"symbol"),selection))) || (!selection && (xrtValueType(result)!=XVALUE_ARRAY || count>200)))return r;
 r->items=xrtCalloc(count?count:1,sizeof(*r->items));if(!r->items)return r;
 for(size_t i=0;i<count;++i){r->count=i+1;if(stopped(stop,user) || !itemDecode(r->items+i,selection?field(result,"symbol"):xrtValueArrayGet(result,i),&bytes))return r;}
 if(selection){xide_workspace_search_item* item=r->items;item->document=field(result,"document");if(!xideReferenceDocumentValid(item->document,item->path))return r;
  const char* origin=string(field(item->document,"origin"));const char* body=NULL;xstrview overlay={0};
  if(origin && !strcmp(origin,"overlay")){int64 actual=0;if(!text || !xrtValueGetString(text,&overlay) || !xrtValueGetInt(field(item->document,"version"),&actual) || actual!=version)return r;body=overlay.Data;}
  else if(origin && !strcmp(origin,"disk")){xide_model_error e={0};if(text || !xideLanguageReadDiskSnapshot(item->path,field(item->document,"input"),stop,user,&r->snapshot,&e)){if(*e.message)shortText(r->error,sizeof(r->error),e.message,120);return r;}body=r->snapshot.text;}
  else return r;
  xide_lsp_position positions[]={item->start,item->end};if(stopped(stop,user) || !matches(body,item->document) || !xideLspPositionsToOffsets(body,positions,2) || positions[1].offset<=positions[0].offset)return r;item->start=positions[0];item->end=positions[1];
 }
 if(!stopped(stop,user)){r->valid=1;r->error[0]=0;}return r;
}
typedef struct search_job {uint64_t token;int request,version,prepared,failed;size_t bytes;xvalue *selection,*text,*message;xide_workspace_search_result* emergency;xide_language_batch* batch;char query[513];} search_job;
struct xide_workspace_search_decoder {xthread* thread;xmutex lock;xcond changed;int stop,request,received;uint64_t current;search_job* pending;xide_workspace_search_result *completed,*retired;xide_language_batch* retiredBatch;void (*wake)(void*);void* user;};
typedef struct cancel_state {xide_workspace_search_decoder* d;uint64_t token;} cancel_state;
static int cancelled(void* p){cancel_state* c=p;xrtMutexLock(&c->d->lock);int stop=c->d->stop || c->d->current!=c->token;xrtMutexUnlock(&c->d->lock);return stop;}
static void jobFree(search_job* j){if(j){xrtValueRelease(j->selection);xrtValueRelease(j->text);xrtValueRelease(j->message);xideWorkspaceSearchResultRelease(j->emergency);xideLanguageBatchDestroy(j->batch);xrtFree(j);}}
static xide_language_batch* prepare(search_job* j)
{
 xide_language_batch* batch=xideLanguageBatchCreate();xvalue* p=xrtValueObject();int okay=batch && p && xrtValueObjectSetNew(p,xrtStrView("query"),xrtValueString(xrtStrView(j->query))) && xrtValueObjectSetNew(p,xrtStrView("xlangLimit"),xrtValueInt(200));
 if(okay && j->selection)okay=xrtValueObjectSet(p,xrtStrView("symbol"),j->selection);
 if(okay)okay=xideLanguageBatchAppendRequest(batch,j->selection?"xlang/workspaceSymbolNavigation":"workspace/symbol",p,j->request);
 xrtValueRelease(p);if(!okay){xideLanguageBatchDestroy(batch);batch=NULL;}return batch;
}
static int32 run(ptr p)
{
 xide_workspace_search_decoder* d=p;
 for(;;){xrtMutexLock(&d->lock);while(!d->stop && !d->retired && !d->retiredBatch && !(d->completed && d->completed->token!=d->current) && !(d->pending && (!d->pending->prepared || d->pending->failed || d->pending->message || d->pending->token!=d->current)))xrtCondWait(&d->changed,&d->lock);
  if(d->stop){xrtMutexUnlock(&d->lock);break;}xide_workspace_search_result* garbage=d->retired;d->retired=NULL;search_job* job=NULL;
  xide_language_batch* batchGarbage=d->retiredBatch;d->retiredBatch=NULL;
  if(!garbage && d->completed && d->completed->token!=d->current){garbage=d->completed;d->completed=NULL;}
  if(!garbage && !batchGarbage && d->pending && !d->pending->prepared && d->pending->token==d->current){job=d->pending;xrtMutexUnlock(&d->lock);xide_language_batch* batch=prepare(job);xrtMutexLock(&d->lock);job->batch=batch;job->prepared=1;job->failed=batch==NULL;xrtMutexUnlock(&d->lock);if(d->wake)d->wake(d->user);continue;}
  if(!garbage && !batchGarbage){job=d->pending;d->pending=NULL;}xrtMutexUnlock(&d->lock);xideLanguageBatchDestroy(batchGarbage);
  xideWorkspaceSearchResultRelease(garbage);if(!job){if(d->wake)d->wake(d->user);continue;}
  cancel_state c={d,job->token};xide_workspace_search_result* result=NULL;if(job->message && job->bytes<=XIDE_LSP_BODY_BYTES && !cancelled(&c)){xvalue* message=job->message;job->message=NULL;result=xideWorkspaceSearchDecode(message,job->selection,job->text,job->version,cancelled,&c);}
  if(!result && !cancelled(&c)){result=job->emergency;job->emergency=NULL;snprintf(result->error,sizeof(result->error),"无法分配符号结果或响应超过限制。");result->navigation=job->selection!=NULL;result->workerThread=xrtThreadCurrentId();}
  uint64_t token=job->token;jobFree(job);xrtMutexLock(&d->lock);if(result && !d->stop && d->current==token){result->token=token;garbage=d->completed;d->completed=result;result=NULL;}else garbage=NULL;xrtMutexUnlock(&d->lock);xideWorkspaceSearchResultRelease(garbage);xideWorkspaceSearchResultRelease(result);if(d->wake)d->wake(d->user);
 }jobFree(d->pending);xideWorkspaceSearchResultRelease(d->completed);xideWorkspaceSearchResultRelease(d->retired);xideLanguageBatchDestroy(d->retiredBatch);return 0;
}
xide_workspace_search_decoder* xideWorkspaceSearchDecoderCreate(void (*wake)(void*),void* user)
{xide_workspace_search_decoder* d=xrtCalloc(1,sizeof(*d));if(!d)return NULL;if(!xrtMutexInit(&d->lock)){xrtFree(d);return NULL;}if(!xrtCondInit(&d->changed)){xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}d->wake=wake;d->user=user;d->thread=xrtThreadCreate(run,d,0);if(!d->thread){xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}return d;}
int xideWorkspaceSearchDecoderArm(xide_workspace_search_decoder* d,uint64_t token,int request,const char* query,const xvalue* selection,const xvalue* text,int version)
{if(!d || !token || request<=1 || !query || strlen(query)>512)return 0;search_job* j=xrtCalloc(1,sizeof(*j));if(!j)return 0;*j=(search_job){.token=token,.request=request,.version=version,.selection=xrtValueRetain((xvalue*)selection),.text=xrtValueRetain((xvalue*)text),.emergency=xrtCalloc(1,sizeof(*j->emergency))};memcpy(j->query,query,strlen(query)+1);if(!j->emergency){jobFree(j);return 0;}xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending;if(okay){d->current=token;d->request=request;d->received=0;d->pending=j;j=NULL;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);jobFree(j);return okay;}
xide_language_batch* xideWorkspaceSearchDecoderTakeBatch(xide_workspace_search_decoder* d,uint64_t token)
{if(!d)return NULL;xrtMutexLock(&d->lock);xide_language_batch* batch=d->current==token && d->pending && d->pending->prepared?d->pending->batch:NULL;if(batch)d->pending->batch=NULL;xrtMutexUnlock(&d->lock);return batch;}
int xideWorkspaceSearchDecoderRetireBatch(xide_workspace_search_decoder* d,xide_language_batch* batch)
{if(!d || !batch)return 0;xrtMutexLock(&d->lock);int okay=!d->stop && !d->retiredBatch;if(okay){d->retiredBatch=batch;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;}
int xideWorkspaceSearchDecoderSubmit(xide_workspace_search_decoder* d,uint64_t token,xvalue* message,size_t bytes)
{if(!d || !message)return 0;int id=0;if(!integer(field(message,"id"),&id))return 0;xrtMutexLock(&d->lock);int take=!d->stop && token==d->current && id==d->request && d->pending && !d->pending->message && !d->received;if(take){d->pending->message=message;d->pending->bytes=bytes;d->received=1;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return take;}
int xideWorkspaceSearchDecoderReceive(xvalue* message,size_t bytes,void* p)
{xide_workspace_search_decoder* d=p;int id=0;const char* rpc=string(field(message,"jsonrpc"));if(!d || !rpc || strcmp(rpc,"2.0") || field(message,"method") || !integer(field(message,"id"),&id))return 0;xrtMutexLock(&d->lock);int match=!d->stop && d->current && id==d->request;int duplicate=match && d->received;int take=match && !duplicate && d->pending && !d->pending->message;if(take){d->pending->message=message;d->pending->bytes=bytes;d->received=1;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);if(duplicate)xrtValueRelease(message);return take || duplicate;}
void xideWorkspaceSearchDecoderCancel(xide_workspace_search_decoder* d){if(d){xrtMutexLock(&d->lock);d->current=0;d->request=0;d->received=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);}}
xide_workspace_search_result* xideWorkspaceSearchDecoderTake(xide_workspace_search_decoder* d){if(!d)return NULL;xrtMutexLock(&d->lock);xide_workspace_search_result* r=d->completed;if(r)d->completed=NULL;xrtMutexUnlock(&d->lock);return r;}
int xideWorkspaceSearchDecoderRetire(xide_workspace_search_decoder* d,xide_workspace_search_result* r){if(!d || !r)return 0;xrtMutexLock(&d->lock);int okay=!d->stop && !d->retired;if(okay){d->retired=r;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;}
void xideWorkspaceSearchDecoderDestroy(xide_workspace_search_decoder* d){if(!d)return;xrtMutexLock(&d->lock);d->stop=1;d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);xrtThreadWait(d->thread);xrtThreadDestroy(d->thread);xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);}
