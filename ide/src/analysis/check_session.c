#include "analysis/check_session.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/input_batch.h"
#include "analysis/session_state.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

struct xls_check_session {
    xls_input_batch* batch;
    int captureIndex;
    xls_session_limits limits;
    xls_semantic_check* result;
    xls_syntax* entrySyntax;
    xls_entry_cache* entryCache;
    xls_check_stats stats;
    uint64_t usedInputs;int usageKnown;
};
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
static void fail(char* error,size_t size,const char* message) { if(error && size) snprintf(error,size,"%s",message); }
xls_check_session* xlsCheckSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen; if(!xlsInputLimits(limits,&chosen)) return NULL;
    xls_check_session* s=xrtCalloc(1,sizeof(*s)); if(s) s->limits=chosen; return s;
}
void xlsCheckSessionDestroy(xls_check_session* s)
{ if(s) { xlsInputBatchDestroy(s->batch); xlsSyntaxRelease(s->entrySyntax); xlsSemanticCheckDestroy(s->result); xlsEntryCacheDestroy(s->entryCache); xrtFree(s); } }
void xlsCheckSessionStats(const xls_check_session* s,xls_check_stats* stats)
{ if(stats) *stats=s ? s->stats : (xls_check_stats){0}; }
xls_session_state xlsCheckSessionState(const xls_check_session* s)
{return (xls_session_state){s ? s->batch : NULL,NULL};}
xls_semantic_checkpoint xlsCheckSessionCheckpoint(const xls_check_session* s)
{
    if(!s)return (xls_semantic_checkpoint){0};
    return (xls_semantic_checkpoint){.inputs=xlsInputBatchRetain(s->batch),.check=xlsSemanticCheckRetain(s->result),
        .syntax=xlsSyntaxRetain(s->entrySyntax),.usedInputs=s->usedInputs,.usageKnown=s->usageKnown,.captureIndex=s->captureIndex,
        .documents=s->stats.documents,.textBytes=s->stats.textBytes,.memoBytes=s->stats.semanticMemoBytes};
}
void xlsCheckSessionRestore(xls_check_session* s,xls_semantic_checkpoint* old)
{
    if(!s || !old)return;
    xlsInputBatchDestroy(s->batch);xlsSemanticCheckDestroy(s->result);xlsSyntaxRelease(s->entrySyntax);
    s->batch=old->inputs;s->result=old->check;s->entrySyntax=old->syntax;s->usedInputs=old->usedInputs;s->usageKnown=old->usageKnown;s->captureIndex=old->captureIndex;
    s->stats.documents=old->documents;s->stats.textBytes=old->textBytes;s->stats.semanticMemoBytes=old->memoBytes;*old=(xls_semantic_checkpoint){0};
}
xls_semantic_check* xlsCheckSessionCheckWithHistory(xls_check_session* s,xls_context* context,const xls_input_batch* history,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL; xls_semantic_check* result=NULL;
    xls_syntax* syntax=NULL;
    if(error && capacity) *error=0;
    if(!s || (captureIndex!=0 && captureIndex!=1)) goto invalid;
    batch=xlsInputBatchCreateInContext(context,&s->limits,history,documents,count,target,cancelled,user,error,capacity);
    if(!batch) goto done;
    const xls_analysis_document* entryDocument=xlsInputBatchTarget(batch);
    const char* entry=xlsInputBatchEntry(batch);
    uint64_t remapped=0;int ordinary=s->result && s->captureIndex==captureIndex && xlsInputBatchEqual(s->batch,batch);
    int used=s->result && s->captureIndex==captureIndex && s->usageKnown && xlsInputBatchEqualUsed(s->batch,batch,s->usedInputs,NULL,&remapped);
    int equal=ordinary || used;
    xls_semantic_input inputs[64]; xlsInputBatchInputs(batch,inputs,64);
    if(equal && !xlsSemanticCheckValidate(s->result)) { equal=0; ++s->stats.invalidations; }
    if(stopped(cancelled,user)) goto cancel;
    if(equal) result=xlsSemanticCheckRetain(s->result);
    else {
        ++s->stats.builds;
        const xls_analysis_document* oldTarget=xlsInputBatchTarget(s->batch);
        const xls_analysis_document* old=entryDocument && oldTarget && !strcmp(oldTarget->key,target) ? xlsInputBatchFind(s->batch,target) : NULL;
        const xls_syntax* previous=xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) && old && old->identity==entryDocument->identity && !strcmp(old->path,entry) ? s->entrySyntax : NULL;
        if(previous && xlsInputSameBody(old,entryDocument)) { syntax=xlsSyntaxRetain(s->entrySyntax); ++s->stats.entrySyntaxHits; }
        else if(entryDocument) {
            char* path=xrtPathAbs(entry); if(!path) goto memory;
            xls_parse_work work={0}; syntax=xlsSyntaxBuild(path,entryDocument->text,previous,&work); xrtFree(path);
            s->stats.entryFullParses+=work.full; s->stats.entryFragmentParses+=work.fragments;
            s->stats.entryReusedDeclarations+=work.reused; s->stats.entryParsedBytes+=work.bytes;
            if(!syntax) goto memory;
        }
        if(stopped(cancelled,user)) goto cancel;
        int compatible=s->result && s->captureIndex==captureIndex &&
            xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) &&
            !strcmp(xlsInputBatchEntry(s->batch),entry) &&
            !strcmp(xlsInputBatchDirectory(s->batch),xlsInputBatchDirectory(batch));
        for(size_t i=0;compatible && i<count;++i) {
            const xls_analysis_document* old=xlsInputBatchFind(s->batch,documents[i].key);
            compatible=old && old->identity==documents[i].identity && !strcmp(old->path,documents[i].path);
        }
        xls_semantic_check* baseline=compatible ? s->result : NULL;
        if(!s->entryCache) s->entryCache=xlsEntryCacheCreate(32u*1024u*1024u);
        result=xlsSemanticCheckCreateCachedInContext(context,s->entryCache,entry,inputs,count,captureIndex,syntax,baseline,cancelled,user,error,capacity);
        if(!result) goto done;
        xls_check_semantic_work work=xlsSemanticCheckWork(result);
        s->stats.semanticFunctionsChecked+=work.functionsChecked; s->stats.semanticFunctionsReused+=work.functionsReused;
        s->stats.semanticContextHits+=work.contextHits;
    }
    if(stopped(cancelled,user)) { xlsSemanticCheckDestroy(result); result=NULL; goto cancel; }
    if(equal){++s->stats.hits;if(used && !ordinary)++s->stats.inputScopeHits;s->usageKnown=used;s->usedInputs=remapped;}
    else s->usageKnown=xlsSemanticCheckInputUsage(result,&s->usedInputs);
    xlsInputBatchDestroy(s->batch); s->batch=batch; batch=NULL;
    xlsSemanticCheckDestroy(s->result); s->result=xlsSemanticCheckRetain(result);
    s->captureIndex=captureIndex; s->stats.documents=count; s->stats.textBytes=xlsInputBatchBytes(s->batch);
    s->stats.semanticMemoBytes=xlsSemanticCheckWork(result).memoBytes;
    if(!equal) { xlsSyntaxRelease(s->entrySyntax); s->entrySyntax=syntax; syntax=NULL; }
    goto done;
invalid: fail(error,capacity,"invalid semantic batch identity, revision, path or capacity"); goto done;
memory: fail(error,capacity,"cannot retain semantic input batch"); goto done;
cancel: fail(error,capacity,"semantic session cancelled");
done: xlsSyntaxRelease(syntax); xlsInputBatchDestroy(batch); return result;
}

xls_semantic_check* xlsCheckSessionCheckInContext(xls_check_session* s,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return xlsCheckSessionCheckWithHistory(s,context,s ? s->batch : NULL,documents,count,target,captureIndex,cancelled,user,error,capacity);}
xls_semantic_check* xlsCheckSessionCheck(xls_check_session* session,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsCheckSessionCheckInContext(session,NULL,documents,count,target,captureIndex,cancelled,user,error,capacity); }
