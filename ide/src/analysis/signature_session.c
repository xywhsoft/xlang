#include "analysis/signature_session.h"
#include "analysis/signature_internal.h"
#include "analysis/input_batch.h"
#include "analysis/session_state.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include <stdio.h>
#include <string.h>
struct xls_signature_session {
    xls_session_limits limits;xls_input_batch* batch;
    const char* queryKey;size_t offset;xls_signature_help* result;
    xls_entry_cache* entryCache;xls_signature_stats stats;
    uint64_t usedInputs;int usageKnown;
};
xls_signature_session* xlsSignatureSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen;if(!xlsInputLimits(limits,&chosen))return NULL;
    if(chosen.documents>64)chosen.documents=64;
    xls_signature_session* s=xrtCalloc(1,sizeof(*s));if(s)s->limits=chosen;return s;
}
void xlsSignatureSessionDestroy(xls_signature_session* s)
{if(s){xlsSignatureRelease(s->result);xlsInputBatchDestroy(s->batch);xlsEntryCacheDestroy(s->entryCache);xrtFree(s);}}
void xlsSignatureSessionStats(const xls_signature_session* s,xls_signature_stats* stats)
{if(stats)*stats=s ? s->stats : (xls_signature_stats){0};}
xls_session_state xlsSignatureSessionState(const xls_signature_session* s)
{return (xls_session_state){s ? s->batch : NULL,s ? s->queryKey : NULL};}
xls_semantic_checkpoint xlsSignatureSessionCheckpoint(const xls_signature_session* s)
{
    if(!s)return (xls_semantic_checkpoint){0};
    return (xls_semantic_checkpoint){.inputs=xlsInputBatchRetain(s->batch),.signature=xlsSignatureRetain(s->result),.queryKey=s->queryKey,
        .offset=s->offset,.usedInputs=s->usedInputs,.usageKnown=s->usageKnown,.documents=s->stats.documents,.textBytes=s->stats.textBytes};
}
void xlsSignatureSessionRestore(xls_signature_session* s,xls_semantic_checkpoint* old)
{
    if(!s || !old)return;
    xlsInputBatchDestroy(s->batch);xlsSignatureRelease(s->result);s->batch=old->inputs;s->result=old->signature;
    s->queryKey=old->queryKey;s->offset=old->offset;s->usedInputs=old->usedInputs;s->usageKnown=old->usageKnown;
    s->stats.documents=old->documents;s->stats.textBytes=old->textBytes;*old=(xls_semantic_checkpoint){0};
}
xls_signature_help* xlsSignatureSessionQueryWithHistory(xls_signature_session* s,xls_context* context,const xls_input_batch* history,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL;xls_signature_help* result=NULL;if(error && capacity)*error=0;
    if(!s)goto invalid;
    batch=xlsInputBatchCreateInContext(context,&s->limits,history,documents,count,entry,cancelled,user,error,capacity);if(!batch)goto done;
    const xls_analysis_document* query=xlsInputBatchFind(batch,queryKey);if(!query)goto invalid;
    size_t bytes=xlsTextSize(query->text),bom=bytes>=3 && !memcmp(xlsTextData(query->text),"\xef\xbb\xbf",3) ? 3 : 0;
    if(offset>bytes-bom || (offset<bytes-bom && ((unsigned char)xlsTextData(query->text)[offset+bom]&0xc0)==0x80) ||
       (offset && offset<bytes-bom && xlsTextData(query->text)[offset+bom-1]=='\r' && xlsTextData(query->text)[offset+bom]=='\n'))goto invalid;
    uint64_t remapped=0;int ordinary=s->result && xlsInputBatchEqual(s->batch,batch);
    int used=s->result && s->usageKnown && xlsInputBatchEqualUsed(s->batch,batch,s->usedInputs,queryKey,&remapped);
    int caret=0;int equal=s->result && s->offset==offset && !strcmp(s->queryKey,query->key) && (ordinary || used);
    if(equal && !xlsSignatureValidate(s->result)){equal=0;++s->stats.invalidations;}
    if(cancelled && cancelled(user))goto cancel;
    if(equal)result=xlsSignatureRetain(s->result);
    else if(s->result && !strcmp(s->queryKey,query->key) && (ordinary || used) &&
            (result=xlsSignatureReposition(s->result,offset))!=NULL){caret=1;}
    else{
        ++s->stats.builds;int compatible=s->result && s->batch && count==s->stats.documents && xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) &&
            !strcmp(xlsInputBatchDirectory(batch),xlsInputBatchDirectory(s->batch)) && !strcmp(xlsInputBatchEntry(batch),xlsInputBatchEntry(s->batch));
        for(size_t i=0;compatible && i<count;++i){const xls_analysis_document* old=xlsInputBatchFind(s->batch,documents[i].key);compatible=old && old->identity==documents[i].identity && !strcmp(old->path,documents[i].path);}
        const xls_analysis_document* old=xlsInputBatchFind(s->batch,queryKey);xls_call_context* index=NULL;
        if(s->result && old && s->queryKey && !strcmp(s->queryKey,queryKey) && old->identity==query->identity && !strcmp(old->path,query->path) && xlsInputSameBody(old,query))index=xlsSignatureData(s->result)->index;
        if(!s->entryCache)s->entryCache=xlsEntryCacheCreate(32u*1024u*1024u);
        xls_semantic_input inputs[64];xlsInputBatchInputs(batch,inputs,64);
        result=xlsSignatureCreateRetainedInContext(context,xlsInputBatchEntry(batch),query->path,offset,inputs,count,index,s->entryCache,compatible ? s->result : NULL,cancelled,user,error,capacity);
        if(!result)goto done;
        if(index && xlsSignatureData(result)->index==index)++s->stats.indexHits;else ++s->stats.indexBuilds;
    }
    if(cancelled && cancelled(user)){xlsSignatureRelease(result);result=NULL;goto cancel;}
    if(equal)++s->stats.hits;
    if(caret){++s->stats.caretHits;++s->stats.indexHits;}
    if(equal || caret){if(used && !ordinary)++s->stats.inputScopeHits;s->usageKnown=used;s->usedInputs=remapped;}
    else s->usageKnown=xlsSemanticCompletionInputUsage(xlsSignatureSemantic(result),&s->usedInputs);
    s->queryKey=query->key;s->offset=offset;xlsInputBatchDestroy(s->batch);s->batch=batch;batch=NULL;
    xlsSignatureRelease(s->result);s->result=xlsSignatureRetain(result);s->stats.documents=count;s->stats.textBytes=xlsInputBatchBytes(s->batch);goto done;
invalid:if(error && capacity)snprintf(error,capacity,"invalid signature batch,target or UTF-8 cursor");goto done;
cancel:if(error && capacity)snprintf(error,capacity,"signature query cancelled");
done:xlsInputBatchDestroy(batch);return result;
}
xls_signature_help* xlsSignatureSessionQueryInContext(xls_signature_session* s,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return xlsSignatureSessionQueryWithHistory(s,context,s ? s->batch : NULL,documents,count,entry,queryKey,offset,cancelled,user,error,capacity);}
