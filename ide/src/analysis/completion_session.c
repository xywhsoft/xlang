#include "analysis/completion_session.h"
#include "analysis/input_batch.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
struct xls_completion_session {
    xls_session_limits limits;
    xls_input_batch* batch;
    const char* queryKey;
    size_t offset;
    xls_semantic_completion* result;
    xls_completion_stats stats;
};
xls_completion_session* xlsCompletionSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen; if(!xlsInputLimits(limits,&chosen)) return NULL;
    xls_completion_session* s=xrtCalloc(1,sizeof(*s)); if(s) s->limits=chosen; return s;
}
void xlsCompletionSessionDestroy(xls_completion_session* s)
{ if(s) { xlsInputBatchDestroy(s->batch); xlsSemanticCompletionDestroy(s->result); xrtFree(s); } }
void xlsCompletionSessionStats(const xls_completion_session* s,xls_completion_stats* stats)
{ if(stats) *stats=s ? s->stats : (xls_completion_stats){0}; }
xls_semantic_completion* xlsCompletionSessionQuery(xls_completion_session* s,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL; xls_semantic_completion* result=NULL;
    if(error && capacity) *error=0;
    if(!s) goto invalid;
    batch=xlsInputBatchCreate(&s->limits,s->batch,documents,count,entryKey,cancelled,user,error,capacity);
    if(!batch) goto done;
    const xls_analysis_document *entry=xlsInputBatchTarget(batch),*query=xlsInputBatchFind(batch,queryKey);
    if(!query) goto invalid;
    size_t bytes=xlsTextSize(query->text),bom=bytes>=3 && !memcmp(xlsTextData(query->text),"\xef\xbb\xbf",3) ? 3 : 0;
    if(offset>bytes-bom || (offset<bytes-bom && ((unsigned char)xlsTextData(query->text)[offset+bom]&0xc0)==0x80)) goto invalid;
    int equal=s->result && s->offset==offset && !strcmp(s->queryKey,query->key) && xlsInputBatchEqual(s->batch,batch);
    if(equal && !xlsSemanticCompletionValidate(s->result)) { equal=0; ++s->stats.invalidations; }
    if(cancelled && cancelled(user)) goto cancel;
    if(equal) result=xlsSemanticCompletionRetain(s->result);
    else {
        xls_semantic_input inputs[64]; xlsInputBatchInputs(batch,inputs,64); ++s->stats.builds;
        result=xlsSemanticCompletionCreate(entry->path,query->path,offset,inputs,count,cancelled,user,error,capacity);
        if(!result) goto done;
    }
    if(cancelled && cancelled(user)) { xlsSemanticCompletionDestroy(result); result=NULL; goto cancel; }
    if(equal) ++s->stats.hits;
    s->queryKey=query->key; s->offset=offset;
    xlsInputBatchDestroy(s->batch); s->batch=batch; batch=NULL;
    xlsSemanticCompletionDestroy(s->result); s->result=xlsSemanticCompletionRetain(result);
    s->stats.documents=count; s->stats.textBytes=xlsInputBatchBytes(s->batch); goto done;
invalid: if(error && capacity) snprintf(error,capacity,"invalid completion batch, target or UTF-8 cursor"); goto done;
cancel: if(error && capacity) snprintf(error,capacity,"completion session cancelled");
done: xlsInputBatchDestroy(batch); return result;
}
