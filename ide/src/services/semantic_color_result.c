#include "services/semantic_color_result.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define COLOR_LIMIT 65536u
#define COLOR_WORD_LIMIT (COLOR_LIMIT*5u)
#define COLOR_BUDGET (16u*1024u*1024u)
static const char* const roleNames[]={"type","class","interface","enum","struct","typeParameter","parameter","variable","property","enumMember","function","method"};
static const char* const modifierNames[]={"declaration","readonly","async"};
static const xvalue* field(const xvalue* v,const char* key)
{ return v && xrtValueType(v)==XVALUE_OBJECT ? xrtValueObjectGet(v,xrtStrView(key)) : NULL; }
static int number(const xvalue* v,uint32_t* out)
{ int64 n=0;if(!v || xrtValueType(v)!=XVALUE_INT || !xrtValueGetInt(v,&n) || n<0 || n>INT_MAX)return 0;*out=(uint32_t)n;return 1; }
static int truth(const xvalue* v,int* out)
{ bool b=false;if(!v || xrtValueType(v)!=XVALUE_BOOL || !xrtValueGetBool(v,&b))return 0;*out=b;return 1; }
static int names(const xvalue* v,const char* const* choices,unsigned maximum,unsigned* out,unsigned* count,int bits)
{
    if(!v || xrtValueType(v)!=XVALUE_ARRAY || xrtValueCount(v)>maximum)return 0;
    *count=(unsigned)xrtValueCount(v);unsigned seen=0;
    for(unsigned i=0;i<*count;++i){
        xstrview s={0};if(!xrtValueGetString(xrtValueArrayGet(v,i),&s))return 0;
        unsigned j=0;for(;j<maximum;++j)if(s.Size==strlen(choices[j]) && !memcmp(s.Data,choices[j],s.Size))break;
        if(j==maximum || (seen&(1u<<j)))return 0;
        seen|=1u<<j;out[i]=bits?1u<<j:j;
    }
    return 1;
}
static int validLegend(const xide_color_legend* l)
{
    if(!l || !l->typeCount || l->typeCount>XIDE_COLOR_ROLE_COUNT || l->modifierCount>3 || (l->delta!=0 && l->delta!=1))return 0;
    unsigned seen=0;
    for(unsigned i=0;i<l->typeCount;++i){unsigned k=l->types[i];if(k>=XIDE_COLOR_ROLE_COUNT || (seen&(1u<<k)))return 0;seen|=1u<<k;}
    seen=0;for(unsigned i=0;i<l->modifierCount;++i){unsigned b=l->modifiers[i];if((b!=1 && b!=2 && b!=4) || (seen&b))return 0;seen|=b;}
    return 1;
}
int xideColorLegendParse(const xvalue* v,xide_color_legend* out)
{
    if(!out)return 0;
    memset(out,0,sizeof(*out));xide_color_legend l={0};int full=0;const xvalue* f=field(v,"full");
    if(f && xrtValueType(f)==XVALUE_OBJECT){full=1;const xvalue* d=field(f,"delta");if(d && !truth(d,&l.delta))return 0;}
    else if(!truth(f,&full))return 0;
    const xvalue* legend=field(v,"legend");
    if(!full || !names(field(legend,"tokenTypes"),roleNames,XIDE_COLOR_ROLE_COUNT,l.types,&l.typeCount,0) ||
       !names(field(legend,"tokenModifiers"),modifierNames,3,l.modifiers,&l.modifierCount,1) || !validLegend(&l))return 0;
    *out=l;return 1;
}
static int sameLegend(const xide_color_legend* a,const xide_color_legend* b)
{
    return a->typeCount==b->typeCount && a->modifierCount==b->modifierCount &&
        !memcmp(a->types,b->types,a->typeCount*sizeof(*a->types)) &&
        !memcmp(a->modifiers,b->modifiers,a->modifierCount*sizeof(*a->modifiers));
}
struct xide_color_result {
    xatomic32 references;
    xide_color_legend legend;
    uint32_t* words;xide_color_range* ranges;char* id;
    size_t wordCount,count,bytes,peakBytes,scanned;
};
xide_color_result* xideColorResultRetain(xide_color_result* r)
{ if(r)xrtAtomic32FetchAdd(&r->references,1,XMEMORY_RELAXED);return r; }
void xideColorResultRelease(xide_color_result* r)
{ if(r && xrtAtomic32FetchSub(&r->references,1,XMEMORY_ACQ_REL)==1){xrtFree(r->words);xrtFree(r->ranges);xrtFree(r->id);xrtFree(r);} }
size_t xideColorResultCount(const xide_color_result* r){return r?r->count:0;}
const xide_color_range* xideColorResultRanges(const xide_color_result* r){return r?r->ranges:NULL;}
size_t xideColorResultWordCount(const xide_color_result* r){return r?r->wordCount:0;}
const uint32_t* xideColorResultWords(const xide_color_result* r){return r?r->words:NULL;}
const char* xideColorResultId(const xide_color_result* r){return r?r->id:NULL;}
size_t xideColorResultBytes(const xide_color_result* r){return r?r->bytes:0;}
size_t xideColorResultPeakBytes(const xide_color_result* r){return r?r->peakBytes:0;}
size_t xideColorResultScannedBytes(const xide_color_result* r){return r?r->scanned:0;}
const xide_color_range* xideColorResultFind(const xide_color_result* r,int start,int end)
{
    if(!r || start<0 || end<=start)return NULL;
    size_t lo=0,hi=r->count;
    while(lo<hi){size_t m=lo+(hi-lo)/2;if(r->ranges[m].start<start)lo=m+1;else hi=m;}
    return lo<r->count && r->ranges[lo].start==start && r->ranges[lo].end==end ? r->ranges+lo : NULL;
}
typedef struct color_edit { size_t start,remove,insert,index;const xvalue* data; } color_edit;
/* Equal starts retain wire order: text-edit semantics permits ordered inserts
 * followed by one removal/replacement at that offset. */
static int compareEdit(const void* a,const void* b)
{const color_edit *x=a,*y=b;return x->start!=y->start?(x->start>y->start)-(x->start<y->start):(x->index>y->index)-(x->index<y->index);}
static int stopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static int copyWords(uint32_t* to,const uint32_t* from,size_t count,int (*cancelled)(void*),void* user)
{
    for(size_t at=0;at<count;){if(stopped(cancelled,user))return 0;size_t n=count-at;if(n>1024)n=1024;memcpy(to+at,from+at,n*sizeof(*to));at+=n;}
    return 1;
}
static enum xide_color_error readWords(uint32_t* out,const xvalue* array,int (*cancelled)(void*),void* user)
{
    size_t count=xrtValueCount(array);
    for(size_t i=0;i<count;++i){if(!(i%256) && stopped(cancelled,user))return XIDE_COLOR_CANCELLED;if(!number(xrtValueArrayGet(array,i),out+i))return XIDE_COLOR_WIRE;}
    return XIDE_COLOR_OK;
}
static enum xide_color_error decodeWords(xide_color_result* r,const xvalue* v,const xide_color_result* base,size_t budget,int (*cancelled)(void*),void* user)
{
    const xvalue *data=field(v,"data"),*edits=field(v,"edits");color_edit* sorted=NULL;
    size_t count=0,editCount=0;enum xide_color_error error=XIDE_COLOR_WIRE;
    if(!!data==!!edits)return error;
    if(data){
        if(xrtValueType(data)!=XVALUE_ARRAY)return error;
        count=xrtValueCount(data);
    }else{
        if(!r->legend.delta || !base || !base->id || !sameLegend(&base->legend,&r->legend))return XIDE_COLOR_BASE;
        if(xrtValueType(edits)!=XVALUE_ARRAY)return error;
        editCount=xrtValueCount(edits);if(editCount>COLOR_LIMIT)return XIDE_COLOR_LIMIT;
        if(editCount>(budget-r->bytes)/sizeof(*sorted))return XIDE_COLOR_LIMIT;
        if(editCount){sorted=xrtCalloc(editCount,sizeof(*sorted));if(!sorted)return XIDE_COLOR_MEMORY;}
        r->peakBytes=r->bytes+editCount*sizeof(*sorted);
        size_t removed=0,inserted=0;
        for(size_t i=0;i<editCount;++i){
            if(!(i%256) && stopped(cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
            const xvalue* e=xrtValueArrayGet(edits,i);uint32_t start=0,remove=0;const xvalue* add=field(e,"data");
            if(!number(field(e,"start"),&start) || !number(field(e,"deleteCount"),&remove) ||
               start>base->wordCount || remove>base->wordCount-start || (add && xrtValueType(add)!=XVALUE_ARRAY))goto done;
            size_t n=add?xrtValueCount(add):0;
            if(n>COLOR_WORD_LIMIT || inserted>COLOR_WORD_LIMIT-n){error=XIDE_COLOR_LIMIT;goto done;}
            inserted+=n;removed+=remove;sorted[i]=(color_edit){start,remove,n,i,add};
        }
        if(stopped(cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
        if(editCount)qsort(sorted,editCount,sizeof(*sorted),compareEdit);
        size_t consumed=0;
        for(size_t i=0;i<editCount;++i){
            if(!(i%256) && stopped(cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
            if(sorted[i].start<consumed)goto done;
            consumed=sorted[i].start+sorted[i].remove;
        }
        if(removed>base->wordCount)goto done;
        count=base->wordCount-removed+inserted;
    }
    if(count>COLOR_WORD_LIMIT){error=XIDE_COLOR_LIMIT;goto done;}
    if(count%5)goto done;
    if(count>(budget-r->bytes-editCount*sizeof(*sorted))/sizeof(*r->words)){error=XIDE_COLOR_LIMIT;goto done;}
    if(count){r->words=xrtCalloc(count,sizeof(*r->words));if(!r->words){error=XIDE_COLOR_MEMORY;goto done;}}
    r->wordCount=count;r->bytes+=count*sizeof(*r->words);r->peakBytes=r->bytes+editCount*sizeof(*sorted);
    if(data){error=readWords(r->words,data,cancelled,user);goto done;}
    size_t from=0,to=0;
    for(size_t i=0;i<editCount;++i){
        if(!(i%256) && stopped(cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
        color_edit e=sorted[i];size_t n=e.start-from;
        if(n && !copyWords(r->words+to,base->words+from,n,cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
        to+=n;from=e.start+e.remove;
        error=readWords(e.insert?r->words+to:NULL,e.data,cancelled,user);if(error)goto done;to+=e.insert;
    }
    if(base->wordCount>from && !copyWords(r->words+to,base->words+from,base->wordCount-from,cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
    error=XIDE_COLOR_OK;
done:
    xrtFree(sorted);return error;
}
/* xls_text already proved UTF-8. Keep one monotone cursor rather than rescan
 * every long-line prefix. Poll by bytes consumed, not a byte-offset modulus. */
typedef struct color_cursor {const unsigned char* text;size_t size,at,sincePoll;uint32_t line,column;} color_cursor;
static enum xide_color_error advance(color_cursor* c,uint32_t line,uint32_t column,int (*cancelled)(void*),void* user)
{
    if(line<c->line || (line==c->line && column<c->column))return XIDE_COLOR_COORDINATE;
    while(c->line<line || (c->line==line && c->column<column)){
        if(c->sincePoll>=4096){c->sincePoll=0;if(stopped(cancelled,user))return XIDE_COLOR_CANCELLED;}
        if(c->at==c->size)return XIDE_COLOR_COORDINATE;
        unsigned char b=c->text[c->at];size_t n=b<128?1:b<0xe0?2:b<0xf0?3:4;
        if(b=='\r' || b=='\n'){
            if(c->line==line)return XIDE_COLOR_COORDINATE;
            n=b=='\r' && c->at+1<c->size && c->text[c->at+1]=='\n'?2:1;++c->line;c->column=0;
        }else{
            unsigned units=n==4?2:1;
            if(c->line==line && units>column-c->column)return XIDE_COLOR_COORDINATE;
            c->column+=units;
        }
        c->at+=n;c->sincePoll+=n;
    }
    return XIDE_COLOR_OK;
}
xide_color_result* xideColorDecode(const xls_text* text,const xide_color_legend* legend,const xvalue* v,const xide_color_result* base,size_t budget,int (*cancelled)(void*),void* user,enum xide_color_error* out)
{
    enum xide_color_error error=XIDE_COLOR_ARGUMENT;xide_color_result* r=NULL;
    budget=budget?budget:COLOR_BUDGET;
    if(!text || xlsTextSize(text)>8u*1024u*1024u || !validLegend(legend) || !v)goto done;
    if(budget>COLOR_BUDGET || budget<sizeof(*r)){error=XIDE_COLOR_LIMIT;goto done;}
    if(stopped(cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
    r=xrtCalloc(1,sizeof(*r));if(!r){error=XIDE_COLOR_MEMORY;goto done;}
    xrtAtomic32Init(&r->references,1);r->legend=*legend;r->bytes=r->peakBytes=sizeof(*r);
    if(xrtValueType(v)==XVALUE_NULL){error=XIDE_COLOR_OK;goto done;}
    if(xrtValueType(v)!=XVALUE_OBJECT){error=XIDE_COLOR_WIRE;goto done;}
    const xvalue* id=field(v,"resultId");
    if(id){
        xstrview s={0};error=XIDE_COLOR_WIRE;
        if(!xrtValueGetString(id,&s) || !s.Size || s.Size>65535 || memchr(s.Data,0,s.Size) || !xrtUtf8Valid(s,NULL))goto done;
        if(s.Size+1>budget-r->bytes){error=XIDE_COLOR_LIMIT;goto done;}
        r->id=xrtCalloc(s.Size+1,1);if(!r->id){error=XIDE_COLOR_MEMORY;goto done;}
        memcpy(r->id,s.Data,s.Size);r->bytes+=s.Size+1;r->peakBytes=r->bytes;
    }
    error=decodeWords(r,v,base,budget,cancelled,user);if(error)goto done;
    r->count=r->wordCount/5;
    if(r->count>(budget-r->bytes)/sizeof(*r->ranges)){error=XIDE_COLOR_LIMIT;goto done;}
    if(r->count){r->ranges=xrtCalloc(r->count,sizeof(*r->ranges));if(!r->ranges){error=XIDE_COLOR_MEMORY;goto done;}}
    r->bytes+=r->count*sizeof(*r->ranges);if(r->peakBytes<r->bytes)r->peakBytes=r->bytes;
    color_cursor cursor={(const unsigned char*)xlsTextData(text),xlsTextSize(text),0,0,0,0};uint32_t line=0,start=0;
    for(size_t i=0;i<r->count;++i){
        if(!(i%256) && stopped(cancelled,user)){error=XIDE_COLOR_CANCELLED;goto done;}
        const uint32_t* w=r->words+i*5;
        if(w[0]>INT_MAX-line || (!w[0] && w[1]>INT_MAX-start) || !w[2]){error=XIDE_COLOR_COORDINATE;goto done;}
        line+=w[0];start=w[0]?w[1]:start+w[1];
        if(w[2]>INT_MAX-start){error=XIDE_COLOR_COORDINATE;goto done;}
        if(w[3]>=legend->typeCount || w[4]>=(1u<<legend->modifierCount)){error=XIDE_COLOR_WIRE;goto done;}
        error=advance(&cursor,line,start,cancelled,user);if(error)goto done;
        xide_color_range row={(int)cursor.at,0,legend->types[w[3]],0};
        error=advance(&cursor,line,start+w[2],cancelled,user);if(error)goto done;
        row.end=(int)cursor.at;for(unsigned j=0;j<legend->modifierCount;++j)if(w[4]&(1u<<j))row.modifiers|=legend->modifiers[j];
        r->ranges[i]=row;
    }
    r->scanned=cursor.at;error=stopped(cancelled,user)?XIDE_COLOR_CANCELLED:XIDE_COLOR_OK;
done:
    if(error){xideColorResultRelease(r);r=NULL;}
    if(out)*out=error;
    return r;
}

typedef struct color_job {
    uint64_t token;xvalue* message;xls_text* text;xide_color_legend legend;xide_color_result* base;size_t budget;
} color_job;
struct xide_color_decoder {
    xthread* thread;xmutex lock;xcond changed;int stop;
    uint64_t current,lastToken;color_job pending;
    int hasCompleted;xide_color_completion completed;xide_color_result* retired;
    void (*wake)(void*);void* user;
};
typedef struct color_stop {xide_color_decoder* decoder;uint64_t token;} color_stop;
static int jobStopped(void* user)
{color_stop* s=user;xrtMutexLock(&s->decoder->lock);int stop=s->decoder->stop || s->decoder->current!=s->token;xrtMutexUnlock(&s->decoder->lock);return stop;}
static void releaseJob(color_job* j)
{xrtValueRelease(j->message);xlsTextRelease(j->text);xideColorResultRelease(j->base);}
static int stale(const xide_color_decoder* d){return d->hasCompleted && d->completed.token!=d->current;}
static int32 run(ptr user)
{
    xide_color_decoder* d=user;
    for(;;){
        xrtMutexLock(&d->lock);
        while(!d->stop && !d->pending.message && !d->retired && !stale(d))xrtCondWait(&d->changed,&d->lock);
        if(d->stop){xrtMutexUnlock(&d->lock);break;}
        color_job job=d->pending;memset(&d->pending,0,sizeof(d->pending));
        xide_color_result *retired=d->retired,*old=NULL;d->retired=NULL;
        if(stale(d)){old=d->completed.result;memset(&d->completed,0,sizeof(d->completed));d->hasCompleted=0;}
        xrtMutexUnlock(&d->lock);
        xideColorResultRelease(retired);xideColorResultRelease(old);
        color_stop stop={d,job.token};xide_color_completion answer={.token=job.token,.error=XIDE_COLOR_REMOTE};
        if(job.token && !jobStopped(&stop) && !field(job.message,"error"))
            answer.result=xideColorDecode(job.text,&job.legend,field(job.message,"result"),job.base,job.budget,jobStopped,&stop,&answer.error);
        releaseJob(&job);
        xrtMutexLock(&d->lock);old=NULL;
        if(job.token && !d->stop && d->current==job.token){
            old=d->completed.result;d->completed=answer;d->hasCompleted=1;answer.result=NULL;
        }
        xrtMutexUnlock(&d->lock);
        xideColorResultRelease(old);xideColorResultRelease(answer.result);
        if(d->wake)d->wake(d->user);
    }
    releaseJob(&d->pending);xideColorResultRelease(d->retired);xideColorResultRelease(d->completed.result);return 0;
}
xide_color_decoder* xideColorDecoderCreate(void (*wake)(void*),void* user)
{
    xide_color_decoder* d=xrtCalloc(1,sizeof(*d));if(!d)return NULL;
    if(!xrtMutexInit(&d->lock)){xrtFree(d);return NULL;}
    if(!xrtCondInit(&d->changed)){xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}
    d->wake=wake;d->user=user;d->thread=xrtThreadCreate(run,d,0);
    if(!d->thread){xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);return NULL;}return d;
}
int xideColorDecoderSubmit(xide_color_decoder* d,uint64_t token,xvalue* message,xls_text* text,const xide_color_legend* legend,xide_color_result* base,size_t budget)
{
    if(!d || !token || !message || !text || !validLegend(legend))return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending.message && token>d->lastToken;
    if(okay){d->current=d->lastToken=token;d->pending=(color_job){token,message,text,*legend,base,budget};xrtCondSignal(&d->changed);}
    xrtMutexUnlock(&d->lock);return okay;
}
int xideColorDecoderDiscard(xide_color_decoder* d,xvalue* message)
{
    if(!d || !message)return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->pending.message;
    if(okay){d->pending=(color_job){.message=message};xrtCondSignal(&d->changed);}
    xrtMutexUnlock(&d->lock);return okay;
}
int xideColorDecoderRetire(xide_color_decoder* d,xide_color_result* result)
{
    if(!d || !result)return 0;
    xrtMutexLock(&d->lock);int okay=!d->stop && !d->retired;
    if(okay){d->retired=result;xrtCondSignal(&d->changed);}
    xrtMutexUnlock(&d->lock);return okay;
}
void xideColorDecoderCancel(xide_color_decoder* d)
{if(d){xrtMutexLock(&d->lock);d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);}}
int xideColorDecoderTake(xide_color_decoder* d,xide_color_completion* out)
{
    if(!d || !out)return 0;
    xrtMutexLock(&d->lock);int okay=d->hasCompleted && d->completed.token==d->current;
    if(okay){*out=d->completed;memset(&d->completed,0,sizeof(d->completed));d->hasCompleted=0;}
    xrtMutexUnlock(&d->lock);return okay;
}
void xideColorDecoderDestroy(xide_color_decoder* d)
{
    if(!d)return;
    xrtMutexLock(&d->lock);d->stop=1;d->current=0;xrtCondSignal(&d->changed);xrtMutexUnlock(&d->lock);
    xrtThreadWait(d->thread);xrtThreadDestroy(d->thread);xrtCondUnit(&d->changed);xrtMutexUnit(&d->lock);xrtFree(d);
}
