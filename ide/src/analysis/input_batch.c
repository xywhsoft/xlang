#include "analysis/input_batch.h"
#include "analysis/frontend_context.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
struct xls_input_batch {
    xmap keys;
    xls_context* context;
    xls_analysis_document documents[64];
    size_t count,bytes,target;
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
    if(!b) return;
    xrtMapUnit(&b->keys);
    for(size_t i=0;i<b->count;++i) {
        xrtFree((void*)b->documents[i].key); xrtFree((void*)b->documents[i].path);
        xlsTextRelease(b->documents[i].text);
    }
    xlsContextRelease(b->context); xrtFree(b->directory); xrtFree(b);
}
const xls_analysis_document* xlsInputBatchFind(const xls_input_batch* b,const char* name)
{
    if(!b || !name) return NULL;
    const size_t* at=xrtMapConstGet(&b->keys,key(name));
    return at ? &b->documents[*at] : NULL;
}
const xls_analysis_document* xlsInputBatchTarget(const xls_input_batch* b)
{ return b ? &b->documents[b->target] : NULL; }
const char* xlsInputBatchDirectory(const xls_input_batch* b) { return b ? b->directory : NULL; }
const xls_context* xlsInputBatchContext(const xls_input_batch* b) { return b ? b->context : NULL; }
size_t xlsInputBatchBytes(const xls_input_batch* b) { return b ? b->bytes : 0; }
int xlsInputSameBody(const xls_analysis_document* a,const xls_analysis_document* b)
{
    return a && b && (a->text==b->text || (xlsTextSize(a->text)==xlsTextSize(b->text) &&
        !memcmp(xlsTextData(a->text),xlsTextData(b->text),xlsTextSize(a->text))));
}
xls_input_batch* xlsInputBatchCreateInContext(xls_context* context,const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_session_limits chosen; xls_input_batch* b=NULL;
    if(error && capacity) *error=0;
    if(!xlsInputLimits(limits,&chosen) || !documents || !count || count>chosen.documents || !label(target)) goto invalid;
    if(cancelled && cancelled(user)) goto cancel;
    b=xrtCalloc(1,sizeof(*b)); if(!b) goto memory;
    b->context=xlsContextRetain(context); b->target=SIZE_MAX; b->directory=context ? xrtStrDup(xlsContextSpec(context)->directory) : xrtPathAbs(".");
    if(!b->directory || !xrtMapInit(&b->keys,sizeof(size_t))) goto memory;
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
        if(!strcmp(name,target)) b->target=index;
    }
    if(b->target==SIZE_MAX) goto invalid;
    if(!xlsFrontendContextValid(context,b->documents[b->target].path,error,capacity)) goto done;
    return b;
invalid: fail(error,capacity,"invalid semantic batch identity, revision, path or capacity"); goto done;
memory: fail(error,capacity,"cannot retain semantic input batch"); goto done;
cancel: fail(error,capacity,"semantic session cancelled");
done: xlsInputBatchDestroy(b); return NULL;
}
xls_input_batch* xlsInputBatchCreate(const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsInputBatchCreateInContext(NULL,limits,previous,documents,count,target,cancelled,user,error,capacity); }
int xlsInputBatchEqual(const xls_input_batch* a,const xls_input_batch* b)
{
    if(!a || !b || !xlsFrontendContextsEqual(a->context,b->context) || a->count!=b->count || strcmp(a->directory,b->directory) ||
       strcmp(xlsInputBatchTarget(a)->key,xlsInputBatchTarget(b)->key)) return 0;
    for(size_t i=0;i<b->count;++i) {
        const xls_analysis_document *in=&b->documents[i],*old=xlsInputBatchFind(a,in->key);
        if(!old || old->identity!=in->identity || strcmp(old->path,in->path) || !xlsInputSameBody(old,in)) return 0;
    }
    return 1;
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
