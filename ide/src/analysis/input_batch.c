#include "analysis/input_batch.h"
#include "analysis/frontend_context.h"
#include "analysis/input_topology.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
struct xls_input_batch {
    xatomic32 references;
    xmap keys;
    xls_context* context;
    xls_analysis_document documents[64];
    size_t count,bytes,target,storageBytes;
    char* directory;
};
static xbytesview key(const char* s) { return (xbytesview){(const unsigned char*)s,strlen(s)}; }
static int label(const char* s)
{ size_t n=s ? strlen(s) : 0; return n && n<=262144 && xrtUtf8Valid((xstrview){s,n},NULL); }
static void fail(char* error,size_t size,const char* text) { if(error && size) snprintf(error,size,"%s",text); }
int xlsInputLimits(const xls_session_limits* requested,xls_session_limits* chosen)
{
    if(!chosen) return 0;
    *chosen=requested ? *requested : (xls_session_limits){64,8u*1024u*1024u,32u*1024u*1024u};
    return chosen->documents && chosen->documents<=64 && chosen->documentBytes &&
        chosen->documentBytes<=8u*1024u*1024u && chosen->documentBytes<=chosen->totalBytes &&
        chosen->totalBytes<=32u*1024u*1024u;
}
void xlsInputBatchDestroy(xls_input_batch* b)
{
    if(!b || xrtAtomic32FetchSub(&b->references,1,XMEMORY_ACQ_REL)!=1) return;
    xrtMapUnit(&b->keys);
    for(size_t i=0;i<b->count;++i) {
        xrtFree((void*)b->documents[i].key); xrtFree((void*)b->documents[i].path);
        xlsTextRelease(b->documents[i].text);
    }
    xlsContextRelease(b->context); xrtFree(b->directory); xrtFree(b);
}
xls_input_batch* xlsInputBatchRetain(const xls_input_batch* batch)
{xls_input_batch* b=(xls_input_batch*)batch;if(b)xrtAtomic32FetchAdd(&b->references,1,XMEMORY_RELAXED);return b;}
static size_t storageAdd(size_t left,size_t right){return right>SIZE_MAX-left ? SIZE_MAX : left+right;}
const xls_analysis_document* xlsInputBatchFind(const xls_input_batch* b,const char* name)
{
    if(!b || !name) return NULL;
    const size_t* at=xrtMapConstGet(&b->keys,key(name));
    return at ? &b->documents[*at] : NULL;
}
const xls_analysis_document* xlsInputBatchTarget(const xls_input_batch* b)
{ return b && b->target<b->count ? &b->documents[b->target] : NULL; }
const char* xlsInputBatchEntry(const xls_input_batch* b)
{
    const xls_analysis_document* target=xlsInputBatchTarget(b);
    return target ? target->path : b && b->context ? xlsContextSpec(b->context)->entry : NULL;
}
const char* xlsInputBatchDirectory(const xls_input_batch* b) { return b ? b->directory : NULL; }
const xls_context* xlsInputBatchContext(const xls_input_batch* b) { return b ? b->context : NULL; }
size_t xlsInputBatchBytes(const xls_input_batch* b) { return b ? b->bytes : 0; }
size_t xlsInputBatchStorageBytes(const xls_input_batch* b) {return b ? b->storageBytes : 0;}
int xlsInputSameBody(const xls_analysis_document* a,const xls_analysis_document* b)
{
    return a && b && (a->text==b->text || (xlsTextSize(a->text)==xlsTextSize(b->text) &&
        !memcmp(xlsTextData(a->text),xlsTextData(b->text),xlsTextSize(a->text))));
}
static xls_input_batch* createInContext(xls_context* context,const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity,int allowClosed)
{
    xls_session_limits chosen; xls_input_batch* b=NULL;
    if(error && capacity) *error=0;
    if(!xlsInputLimits(limits,&chosen) || (count && !documents) || (!count && (!allowClosed || target || !context)) || count>chosen.documents ||
       (target ? !label(target) : !context)) goto invalid;
    if(cancelled && cancelled(user)) goto cancel;
    b=xrtCalloc(1,sizeof(*b)); if(!b) goto memory;
    xrtAtomic32Init(&b->references,1);
    b->context=xlsContextRetain(context); b->target=SIZE_MAX; b->directory=context ? xrtStrDup(xlsContextSpec(context)->directory) : xrtPathAbs(".");
    if(!b->directory || !xrtMapInit(&b->keys,sizeof(size_t))) goto memory;
    b->storageBytes=storageAdd(storageAdd(sizeof(*b),strlen(b->directory)+1),xlsContextBytes(context));
    for(size_t i=0;i<count;++i) {
        if(cancelled && cancelled(user)) goto cancel;
        const xls_analysis_document* in=&documents[i];
        if(!label(in->key) || !label(in->path) || !xrtPathIsAbs(in->path) || !in->text ||
           xlsTextSize(in->text)>chosen.documentBytes || xlsTextSize(in->text)>chosen.totalBytes-b->bytes ||
           xrtMapHas(&b->keys,key(in->key))) goto invalid;
        const xls_analysis_document* old=xlsInputBatchFind(previous,in->key);
        if(old && old->identity==in->identity &&
           (in->revision<old->revision || (in->revision==old->revision && !xlsInputSameBody(old,in)))) goto invalid;
        char *name=xrtStrDup(in->key),*path=xrtStrDup(in->path);
        if(!name || !path) { xrtFree(name); xrtFree(path); goto memory; }
        size_t index=b->count++;
        b->documents[index]=(xls_analysis_document){name,path,xlsTextRetain(in->text),in->identity,in->revision};
        if(!xrtMapSet(&b->keys,key(name),&index)) goto memory;
        b->bytes+=xlsTextSize(in->text);
        b->storageBytes=storageAdd(storageAdd(storageAdd(b->storageBytes,xlsTextStorageBytes(in->text)),2*(strlen(name)+1)),strlen(path)+1);
        if(target && !strcmp(name,target)) b->target=index;
    }
    if(target && b->target==SIZE_MAX) goto invalid;
    if(!xlsFrontendContextValid(context,xlsInputBatchEntry(b),error,capacity)) goto done;
    return b;
invalid: fail(error,capacity,"invalid semantic batch identity, revision, path or capacity"); goto done;
memory: fail(error,capacity,"cannot retain semantic input batch"); goto done;
cancel: fail(error,capacity,"semantic session cancelled");
done: xlsInputBatchDestroy(b); return NULL;
}
xls_input_batch* xlsInputBatchCreateInContext(xls_context* context,const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return createInContext(context,limits,previous,documents,count,target,cancelled,user,error,capacity,0);}
xls_input_batch* xlsInputBatchCreateClosedInContext(xls_context* context,const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return createInContext(context,limits,previous,documents,count,target,cancelled,user,error,capacity,1);}
xls_input_batch* xlsInputBatchCreate(const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsInputBatchCreateInContext(NULL,limits,previous,documents,count,target,cancelled,user,error,capacity); }
static int equalBatch(const xls_input_batch* a,const xls_input_batch* b,uint64_t used,int all,const char* query,uint64_t* remapped)
{
    if(remapped)*remapped=0;
    if(!a || !b || !xlsFrontendContextsEqual(a->context,b->context) || a->count!=b->count || strcmp(a->directory,b->directory)) return 0;
    if(!all && a->count<64 && (used>>a->count))return 0;
    if(query && !xlsInputBatchFind(b,query))return 0;
    const xls_analysis_document *at=xlsInputBatchTarget(a),*bt=xlsInputBatchTarget(b);
    if((at!=NULL)!=(bt!=NULL) || (at && strcmp(at->key,bt->key))) return 0;
    uint64_t mapped=0;
    for(size_t i=0;i<b->count;++i) {
        const xls_analysis_document* in=&b->documents[i];const size_t* index=xrtMapConstGet(&a->keys,key(in->key));
        const xls_analysis_document* old=index ? &a->documents[*index] : NULL;
        if(!old || old->identity!=in->identity || strcmp(old->path,in->path))return 0;
        int observed=(used&(UINT64_C(1)<<*index))!=0;
        if((all || observed || old==at || (query && !strcmp(in->key,query))) && !xlsInputSameBody(old,in))return 0;
        if(observed)mapped|=UINT64_C(1)<<i;
    }
    if(remapped)*remapped=mapped;
    return 1;
}
int xlsInputBatchEqual(const xls_input_batch* a,const xls_input_batch* b)
{return equalBatch(a,b,0,1,NULL,NULL);}
int xlsInputBatchEqualUsed(const xls_input_batch* a,const xls_input_batch* b,uint64_t used,const char* query,uint64_t* remapped)
{return remapped && equalBatch(a,b,used,0,query,remapped);}
int xlsInputBatchEqualTopology(const xls_input_batch* a,const xls_input_batch* b,uint64_t used,const char* query,
    const xls_input_topology* topology,uint64_t* remapped)
{
    if(remapped)*remapped=0;
    if(!remapped || !topology || !a || !b || !xlsFrontendContextsEqual(a->context,b->context) ||
       strcmp(a->directory,b->directory) || (a->count<64 && used>>a->count))return 0;
    const xls_analysis_document *at=xlsInputBatchTarget(a),*bt=xlsInputBatchTarget(b);
    if((at!=NULL)!=(bt!=NULL) || (at && strcmp(at->key,bt->key)))return 0;
    if(query && (!xlsInputBatchFind(a,query) || !xlsInputBatchFind(b,query)))return 0;
    const char* changed[128];size_t changes=0;uint64_t mapped=0;
    for(size_t i=0;i<a->count;++i){
        const xls_analysis_document* old=&a->documents[i];const size_t* index=xrtMapConstGet(&b->keys,key(old->key));
        const xls_analysis_document* now=index ? &b->documents[*index] : NULL;
        int observed=(used&(UINT64_C(1)<<i))!=0,required=observed || old==at || (query && !strcmp(old->key,query));
        int same=now && old->identity==now->identity && !strcmp(old->path,now->path);
        if(required && (!same || !xlsInputSameBody(old,now)))return 0;
        if(observed)mapped|=UINT64_C(1)<<*index;
        if(!same)changed[changes++]=old->path;
    }
    const char* current[64];
    for(size_t i=0;i<b->count;++i){
        const xls_analysis_document* now=&b->documents[i];const xls_analysis_document* old=xlsInputBatchFind(a,now->key);current[i]=now->path;
        if(!old || old->identity!=now->identity || strcmp(old->path,now->path))changed[changes++]=now->path;
    }
    if(!xlsInputTopologyAdmits(topology,changed,changes,current,b->count))return 0;
    *remapped=mapped;return 1;
}
size_t xlsInputBatchInputs(const xls_input_batch* b,xls_semantic_input* inputs,size_t capacity)
{
    if(!b || !inputs || b->count>capacity) return 0;
    for(size_t i=0;i<b->count;++i) {
        const xls_analysis_document* d=&b->documents[i];
        inputs[i]=(xls_semantic_input){d->path,xlsTextData(d->text),xlsTextSize(d->text)};
    }
    return b->count;
}
