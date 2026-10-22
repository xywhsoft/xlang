#include "services/highlight_result.h"
#include "protocol/lsp_wire.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define HIGHLIGHT_LIMIT 65536u
static const xvalue* field(const xvalue* object,const char* name)
{ return object && xrtValueType(object)==XVALUE_OBJECT ? xrtValueObjectGet(object,xrtStrView(name)) : NULL; }
static int integer(const xvalue* value,int* out)
{ int64 n=0;if(!value || !xrtValueGetInt(value,&n) || n<0 || n>INT_MAX) return 0;*out=(int)n;return 1; }
static int position(const xvalue* value,xide_lsp_position* out)
{ return integer(field(value,"line"),&out->line) && integer(field(value,"character"),&out->character); }
static int compare(const void* a,const void* b)
{
    const xide_highlight_range *x=a,*y=b;
    if(x->start!=y->start) return x->start<y->start ? -1 : 1;
    if(x->end!=y->end) return x->end<y->end ? -1 : 1;
    return (x->kind>y->kind)-(x->kind<y->kind);
}
void xideHighlightResultRelease(xide_highlight_result* result)
{ if(result) { xrtFree(result->ranges);xrtFree(result); } }
static void decodeInto(xide_highlight_result* result,const xls_text* text,const xvalue* value,int (*cancelled)(void*),void* user)
{
    if(!text || xlsTextSize(text)>8u*1024u*1024u || !value || (cancelled && cancelled(user))) return;
    if(xrtValueType(value)==XVALUE_NULL) { result->valid=1;return; }
    if(xrtValueType(value)!=XVALUE_ARRAY || xrtValueCount(value)>HIGHLIGHT_LIMIT) return;
    size_t count=xrtValueCount(value);
    if(!count) { result->valid=1;return; }
    xide_lsp_position* positions=xrtCalloc(count*2,sizeof(*positions));
    xide_highlight_range* ranges=xrtCalloc(count,sizeof(*ranges));
    int okay=positions && ranges;
    for(size_t i=0;okay && i<count;++i) {
        if(cancelled && cancelled(user)) { okay=0;break; }
        const xvalue *item=xrtValueArrayGet(value,i),*range=field(item,"range"),*kind=field(item,"kind");
        ranges[i].kind=1;
        okay=position(field(range,"start"),positions+i*2) && position(field(range,"end"),positions+i*2+1) &&
             (!kind || (integer(kind,&ranges[i].kind) && ranges[i].kind>=1 && ranges[i].kind<=3));
    }
    if(okay) okay=xideLspManyPositionsToOffsets(xlsTextData(text),positions,count*2,cancelled,user);
    for(size_t i=0;okay && i<count;++i) {
        ranges[i].start=positions[i*2].offset;ranges[i].end=positions[i*2+1].offset;
        okay=ranges[i].start<ranges[i].end;
    }
    if(okay) {
        qsort(ranges,count,sizeof(*ranges),compare);
        size_t used=0;
        for(size_t i=0;i<count;++i) if(!used || compare(ranges+i,ranges+used-1)) ranges[used++]=ranges[i];
        if(cancelled && cancelled(user)) okay=0;
        if(okay) { result->ranges=ranges;result->count=used;result->valid=1;ranges=NULL; }
    }
    xrtFree(positions);xrtFree(ranges);
}
xide_highlight_result* xideHighlightDecode(const xls_text* text,const xvalue* value,int (*cancelled)(void*),void* user)
{
    xide_highlight_result* result=xrtCalloc(1,sizeof(*result));
    if(result) decodeInto(result,text,value,cancelled,user);
    return result;
}

typedef struct decode_job { uint64_t token;xvalue* message;xls_text* text;xide_highlight_result* result; } decode_job;
struct xide_highlight_decoder {
    xthread* thread;xmutex lock;xcond changed;
    uint64_t current;int stop;
    decode_job pending;
    xide_highlight_result* completed;
    void (*wake)(void*);void* user;
};
typedef struct cancel_state { xide_highlight_decoder* decoder;uint64_t token; } cancel_state;
static int cancelled(void* user)
{
    cancel_state* state=user;xide_highlight_decoder* decoder=state->decoder;
    xrtMutexLock(&decoder->lock);int stop=decoder->stop || decoder->current!=state->token;xrtMutexUnlock(&decoder->lock);return stop;
}
static void releaseJob(decode_job* job)
{ xrtValueRelease(job->message);xlsTextRelease(job->text);xideHighlightResultRelease(job->result);memset(job,0,sizeof(*job)); }
static int32 run(ptr user)
{
    xide_highlight_decoder* decoder=user;
    for(;;) {
        xrtMutexLock(&decoder->lock);
        while(!decoder->stop && !decoder->pending.token) xrtCondWait(&decoder->changed,&decoder->lock);
        if(decoder->stop) { xrtMutexUnlock(&decoder->lock);break; }
        decode_job job=decoder->pending;memset(&decoder->pending,0,sizeof(decoder->pending));
        xrtMutexUnlock(&decoder->lock);
        cancel_state state={decoder,job.token};
        xide_highlight_result* result=job.result;job.result=NULL;
        if(!cancelled(&state) && !field(job.message,"error")) decodeInto(result,job.text,field(job.message,"result"),cancelled,&state);
        releaseJob(&job);
        xrtMutexLock(&decoder->lock);
        int publish=result && !decoder->stop && decoder->current==state.token;
        if(publish) { result->token=state.token;xideHighlightResultRelease(decoder->completed);decoder->completed=result;result=NULL; }
        xrtMutexUnlock(&decoder->lock);
        xideHighlightResultRelease(result);
        if(publish && decoder->wake) decoder->wake(decoder->user);
    }
    releaseJob(&decoder->pending);return 0;
}
xide_highlight_decoder* xideHighlightDecoderCreate(void (*wake)(void*),void* user)
{
    xide_highlight_decoder* decoder=xrtCalloc(1,sizeof(*decoder));if(!decoder) return NULL;
    if(!xrtMutexInit(&decoder->lock)) { xrtFree(decoder);return NULL; }
    if(!xrtCondInit(&decoder->changed)) { xrtMutexUnit(&decoder->lock);xrtFree(decoder);return NULL; }
    decoder->wake=wake;decoder->user=user;decoder->thread=xrtThreadCreate(run,decoder,0);
    if(!decoder->thread) { xrtCondUnit(&decoder->changed);xrtMutexUnit(&decoder->lock);xrtFree(decoder);return NULL; }return decoder;
}
int xideHighlightDecoderSubmit(xide_highlight_decoder* decoder,uint64_t token,xvalue* message,xls_text* text)
{
    if(!decoder || !token || !message || !text) return 0;
    xide_highlight_result* result=xrtCalloc(1,sizeof(*result));if(!result) return 0;
    xrtMutexLock(&decoder->lock);
    int okay=!decoder->stop && !decoder->pending.token;
    if(okay) { decoder->current=token;decoder->pending=(decode_job){token,message,text,result};result=NULL;xideHighlightResultRelease(decoder->completed);decoder->completed=NULL;xrtCondSignal(&decoder->changed); }
    xrtMutexUnlock(&decoder->lock);xideHighlightResultRelease(result);return okay;
}
void xideHighlightDecoderCancel(xide_highlight_decoder* decoder)
{
    if(!decoder) return;
    xrtMutexLock(&decoder->lock);decoder->current=0;xideHighlightResultRelease(decoder->completed);decoder->completed=NULL;xrtCondSignal(&decoder->changed);xrtMutexUnlock(&decoder->lock);
}
xide_highlight_result* xideHighlightDecoderTake(xide_highlight_decoder* decoder)
{
    if(!decoder) return NULL;
    xrtMutexLock(&decoder->lock);xide_highlight_result* result=decoder->completed;decoder->completed=NULL;xrtMutexUnlock(&decoder->lock);return result;
}
void xideHighlightDecoderDestroy(xide_highlight_decoder* decoder)
{
    if(!decoder) return;
    xrtMutexLock(&decoder->lock);decoder->stop=1;decoder->current=0;xrtCondSignal(&decoder->changed);xrtMutexUnlock(&decoder->lock);
    xrtThreadWait(decoder->thread);xrtThreadDestroy(decoder->thread);xideHighlightResultRelease(decoder->completed);
    xrtCondUnit(&decoder->changed);xrtMutexUnit(&decoder->lock);xrtFree(decoder);
}
