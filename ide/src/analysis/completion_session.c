#include "analysis/completion_session.h"
#include "analysis/frontend_context.h"
#include "analysis/input_batch.h"
#include "analysis/frontend_internal.h"
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
    xls_syntax* entrySyntax;
    xls_completion_stats stats;
};
xls_completion_session* xlsCompletionSessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen; if(!xlsInputLimits(limits,&chosen)) return NULL;
    xls_completion_session* s=xrtCalloc(1,sizeof(*s)); if(s) s->limits=chosen; return s;
}
void xlsCompletionSessionDestroy(xls_completion_session* s)
{ if(s) { xlsInputBatchDestroy(s->batch); xlsSyntaxRelease(s->entrySyntax); xlsSemanticCompletionDestroy(s->result); xrtFree(s); } }
void xlsCompletionSessionStats(const xls_completion_session* s,xls_completion_stats* stats)
{ if(stats) *stats=s ? s->stats : (xls_completion_stats){0}; }
xls_semantic_completion* xlsCompletionSessionQueryInContext(xls_completion_session* s,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL; xls_semantic_completion* result=NULL;
    xls_syntax* syntax=NULL;
    if(error && capacity) *error=0;
    if(!s) goto invalid;
    batch=xlsInputBatchCreateInContext(context,&s->limits,s->batch,documents,count,entryKey,cancelled,user,error,capacity);
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
        if(entry==query) {
            const xls_analysis_document* old=xlsInputBatchTarget(s->batch);
            const xls_syntax* previous=xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) && old && s->entrySyntax && s->queryKey && !strcmp(s->queryKey,old->key) &&
                !strcmp(old->key,entry->key) && old->identity==entry->identity && !strcmp(old->path,entry->path) ? s->entrySyntax : NULL;
            if(previous && s->offset==offset && xlsInputSameBody(old,entry)) {
                syntax=xlsSyntaxRetain(s->entrySyntax); ++s->stats.cursorSyntaxHits;
            } else {
                char* path=xrtPathAbs(entry->path);
                if(path) {
                    XL6_CompletionQuery projection={.path=path,.offset=offset}; xls_parse_work work={0};
                    syntax=xlsSyntaxBuildCursor(path,entry->text,previous,&projection,&work);
                    xl6CompletionFree(&projection); xrtFree(path);
                    s->stats.cursorFullParses+=work.full; s->stats.cursorFragmentParses+=work.fragments;
                    s->stats.cursorReusedDeclarations+=work.reused; s->stats.cursorParsedBytes+=work.bytes;
                }
            }
            if(cancelled && cancelled(user)) goto cancel;
            if(syntax) result=xlsSemanticCompletionCreateParsedInContext(context,entry->path,query->path,offset,inputs,count,syntax,cancelled,user,error,capacity);
        }
        if(!result) {
            if(cancelled && cancelled(user)) goto cancel;
            /* A cache/clone limit must not shrink the existing language
             * surface. This is the original native frontend, never CLI;
             * the explicit route counter distinguishes it from reuse. */
            ++s->stats.cursorFreshBuilds;
            result=xlsSemanticCompletionCreateInContext(context,entry->path,query->path,offset,inputs,count,cancelled,user,error,capacity);
            xlsSyntaxRelease(syntax); syntax=NULL;
        }
        if(!result) goto done;
    }
    if(cancelled && cancelled(user)) { xlsSemanticCompletionDestroy(result); result=NULL; goto cancel; }
    if(equal) ++s->stats.hits;
    s->queryKey=query->key; s->offset=offset;
    xlsInputBatchDestroy(s->batch); s->batch=batch; batch=NULL;
    xlsSemanticCompletionDestroy(s->result); s->result=xlsSemanticCompletionRetain(result);
    if(!equal) { xlsSyntaxRelease(s->entrySyntax); s->entrySyntax=syntax; syntax=NULL; }
    s->stats.documents=count; s->stats.textBytes=xlsInputBatchBytes(s->batch); goto done;
invalid: if(error && capacity) snprintf(error,capacity,"invalid completion batch, target or UTF-8 cursor"); goto done;
cancel: if(error && capacity) snprintf(error,capacity,"completion session cancelled");
done: xlsSyntaxRelease(syntax); xlsInputBatchDestroy(batch); return result;
}

xls_semantic_completion* xlsCompletionSessionQuery(xls_completion_session* session,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsCompletionSessionQueryInContext(session,NULL,documents,count,entryKey,queryKey,offset,cancelled,user,error,capacity); }
