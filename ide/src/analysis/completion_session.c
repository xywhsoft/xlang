#include "analysis/completion_session.h"
#include "analysis/frontend_context.h"
#include "analysis/input_batch.h"
#include "analysis/session_state.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_entry_cache.h"
#include "src/sem/xl6_completion.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
struct xls_completion_session {
    xls_session_limits limits;
    xls_input_batch* batch;
    const char* queryKey;
    size_t offset;
    xls_semantic_completion* result;
    xls_syntax* cursorSyntax;
    xls_entry_cache* entryCache;
    xls_completion_stats stats;
    uint64_t usedInputs;int usageKnown;
};
xls_completion_session* xlsCompletionSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen; if(!xlsInputLimits(limits,&chosen)) return NULL;
    xls_completion_session* s=xrtCalloc(1,sizeof(*s)); if(s) s->limits=chosen; return s;
}
void xlsCompletionSessionDestroy(xls_completion_session* s)
{ if(s) { xlsInputBatchDestroy(s->batch); xlsSyntaxRelease(s->cursorSyntax); xlsSemanticCompletionDestroy(s->result); xlsEntryCacheDestroy(s->entryCache); xrtFree(s); } }
void xlsCompletionSessionStats(const xls_completion_session* s,xls_completion_stats* stats)
{ if(stats) *stats=s ? s->stats : (xls_completion_stats){0}; }
xls_session_state xlsCompletionSessionState(const xls_completion_session* s)
{return (xls_session_state){s ? s->batch : NULL,s ? s->queryKey : NULL};}
xls_semantic_checkpoint xlsCompletionSessionCheckpoint(const xls_completion_session* s)
{
    if(!s)return (xls_semantic_checkpoint){0};
    return (xls_semantic_checkpoint){.inputs=xlsInputBatchRetain(s->batch),.completion=xlsSemanticCompletionRetain(s->result),
        .syntax=xlsSyntaxRetain(s->cursorSyntax),.queryKey=s->queryKey,.offset=s->offset,.usedInputs=s->usedInputs,.usageKnown=s->usageKnown,
        .documents=s->stats.documents,.textBytes=s->stats.textBytes};
}
void xlsCompletionSessionRestore(xls_completion_session* s,xls_semantic_checkpoint* old)
{
    if(!s || !old)return;
    xlsInputBatchDestroy(s->batch);xlsSemanticCompletionDestroy(s->result);xlsSyntaxRelease(s->cursorSyntax);
    s->batch=old->inputs;s->result=old->completion;s->cursorSyntax=old->syntax;s->queryKey=old->queryKey;s->offset=old->offset;s->usedInputs=old->usedInputs;s->usageKnown=old->usageKnown;
    s->stats.documents=old->documents;s->stats.textBytes=old->textBytes;*old=(xls_semantic_checkpoint){0};
}
xls_semantic_completion* xlsCompletionSessionQueryWithHistory(xls_completion_session* s,xls_context* context,const xls_input_batch* history,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL; xls_semantic_completion* result=NULL;
    xls_syntax* syntax=NULL;
    if(error && capacity) *error=0;
    if(!s) goto invalid;
    batch=xlsInputBatchCreateInContext(context,&s->limits,history,documents,count,entryKey,cancelled,user,error,capacity);
    if(!batch) goto done;
    const xls_analysis_document* query=xlsInputBatchFind(batch,queryKey);
    if(!query) goto invalid;
    size_t bytes=xlsTextSize(query->text),bom=bytes>=3 && !memcmp(xlsTextData(query->text),"\xef\xbb\xbf",3) ? 3 : 0;
    if(offset>bytes-bom || (offset<bytes-bom && ((unsigned char)xlsTextData(query->text)[offset+bom]&0xc0)==0x80)) goto invalid;
    uint64_t remapped=0;int ordinary=s->result && xlsInputBatchEqual(s->batch,batch);
    int used=s->result && s->usageKnown && xlsInputBatchEqualUsed(s->batch,batch,s->usedInputs,queryKey,&remapped);
    int equal=s->result && s->offset==offset && !strcmp(s->queryKey,query->key) && (ordinary || used);
    if(equal && !xlsSemanticCompletionValidate(s->result)) { equal=0; ++s->stats.invalidations; }
    if(cancelled && cancelled(user)) goto cancel;
    if(equal) result=xlsSemanticCompletionRetain(s->result);
    else {
        xls_semantic_input inputs[64]; xlsInputBatchInputs(batch,inputs,64); ++s->stats.builds;
        int compatible=s->result && s->batch && count==s->stats.documents &&
            xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) &&
            !strcmp(xlsInputBatchDirectory(batch),xlsInputBatchDirectory(s->batch)) &&
            !strcmp(xlsInputBatchEntry(batch),xlsInputBatchEntry(s->batch));
        for(size_t i=0;compatible && i<count;++i) {
            const xls_analysis_document* old=xlsInputBatchFind(s->batch,documents[i].key);
            compatible=old && old->identity==documents[i].identity && !strcmp(old->path,documents[i].path);
        }
        xls_semantic_completion* baseline=compatible ? s->result : NULL;
        if(!s->entryCache) s->entryCache=xlsEntryCacheCreate(32u*1024u*1024u);
        {
            const xls_analysis_document* old=xlsInputBatchFind(s->batch,queryKey);
            const xls_syntax* previous=xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) && old && s->cursorSyntax && s->queryKey && !strcmp(s->queryKey,old->key) &&
                !strcmp(old->key,query->key) && old->identity==query->identity && !strcmp(old->path,query->path) ? s->cursorSyntax : NULL;
            if(previous && s->offset==offset && xlsInputSameBody(old,query)) {
                syntax=xlsSyntaxRetain(s->cursorSyntax); ++s->stats.cursorSyntaxHits;
            } else {
                char* path=xrtPathAbs(query->path);
                if(path) {
                    XL6_CompletionQuery projection={.path=path,.offset=offset}; xls_parse_work work={0};
                    syntax=xlsSyntaxBuildCursor(path,query->text,previous,&projection,&work);
                    xl6CompletionFree(&projection); xrtFree(path);
                    s->stats.cursorFullParses+=work.full; s->stats.cursorFragmentParses+=work.fragments;
                    s->stats.cursorReusedDeclarations+=work.reused; s->stats.cursorParsedBytes+=work.bytes;
                }
            }
            if(cancelled && cancelled(user)) goto cancel;
            if(syntax) result=xlsSemanticCompletionCreateIncrementalInContext(context,s->entryCache,xlsInputBatchEntry(batch),query->path,offset,inputs,count,syntax,baseline,cancelled,user,error,capacity);
        }
        if(!result) {
            if(cancelled && cancelled(user)) goto cancel;
            /* A cache/clone limit must not shrink the existing language
             * surface. This is the original native frontend, never CLI;
             * the explicit route counter distinguishes it from reuse. */
            ++s->stats.cursorFreshBuilds;
            result=xlsSemanticCompletionCreateIncrementalInContext(context,s->entryCache,xlsInputBatchEntry(batch),query->path,offset,inputs,count,NULL,baseline,cancelled,user,error,capacity);
            xlsSyntaxRelease(syntax); syntax=NULL;
        }
        if(!result) goto done;
    }
    if(cancelled && cancelled(user)) { xlsSemanticCompletionDestroy(result); result=NULL; goto cancel; }
    if(equal){++s->stats.hits;if(used && !ordinary)++s->stats.inputScopeHits;s->usageKnown=used;s->usedInputs=remapped;}
    else s->usageKnown=xlsSemanticCompletionInputUsage(result,&s->usedInputs);
    s->queryKey=query->key; s->offset=offset;
    xlsInputBatchDestroy(s->batch); s->batch=batch; batch=NULL;
    xlsSemanticCompletionDestroy(s->result); s->result=xlsSemanticCompletionRetain(result);
    if(!equal) { xlsSyntaxRelease(s->cursorSyntax); s->cursorSyntax=syntax; syntax=NULL; }
    s->stats.documents=count; s->stats.textBytes=xlsInputBatchBytes(s->batch); goto done;
invalid: if(error && capacity) snprintf(error,capacity,"invalid completion batch, target or UTF-8 cursor"); goto done;
cancel: if(error && capacity) snprintf(error,capacity,"completion session cancelled");
done: xlsSyntaxRelease(syntax); xlsInputBatchDestroy(batch); return result;
}

xls_semantic_completion* xlsCompletionSessionQueryInContext(xls_completion_session* s,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entryKey,const char* queryKey,size_t offset,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return xlsCompletionSessionQueryWithHistory(s,context,s ? s->batch : NULL,documents,count,entryKey,queryKey,offset,cancelled,user,error,capacity);}
xls_semantic_completion* xlsCompletionSessionQuery(xls_completion_session* session,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsCompletionSessionQueryInContext(session,NULL,documents,count,entryKey,queryKey,offset,cancelled,user,error,capacity); }
