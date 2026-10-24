#include "services/signature_result.h"
#include <string.h>
#include <limits.h>
#define SIGNATURES 256u
#define PARAMETERS 128u
#define LABEL_BYTES 32768u
#define RESPONSE_BYTES (16u*1024u*1024u)
#define CONTENT_BYTES (8u*1024u*1024u)
static const xvalue* field(const xvalue* v,const char* name)
{return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,xrtStrView(name)) : NULL;}
static int string(const xvalue* v,size_t limit,xstrview* s)
{return v && xrtValueGetString(v,s) && s->Size<=limit && (!s->Size || (s->Data && !memchr(s->Data,0,s->Size))) && xrtUtf8Valid(*s,NULL);}
static int number(const xvalue* v,size_t limit,size_t* out)
{int64 n=0;if(!xrtValueGetInt(v,&n) || n<0 || (uint64)n>limit)return 0;*out=(size_t)n;return 1;}
static int equal(const xvalue* v,const char* expected)
{xstrview s={0};return string(v,128,&s) && s.Size==strlen(expected) && !memcmp(s.Data,expected,s.Size);}
static int stopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static const char* documentation(const xvalue* v,int* okay,size_t* bytes)
{
 if(!v)return "";
 xstrview s={0};if(!string(v,65536,&s)){
  if(!equal(field(v,"kind"),"plaintext") && !equal(field(v,"kind"),"markdown")){*okay=0;return NULL;}
  if(!string(field(v,"value"),65536,&s)){*okay=0;return NULL;}
 }
 if(s.Size>CONTENT_BYTES-*bytes){*okay=0;return NULL;}*bytes+=s.Size;return s.Size ? s.Data : "";
}
static int advance(const char* label,size_t size,size_t target,size_t* byte,size_t* units)
{
 while(*units<target){uint32 scalar=0;size_t read=0;if(*byte>=size || xrtUtf8Decode((xstrview){label+*byte,size-*byte},&scalar,&read)!=XUTF_OK || !read)return 0;
  size_t width=scalar>0xffff ? 2 : 1;if(width>target-*units)return 0;*units+=width;*byte+=read;
 }return *units==target;
}
void xideSignatureResultRelease(xide_signature_result* r)
{if(r){for(size_t i=0;i<r->count;++i)xrtFree(r->items[i].parameters);xrtFree(r->items);xrtValueRelease(r->message);xrtFree(r);}}
static void decodeInto(xide_signature_result* out,const xls_text* text,const xvalue* message,
 xide_signature_expected expected,int (*cancelled)(void*),void* user)
{
 size_t request=0,version=0,query=0;
 if(!text || xlsTextSize(text)>CONTENT_BYTES || expected.request<=1 || expected.version<=0 || expected.offset<0 || (size_t)expected.offset>xlsTextSize(text) ||
    !equal(field(message,"jsonrpc"),"2.0") || !number(field(message,"id"),INT_MAX,&request) || request!=(size_t)expected.request || field(message,"error") || stopped(cancelled,user))return;
 xls_position caret={0};if(!xlsTextPosition(text,(size_t)expected.offset,&caret))return;
 const xvalue* value=field(message,"result");if(value && xrtValueType(value)==XVALUE_NULL){out->valid=1;return;}
 if(!number(field(value,"version"),INT_MAX,&version) || version!=(size_t)expected.version || !number(field(value,"queryId"),INT_MAX,&query) || query!=request)return;
 const xvalue* list=field(value,"signatures");size_t count=xrtValueCount(list),active=0,global=0;
 if(!list || xrtValueType(list)!=XVALUE_ARRAY || count>SIGNATURES || (field(value,"activeSignature") && !number(field(value,"activeSignature"),UINT32_MAX,&active)) ||
    (field(value,"activeParameter") && !number(field(value,"activeParameter"),UINT32_MAX,&global)))return;
 if(!count){out->valid=1;return;}
 xide_signature_item* items=xrtCalloc(count,sizeof(*items));if(!items)return;
 out->items=items;out->count=count;out->activeSignature=active<count ? active : 0;size_t bytes=count*sizeof(*items);int okay=1;
 for(size_t i=0;okay && i<count;++i){
  if(stopped(cancelled,user)){okay=0;break;}
  const xvalue* sig=xrtValueArrayGet(list,i);xstrview label={0};const xvalue* params=field(sig,"parameters");size_t n=xrtValueCount(params),selected=global;
  okay=string(field(sig,"label"),LABEL_BYTES,&label) && label.Size && (!params || xrtValueType(params)==XVALUE_ARRAY) && n<=PARAMETERS &&
   (!field(sig,"activeParameter") || number(field(sig,"activeParameter"),UINT32_MAX,&selected));
  if(!okay || label.Size>CONTENT_BYTES-bytes){okay=0;break;}bytes+=label.Size;
  items[i].label=label.Data;items[i].documentation=documentation(field(sig,"documentation"),&okay,&bytes);
  items[i].activeParameter=selected<n ? selected : SIZE_MAX;items[i].count=n;
  if(n*sizeof(xide_signature_parameter)>CONTENT_BYTES-bytes){okay=0;break;}bytes+=n*sizeof(xide_signature_parameter);
  items[i].parameters=n ? xrtCalloc(n,sizeof(*items[i].parameters)) : NULL;if(n && !items[i].parameters){okay=0;break;}
  size_t cursor=0,units=0;
  for(size_t j=0;okay && j<n;++j){
   if(stopped(cancelled,user)){okay=0;break;}
   const xvalue* p=xrtValueArrayGet(params,j);const xvalue* raw=field(p,"label");xstrview textLabel={0};size_t start=0,end=0;
   if(string(raw,LABEL_BYTES,&textLabel)){
    if(!textLabel.Size){okay=0;break;}
    const char* found=strstr(label.Data+cursor,textLabel.Data);if(!found){okay=0;break;}
    start=(size_t)(found-label.Data);end=start+textLabel.Size;
    /* Include the intervening label text once, even for mixed string/tuple
       labels. Never scan the label prefix again for later parameters. */
    for(size_t at=cursor;at<end;){uint32 scalar=0;size_t read=0;if(xrtUtf8Decode((xstrview){label.Data+at,end-at},&scalar,&read)!=XUTF_OK || !read){okay=0;break;}units+=scalar>0xffff ? 2 : 1;at+=read;}
    cursor=end;

   }else{
    size_t a=0,b=0;okay=raw && xrtValueType(raw)==XVALUE_ARRAY && xrtValueCount(raw)==2 && number(xrtValueArrayGet(raw,0),UINT32_MAX,&a) && number(xrtValueArrayGet(raw,1),UINT32_MAX,&b) && a<=b && a>=units && advance(label.Data,label.Size,a,&cursor,&units);
    start=cursor;if(okay)okay=advance(label.Data,label.Size,b,&cursor,&units);end=cursor;
   }
   if(okay){items[i].parameters[j]=(xide_signature_parameter){start,end,documentation(field(p,"documentation"),&okay,&bytes)};}
  }
 }
 if(okay && !stopped(cancelled,user)){out->message=xrtValueRetain((xvalue*)message);out->valid=out->available=1;}
 else{for(size_t i=0;i<count;++i)xrtFree(items[i].parameters);xrtFree(items);out->items=NULL;out->count=0;}
}
xide_signature_result* xideSignatureDecode(const xls_text* text,const xvalue* message,xide_signature_expected expected,int (*cancelled)(void*),void* user)
{xide_signature_result* result=xrtCalloc(1,sizeof(*result));if(result)decodeInto(result,text,message,expected,cancelled,user);return result;}

typedef struct decode_job {
    uint64_t token;size_t wireBytes;xvalue* message;xls_text* text;xide_signature_expected expected;xide_signature_result* result;
} decode_job;
struct xide_signature_decoder {
    xthread* thread;xmutex lock;xcond changed;int stop;uint64_t current;
    decode_job pending;xide_signature_result *completed,*retired;
    void (*wake)(void*);void* user;
};
typedef struct cancel_state {xide_signature_decoder* decoder;uint64_t token;} cancel_state;
static int cancelled(void* user)
{
    cancel_state* state=user;xide_signature_decoder* d=state->decoder;
    xrtMutexLock(&d->lock);int result=d->stop || d->current!=state->token;xrtMutexUnlock(&d->lock);return result;
}
static void releaseJob(decode_job* job)
{ xrtValueRelease(job->message);xlsTextRelease(job->text);xideSignatureResultRelease(job->result); }
static int32 run(ptr user)
{
    xide_signature_decoder* d=user;
    for(;;) {
        decode_job job={0};xide_signature_result* garbage=NULL;
        xrtMutexLock(&d->lock);
        while(!d->stop && !d->pending.message && !(d->pending.token && d->pending.token!=d->current) && !d->retired && !(d->completed && d->completed->token!=d->current))
            xrtCondWait(&d->changed,&d->lock);
        if(d->stop) {xrtMutexUnlock(&d->lock);break;}
        if(d->retired) {garbage=d->retired;d->retired=NULL;}
        else if(d->completed && d->completed->token!=d->current) {garbage=d->completed;d->completed=NULL;}
        else {job=d->pending;memset(&d->pending,0,sizeof(d->pending));}
        xrtMutexUnlock(&d->lock);
        if(garbage) {xideSignatureResultRelease(garbage);if(d->wake) d->wake(d->user);continue;}
        cancel_state state={d,job.token};xide_signature_result* result=job.result;job.result=NULL;
        if(result && job.wireBytes<=RESPONSE_BYTES && !cancelled(&state)) decodeInto(result,job.text,job.message,job.expected,cancelled,&state);
        releaseJob(&job);
        xrtMutexLock(&d->lock);
        if(result && !d->stop && d->current==state.token) {result->token=state.token;garbage=d->completed;d->completed=result;result=NULL;}
        xrtMutexUnlock(&d->lock);
        xideSignatureResultRelease(garbage);xideSignatureResultRelease(result);
        if(d->wake) d->wake(d->user);
    }
    releaseJob(&d->pending);xideSignatureResultRelease(d->completed);xideSignatureResultRelease(d->retired);return 0;
}
xide_signature_decoder* xideSignatureDecoderCreate(void (*wake)(void*),void* user)
{
    xide_signature_decoder* d=xrtCalloc(1,sizeof(*d));if(!d) return NULL;
    if(!xrtMutexInit(&d->lock)) {xrtFree(d);return NULL;}
    if(!xrtCondInit(&d->changed)) {xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}
    d->wake=wake;d->user=user;d->thread=xrtThreadCreate(run,d,0);
    if(!d->thread) {xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}return d;
}
int xideSignatureDecoderExpect(xide_signature_decoder* d,uint64_t token,xls_text* text,xide_signature_expected expected)
{
    if(!d || !token || !text || expected.request<=1 || expected.version<=0 || expected.offset<0 ||
       (size_t)expected.offset>xlsTextSize(text) || xlsTextSize(text)>CONTENT_BYTES) return 0;
    xide_signature_result* result=xrtCalloc(1,sizeof(*result));if(!result) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending.message && !d->pending.token;
    if(okay) {d->current=token;d->pending=(decode_job){.token=token,.text=text,.expected=expected,.result=result};result=NULL;}
    xrtMutexUnlock(&d->lock);xideSignatureResultRelease(result);return okay;
}
int xideSignatureDecoderReceiveOwned(xvalue* message,size_t bytes,void* user)
{
    xide_signature_decoder* d=user;size_t request=0;
    if(!d || !message || !bytes || field(message,"method") ||
       !equal(field(message,"jsonrpc"),"2.0") || !number(field(message,"id"),INT_MAX,&request)) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && d->pending.token && d->pending.token==d->current &&
        !d->pending.message && (size_t)d->pending.expected.request==request;
    if(okay) {d->pending.message=message;d->pending.wireBytes=bytes;xrtCondSignal(&d->changed);}
    xrtMutexUnlock(&d->lock);return okay;
}
int xideSignatureDecoderRetire(xide_signature_decoder* d,xide_signature_result* result)
{
    if(!d || !result) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->retired;
    if(okay) {d->retired=result;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;
}
void xideSignatureDecoderCancel(xide_signature_decoder* d)
{ if(d) {xrtMutexLock(&d->lock);d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);} }
xide_signature_result* xideSignatureDecoderTake(xide_signature_decoder* d)
{
    if(!d) return NULL;
    xrtMutexLock(&d->lock);xide_signature_result* result=d->completed;d->completed=NULL;xrtMutexUnlock(&d->lock);return result;
}
void xideSignatureDecoderDestroy(xide_signature_decoder* d)
{
    if(!d) return;
    xrtMutexLock(&d->lock);d->stop=1;d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);
    xrtThreadWait(d->thread);xrtThreadDestroy(d->thread);xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);
}
