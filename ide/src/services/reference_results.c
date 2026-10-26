#include "services/reference_results.h"
#include "platform/path.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#define ROW_LIMIT 65536u
#define STORAGE_LIMIT (32u*1024u*1024u)
static const xvalue* field(const xvalue* o,const char* key){return o && xrtValueType(o)==XVALUE_OBJECT ? xrtValueObjectGet(o,xrtStrView(key)) : NULL;}
static const char* string(const xvalue* v,size_t limit)
{xstrview s={0};return v && xrtValueGetString(v,&s) && s.Size && s.Size<=limit && !memchr(s.Data,0,s.Size) ? s.Data : NULL;}
static int integer(const xvalue* v,int* out)
{int64 n=0;if(!v || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX)return 0;*out=(int)n;return 1;}
static int position(const xvalue* v,xide_lsp_position* p){return integer(field(v,"line"),&p->line) && integer(field(v,"character"),&p->character);}
static int wireVersion(const xvalue* v){int64 n=0;return v && xrtValueGetInt(v,&n) && n>=INT_MIN && n<=INT_MAX;}
static int hex(const xvalue* v)
{const char* s=string(v,64);if(!s || strlen(s)!=64)return 0;for(size_t i=0;i<64;++i)if(!((s[i]>='0' && s[i]<='9') || (s[i]>='a' && s[i]<='f')))return 0;return 1;}
static int proof(const xvalue* v,const char* path,const xvalue** document)
{
 int schema=0,size=0;const xvalue* d=field(v,"document");const char *declared=string(field(d,"path"),65535),*origin=string(field(d,"origin"),16);
 if(!integer(field(v,"schema"),&schema) || schema!=1 || !declared || !xidePathLexicalEqual(path,declared) || !origin || !integer(field(d,"normalizedSize"),&size) || size>8*1024*1024 || !hex(field(d,"normalizedSha256")))return 0;
 const xvalue* input=field(d,"input");
 if(!strcmp(origin,"overlay")){if(input || !wireVersion(field(d,"version")))return 0;}
 else if(!strcmp(origin,"disk")){const char *file=string(field(input,"path"),65535),*kind=string(field(input,"origin"),16);if(field(d,"version") || !file || !xidePathLexicalEqual(path,file) || !kind || strcmp(kind,"disk") || !integer(field(input,"rawSize"),&size) || size>8*1024*1024 || !hex(field(input,"rawSha256")))return 0;}
 else return 0;
 *document=d;return 1;
}
static int compare(const void* a,const void* b)
{const xide_reference_item *x=a,*y=b;int n=xidePathNativeKeyCompare(x->key,y->key);if(!n)n=(x->start.line>y->start.line)-(x->start.line<y->start.line);if(!n)n=(x->start.character>y->start.character)-(x->start.character<y->start.character);if(!n)n=(x->end.line>y->end.line)-(x->end.line<y->end.line);return n ? n : (x->end.character>y->end.character)-(x->end.character<y->end.character);}
static void freeItem(xide_reference_item* item){free(item->path);free(item->key);free(item->label);memset(item,0,sizeof(*item));}
size_t xideReferenceResultFind(const xide_reference_result* result,const wchar_t* key,xide_lsp_position start,xide_lsp_position end)
{if(!result || !result->valid || !key)return SIZE_MAX;xide_reference_item wanted={.key=(wchar_t*)key,.start=start,.end=end};size_t lo=0,hi=result->count;while(lo<hi){size_t mid=lo+(hi-lo)/2;if(compare(result->items+mid,&wanted)<0)lo=mid+1;else hi=mid;}return lo<result->count && !compare(result->items+lo,&wanted) ? lo : SIZE_MAX;}
void xideReferenceResultRelease(xide_reference_result* result)
{if(!result)return;for(size_t i=0;i<result->count;++i)freeItem(result->items+i);xrtFree(result->items);xrtValueRelease(result->message);xrtFree(result);}
static int charge(size_t* size,size_t add){if(*size>STORAGE_LIMIT || add>STORAGE_LIMIT-*size)return 0;*size+=add;return 1;}
static void decode(xide_reference_result* out,int (*cancelled)(void*),void* user)
{
 const char* rpc=string(field(out->message,"jsonrpc"),16);int id=0;
 const xvalue* value=field(out->message,"result");if(!rpc || strcmp(rpc,"2.0") || field(out->message,"method") || !integer(field(out->message,"id"),&id) || !id || field(out->message,"error") || !value || (cancelled && cancelled(user)))return;
 if(xrtValueType(value)==XVALUE_NULL){out->valid=1;return;}if(xrtValueType(value)!=XVALUE_ARRAY || xrtValueCount(value)>ROW_LIMIT)return;
 size_t count=xrtValueCount(value),bytes=count*sizeof(xide_reference_item);if(bytes>STORAGE_LIMIT)return;
 out->items=xrtCalloc(count?count:1,sizeof(*out->items));if(!out->items)return;
 for(size_t i=0;i<count;++i){if(cancelled && cancelled(user))return;const xvalue* row=xrtValueArrayGet(value,i);xide_reference_item* item=out->items+i;out->count=i+1;const char* uri=string(field(row,"uri"),262144);item->range=field(row,"range");
  if(!uri || !position(field(item->range,"start"),&item->start) || !position(field(item->range,"end"),&item->end) || item->end.line<item->start.line || (item->end.line==item->start.line && item->end.character<=item->start.character))return;
  item->path=xideLspUriToPath(uri,NULL);if(!item->path || !proof(field(row,"xlang"),item->path,&item->document))return;item->key=xidePathNativeWide(item->path);if(!item->key)return;
  size_t length=strlen(item->path);if(!charge(&bytes,length*2+66+(wcslen(item->key)+1)*sizeof(wchar_t)))return;item->label=malloc(length+65);if(!item->label)return;const char* name=xidePathFilename(item->path);snprintf(item->label,length+65,"%s  (%u:%u)  %.*s",name,(unsigned)item->start.line+1,(unsigned)item->start.character+1,(int)(name-item->path),item->path);
 }
 qsort(out->items,count,sizeof(*out->items),compare);
 size_t used=0;for(size_t i=0;i<count;++i){if(used && !compare(out->items+used-1,out->items+i)){freeItem(out->items+i);continue;}if(i!=used){out->items[used]=out->items[i];memset(out->items+i,0,sizeof(*out->items));}++used;}out->count=used;
 if(!cancelled || !cancelled(user))out->valid=1;
}
xide_reference_result* xideReferenceDecode(xvalue* message,int (*cancelled)(void*),void* user)
{xide_reference_result* out=xrtCalloc(1,sizeof(*out));if(!out){xrtValueRelease(message);return NULL;}out->message=message;decode(out,cancelled,user);return out;}
typedef struct decode_job {uint64_t token;xvalue* message;} decode_job;
struct xide_reference_decoder {xthread* thread;xmutex lock;xcond changed;int stop,requestId,received;uint64_t current,receiveCount;decode_job pending;xide_reference_result *completed,*retired;void (*wake)(void*);void* user;};
typedef struct cancel_state {xide_reference_decoder* decoder;uint64_t token;} cancel_state;
static int stopped(void* user){cancel_state* state=user;xrtMutexLock(&state->decoder->lock);int stop=state->decoder->stop || state->decoder->current!=state->token;xrtMutexUnlock(&state->decoder->lock);return stop;}
static int32 run(ptr user)
{
 xide_reference_decoder* decoder=user;
 for(;;){xrtMutexLock(&decoder->lock);while(!decoder->stop && !decoder->pending.message && !decoder->retired && !(decoder->completed && decoder->completed->token!=decoder->current))xrtCondWait(&decoder->changed,&decoder->lock);
  xide_reference_result *retired=decoder->retired,*obsolete=NULL;decoder->retired=NULL;int stop=decoder->stop;
  if(decoder->completed && (stop || decoder->completed->token!=decoder->current)){obsolete=decoder->completed;decoder->completed=NULL;}
  decode_job job=decoder->pending;memset(&decoder->pending,0,sizeof(decoder->pending));xrtMutexUnlock(&decoder->lock);xideReferenceResultRelease(retired);xideReferenceResultRelease(obsolete);
  xide_reference_result* result=NULL;
  if(job.message){cancel_state state={decoder,job.token};if(!stop && job.token && !stopped(&state))result=xideReferenceDecode(job.message,stopped,&state);else xrtValueRelease(job.message);}
  xrtMutexLock(&decoder->lock);int publish=result && !decoder->stop && decoder->current==job.token;
  if(publish){result->token=job.token;obsolete=decoder->completed;decoder->completed=result;result=NULL;}else obsolete=NULL;
  xrtMutexUnlock(&decoder->lock);xideReferenceResultRelease(obsolete);xideReferenceResultRelease(result);if(decoder->wake)decoder->wake(decoder->user);if(stop)break;
 }return 0;
}
xide_reference_decoder* xideReferenceDecoderCreate(void (*wake)(void*),void* user)
{
 xide_reference_decoder* d=xrtCalloc(1,sizeof(*d));if(!d)return NULL;if(!xrtMutexInit(&d->lock)){xrtFree(d);return NULL;}if(!xrtCondInit(&d->changed)){xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}d->wake=wake;d->user=user;d->thread=xrtThreadCreate(run,d,0);if(!d->thread){xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}return d;
}
int xideReferenceDecoderSubmit(xide_reference_decoder* d,uint64_t token,xvalue* message)
{if(!d || !message)return 0;xrtMutexLock(&d->lock);int id=0,matched=token && integer(field(message,"id"),&id) && id==d->requestId;int okay=!d->stop && !d->pending.message && !(matched && d->received);if(okay){if(token){d->current=token;if(matched)d->received=1;}d->pending=(decode_job){token,message};xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;}
int xideReferenceDecoderArm(xide_reference_decoder* d,uint64_t token,int requestId)
{if(!d || !token || requestId<=0)return 0;xrtMutexLock(&d->lock);int okay=!d->stop;if(okay){d->current=token;d->requestId=requestId;d->received=0;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;}
int xideReferenceDecoderReceive(xvalue* message,size_t bytes,void* user)
{
 (void)bytes;xide_reference_decoder* d=user;int id=0;const char* rpc=string(field(message,"jsonrpc"),16);if(!d || !rpc || strcmp(rpc,"2.0") || field(message,"method") || !integer(field(message,"id"),&id) || !id)return 0;
 xrtMutexLock(&d->lock);int match=!d->stop && d->current && id==d->requestId;int duplicate=match && d->received;int take=match && !duplicate && !d->pending.message;
 if(take){d->received=1;++d->receiveCount;d->pending=(decode_job){d->current,message};xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);
 /* A duplicate is not a new result. Its caller is the transport worker,so
    disposal also stays off the GUI and cannot overwrite a completed answer. */
 if(duplicate)xrtValueRelease(message);
 return take || duplicate;
}
int xideReferenceDecoderHasResult(xide_reference_decoder* d)
{if(!d)return 0;xrtMutexLock(&d->lock);int ready=d->completed && d->completed->token==d->current;xrtMutexUnlock(&d->lock);return ready;}
uint64_t xideReferenceDecoderReceived(xide_reference_decoder* d)
{if(!d)return 0;xrtMutexLock(&d->lock);uint64_t count=d->receiveCount;xrtMutexUnlock(&d->lock);return count;}
int xideReferenceDecoderRetire(xide_reference_decoder* d,xide_reference_result* result)
{if(!d || !result)return 0;xrtMutexLock(&d->lock);int okay=!d->stop && !d->retired;if(okay){d->retired=result;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;}
void xideReferenceDecoderCancel(xide_reference_decoder* d)
{if(!d)return;xrtMutexLock(&d->lock);d->current=0;d->requestId=0;d->received=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);}
xide_reference_result* xideReferenceDecoderTake(xide_reference_decoder* d)
{if(!d)return NULL;xrtMutexLock(&d->lock);xide_reference_result* result=d->completed && d->completed->token==d->current ? d->completed : NULL;if(result)d->completed=NULL;else if(d->completed)xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);return result;}
void xideReferenceDecoderDestroy(xide_reference_decoder* d)
{if(!d)return;xrtMutexLock(&d->lock);d->stop=1;d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);xrtThreadWait(d->thread);xrtThreadDestroy(d->thread);xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);}
