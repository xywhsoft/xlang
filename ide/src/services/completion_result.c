#include "services/completion_result.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#define ITEM_LIMIT 4096u
#define TEXT_LIMIT (8u*1024u*1024u)

static const xvalue* field(const xvalue* value,const char* name)
{ return value && xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,xrtStrView(name)) : NULL; }
static int integer(const xvalue* value,int* out)
{ int64 n=0;if(!value || !xrtValueGetInt(value,&n) || n<0 || n>INT_MAX) return 0;*out=(int)n;return 1; }
static int boolIs(const xvalue* value,int expected)
{ bool b=false;return value && xrtValueGetBool(value,&b) && b==(bool)expected; }
static int equal(const xvalue* value,xstrview expected)
{ xstrview s={0};return value && xrtValueGetString(value,&s) && s.Size==expected.Size && (!s.Size || !memcmp(s.Data,expected.Data,s.Size)); }
static int literal(const xvalue* value,const char* text) { return equal(value,xrtStrView(text)); }
static int string(const xvalue* value,size_t limit,xstrview* out)
{
    xstrview s={0};if(!value || !xrtValueGetString(value,&s) || s.Size>limit ||
        (s.Size && (!s.Data || memchr(s.Data,0,s.Size))) || !xrtUtf8Valid(s,NULL)) return 0;
    if(!s.Data) s.Data="";
    *out=s;return 1;
}
static int identifier(xstrview s)
{
    if(!s.Size || ((unsigned char)s.Data[0]<128 && !isalpha((unsigned char)s.Data[0]) && s.Data[0]!='_')) return 0;
    for(size_t i=0;i<s.Size;++i) if((unsigned char)s.Data[i]<128 && !isalnum((unsigned char)s.Data[i]) && s.Data[i]!='_') return 0;
    return 1;
}
typedef struct range { int sl,sc,el,ec; } range;
static int readRange(const xvalue* value,range* out)
{
    return integer(field(field(value,"start"),"line"),&out->sl) && integer(field(field(value,"start"),"character"),&out->sc) &&
        integer(field(field(value,"end"),"line"),&out->el) && integer(field(field(value,"end"),"character"),&out->ec);
}
static int sameRange(range a,range b) { return a.sl==b.sl && a.sc==b.sc && a.el==b.el && a.ec==b.ec; }
/* xlsTextOffset intentionally clamps columns, so require an exact inverse. */
static int strictOffset(const xls_text* text,int line,int character,int* out)
{
    size_t at=0;xls_position check={0};
    if(!xlsTextOffset(text,(xls_position){(size_t)line,(size_t)character},&at) || at>INT_MAX ||
       !xlsTextPosition(text,at,&check) || check.line!=(size_t)line || check.character!=(size_t)character) return 0;
    *out=(int)at;return 1;
}
static int nameCompare(const void* a,const void* b) { return strcmp(*(const char* const*)a,*(const char* const*)b); }
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
void xideCompletionResultRelease(xide_completion_result* result)
{ if(result) {xrtFree(result->items);xrtFree(result->names);xrtValueRelease(result->message);xrtFree(result);} }
static void decodeInto(xide_completion_result* out,const xls_text* text,const xvalue* message,
    xide_completion_expected expected,int (*cancelled)(void*),void* user)
{
    if(!text || xlsTextSize(text)>TEXT_LIMIT || expected.request<=1 || expected.version<=0 || expected.offset<0 ||
       (size_t)expected.offset>xlsTextSize(text) || stopped(cancelled,user)) return;
    const xvalue* value=field(message,"result");int request=0,version=0,query=0;
    const xvalue* list=field(value,"items");
    if(!literal(field(message,"jsonrpc"),"2.0") || !integer(field(message,"id"),&request) || request!=expected.request ||
       field(message,"error") || !integer(field(value,"version"),&version) || version!=expected.version ||
       !integer(field(value,"queryId"),&query) || query!=expected.request || !literal(field(value,"scope"),"cursor-query") ||
       !literal(field(value,"guardScope"),"background-poll") || !boolIs(field(value,"checkedCompilation"),0) ||
       !boolIs(field(value,"isIncomplete"),0) || !list || xrtValueType(list)!=XVALUE_ARRAY || xrtValueCount(list)>ITEM_LIMIT) return;
    if(boolIs(field(value,"available"),0)) {out->valid=!xrtValueCount(list) && boolIs(field(value,"ok"),0) && !stopped(cancelled,user);return;}
    if(!boolIs(field(value,"available"),1) || !boolIs(field(value,"ok"),1)) return;
    range replacement={0};int start=0,end=0;xls_position caret={0};
    if(!xlsTextPosition(text,(size_t)expected.offset,&caret) || !readRange(field(value,"replacementRange"),&replacement) ||
       !strictOffset(text,replacement.sl,replacement.sc,&start) || !strictOffset(text,replacement.el,replacement.ec,&end) ||
       start>expected.offset || end<expected.offset) return;
    const char* bytes=xlsTextData(text);
    if(!equal(field(value,"prefix"),(xstrview){bytes+start,(size_t)(expected.offset-start)})) return;
    if(start==end) {if(!start || bytes[start-1]!='.') return;}
    else {
        if(start>0 && ((unsigned char)bytes[start-1]>=128 || isalnum((unsigned char)bytes[start-1]) || bytes[start-1]=='_')) return;
        if((unsigned char)bytes[end]>=128 || isalnum((unsigned char)bytes[end]) || bytes[end]=='_') return;
        for(int at=start;at<end;++at) {
            if((at-start)%1024==0 && stopped(cancelled,user)) return;
            if((unsigned char)bytes[at]<128 && !isalnum((unsigned char)bytes[at]) && bytes[at]!='_') return;
        }
    }
    size_t count=xrtValueCount(list);
    xide_completion_item* items=count ? xrtCalloc(count,sizeof(*items)) : NULL;
    const char** names=count ? xrtCalloc(count,sizeof(*names)) : NULL;
    int okay=!count || (items && names);
    for(size_t i=0;okay && i<count;++i) {
        if(i%16==0 && stopped(cancelled,user)) {okay=0;break;}
        const xvalue *item=xrtValueArrayGet(list,i),*edit=field(item,"textEdit"),*detail=field(item,"detail");
        xstrview label={0},sort={0},description={"",0};int format=0,kind=0;int64 symbol=0;range itemRange={0};
        okay=string(field(item,"label"),512,&label) && identifier(label) && equal(field(edit,"newText"),label) &&
            equal(field(item,"filterText"),label) && string(field(item,"sortText"),32,&sort) &&
            integer(field(item,"insertTextFormat"),&format) && format==1 && integer(field(item,"kind"),&kind) &&
            (kind==2 || kind==3 || kind==5 || kind==6 || kind==7 || kind==21) &&
            xrtValueGetInt(field(field(item,"data"),"symbolId"),&symbol) && symbol>=0 && (uint64_t)symbol<=UINT32_MAX &&
            readRange(field(edit,"range"),&itemRange) && sameRange(itemRange,replacement) && (!detail || string(detail,8192,&description));
        if(okay) {items[i]=(xide_completion_item){label.Data,description.Data,sort.Data,(uint32_t)symbol};names[i]=label.Data;}
    }
    if(okay && !stopped(cancelled,user)) {
        if(count) qsort(names,count,sizeof(*names),nameCompare);
        if(!stopped(cancelled,user)) {
            out->message=xrtValueRetain((xvalue*)message);out->items=items;out->names=names;out->count=count;
            out->start=start;out->end=end;out->available=out->valid=1;return;
        }
    }
    xrtFree(items);xrtFree(names);
}
xide_completion_result* xideCompletionDecode(const xls_text* text,const xvalue* message,
    xide_completion_expected expected,int (*cancelled)(void*),void* user)
{
    xide_completion_result* result=xrtCalloc(1,sizeof(*result));
    if(result) decodeInto(result,text,message,expected,cancelled,user);
    return result;
}

typedef struct decode_job {
    uint64_t token;xvalue* message;xls_text* text;xide_completion_expected expected;xide_completion_result* result;
} decode_job;
struct xide_completion_decoder {
    xthread* thread;xmutex lock;xcond changed;int stop;uint64_t current;
    decode_job pending;xide_completion_result *completed,*retired;
    void (*wake)(void*);void* user;
};
typedef struct cancel_state {xide_completion_decoder* decoder;uint64_t token;} cancel_state;
static int cancelled(void* user)
{
    cancel_state* state=user;xide_completion_decoder* d=state->decoder;
    xrtMutexLock(&d->lock);int result=d->stop || d->current!=state->token;xrtMutexUnlock(&d->lock);return result;
}
static void releaseJob(decode_job* job)
{ xrtValueRelease(job->message);xlsTextRelease(job->text);xideCompletionResultRelease(job->result); }
static int32 run(ptr user)
{
    xide_completion_decoder* d=user;
    for(;;) {
        decode_job job={0};xide_completion_result* garbage=NULL;
        xrtMutexLock(&d->lock);
        while(!d->stop && !d->pending.message && !(d->pending.token && d->pending.token!=d->current) && !d->retired && !(d->completed && d->completed->token!=d->current))
            xrtCondWait(&d->changed,&d->lock);
        if(d->stop) {xrtMutexUnlock(&d->lock);break;}
        if(d->retired) {garbage=d->retired;d->retired=NULL;}
        else if(d->completed && d->completed->token!=d->current) {garbage=d->completed;d->completed=NULL;}
        else {job=d->pending;memset(&d->pending,0,sizeof(d->pending));}
        xrtMutexUnlock(&d->lock);
        if(garbage) {xideCompletionResultRelease(garbage);if(d->wake) d->wake(d->user);continue;}
        cancel_state state={d,job.token};xide_completion_result* result=job.result;job.result=NULL;
        if(result && !cancelled(&state)) decodeInto(result,job.text,job.message,job.expected,cancelled,&state);
        releaseJob(&job);
        xrtMutexLock(&d->lock);
        if(result && !d->stop && d->current==state.token) {result->token=state.token;garbage=d->completed;d->completed=result;result=NULL;}
        xrtMutexUnlock(&d->lock);
        xideCompletionResultRelease(garbage);xideCompletionResultRelease(result);
        if(d->wake) d->wake(d->user);
    }
    releaseJob(&d->pending);xideCompletionResultRelease(d->completed);xideCompletionResultRelease(d->retired);return 0;
}
xide_completion_decoder* xideCompletionDecoderCreate(void (*wake)(void*),void* user)
{
    xide_completion_decoder* d=xrtCalloc(1,sizeof(*d));if(!d) return NULL;
    if(!xrtMutexInit(&d->lock)) {xrtFree(d);return NULL;}
    if(!xrtCondInit(&d->changed)) {xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}
    d->wake=wake;d->user=user;d->thread=xrtThreadCreate(run,d,0);
    if(!d->thread) {xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}return d;
}
int xideCompletionDecoderSubmit(xide_completion_decoder* d,uint64_t token,xvalue* message,xls_text* text,xide_completion_expected expected)
{
    if(!d || !token || !message || !text) return 0;
    xide_completion_result* result=xrtCalloc(1,sizeof(*result));if(!result) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending.message && !d->pending.token;
    if(okay) {d->current=token;d->pending=(decode_job){token,message,text,expected,result};result=NULL;xrtCondSignal(&d->changed);}
    xrtMutexUnlock(&d->lock);xideCompletionResultRelease(result);return okay;
}
int xideCompletionDecoderExpect(xide_completion_decoder* d,uint64_t token,xls_text* text,xide_completion_expected expected)
{
    if(!d || !token || !text || expected.request<=1 || expected.version<=0 || expected.offset<0 ||
       (size_t)expected.offset>xlsTextSize(text) || xlsTextSize(text)>TEXT_LIMIT) return 0;
    xide_completion_result* result=xrtCalloc(1,sizeof(*result));if(!result) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending.message && !d->pending.token;
    if(okay) {d->current=token;d->pending=(decode_job){token,NULL,text,expected,result};result=NULL;}
    xrtMutexUnlock(&d->lock);xideCompletionResultRelease(result);return okay;
}
int xideCompletionDecoderReceiveOwned(xvalue* message,size_t bytes,void* user)
{
    xide_completion_decoder* d=user;int request=0;
    if(!d || !message || !bytes || bytes>TEXT_LIMIT || field(message,"method") ||
       !literal(field(message,"jsonrpc"),"2.0") || !integer(field(message,"id"),&request)) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && d->pending.token && d->pending.token==d->current &&
        !d->pending.message && d->pending.expected.request==request;
    if(okay) {d->pending.message=message;xrtCondSignal(&d->changed);}
    xrtMutexUnlock(&d->lock);return okay;
}
int xideCompletionDecoderDiscard(xide_completion_decoder* d,xvalue* message)
{
    if(!d || !message) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending.message && !d->pending.token;
    if(okay) {d->pending=(decode_job){.message=message};xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;
}
int xideCompletionDecoderRetire(xide_completion_decoder* d,xide_completion_result* result)
{
    if(!d || !result) return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->retired;
    if(okay) {d->retired=result;xrtCondSignal(&d->changed);}xrtMutexUnlock(&d->lock);return okay;
}
void xideCompletionDecoderCancel(xide_completion_decoder* d)
{ if(d) {xrtMutexLock(&d->lock);d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);} }
xide_completion_result* xideCompletionDecoderTake(xide_completion_decoder* d)
{
    if(!d) return NULL;
    xrtMutexLock(&d->lock);xide_completion_result* result=d->completed;d->completed=NULL;xrtMutexUnlock(&d->lock);return result;
}
void xideCompletionDecoderDestroy(xide_completion_decoder* d)
{
    if(!d) return;
    xrtMutexLock(&d->lock);d->stop=1;d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);
    xrtThreadWait(d->thread);xrtThreadDestroy(d->thread);xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);
}
