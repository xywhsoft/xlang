#include "analysis/check_session.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include "analysis/input_batch.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

struct xls_check_session {
    xls_input_batch* batch;
    int captureIndex;
    xls_session_limits limits;
    xls_semantic_check* result;
    xls_syntax* entrySyntax;
    xls_check_stats stats;
};
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
static void fail(char* error,size_t size,const char* message) { if(error && size) snprintf(error,size,"%s",message); }
xls_check_session* xlsCheckSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen; if(!xlsInputLimits(limits,&chosen)) return NULL;
    xls_check_session* s=xrtCalloc(1,sizeof(*s)); if(s) s->limits=chosen; return s;
}
void xlsCheckSessionDestroy(xls_check_session* s)
{ if(s) { xlsInputBatchDestroy(s->batch); xlsSyntaxRelease(s->entrySyntax); xlsSemanticCheckDestroy(s->result); xrtFree(s); } }
void xlsCheckSessionStats(const xls_check_session* s,xls_check_stats* stats)
{ if(stats) *stats=s ? s->stats : (xls_check_stats){0}; }
xls_semantic_check* xlsCheckSessionCheckInContext(xls_check_session* s,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL; xls_semantic_check* result=NULL;
    xls_syntax* syntax=NULL;
    if(error && capacity) *error=0;
    if(!s || (captureIndex!=0 && captureIndex!=1)) goto invalid;
    batch=xlsInputBatchCreateInContext(context,&s->limits,s->batch,documents,count,target,cancelled,user,error,capacity);
    if(!batch) goto done;
    const xls_analysis_document* entryDocument=xlsInputBatchTarget(batch);
    const char* entry=xlsInputBatchEntry(batch);
    int equal=s->result && s->captureIndex==captureIndex && xlsInputBatchEqual(s->batch,batch);
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
        xls_semantic_check* baseline=previous && s->captureIndex==captureIndex &&
            !strcmp(xlsInputBatchDirectory(s->batch),xlsInputBatchDirectory(batch)) ? s->result : NULL;
        result=xlsSemanticCheckCreateIncrementalInContext(context,entry,inputs,count,captureIndex,syntax,baseline,cancelled,user,error,capacity);
        if(!result) goto done;
        xls_check_semantic_work work=xlsSemanticCheckWork(result);
        s->stats.semanticFunctionsChecked+=work.functionsChecked; s->stats.semanticFunctionsReused+=work.functionsReused;
        s->stats.semanticContextHits+=work.contextHits;
    }
    if(stopped(cancelled,user)) { xlsSemanticCheckDestroy(result); result=NULL; goto cancel; }
    if(equal) ++s->stats.hits;
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

xls_semantic_check* xlsCheckSessionCheck(xls_check_session* session,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsCheckSessionCheckInContext(session,NULL,documents,count,target,captureIndex,cancelled,user,error,capacity); }
