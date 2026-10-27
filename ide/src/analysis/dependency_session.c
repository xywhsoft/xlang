#include "analysis/dependency_session.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/input_batch.h"
#include "analysis/input_topology.h"
#include "analysis/session_state.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

struct xls_dependency_session {
    xls_input_batch* batch;
    xls_session_limits limits;
    xls_dependency_capture* result;
    xls_syntax* entrySyntax;
    xls_entry_cache* entryCache;
    xls_dependency_session_stats stats;
    uint64_t usedInputs;int usageKnown;
};
static int stopped(int (*cancelled)(void*),void* user){return cancelled && cancelled(user);}
static void fail(char* error,size_t capacity,const char* message){if(error && capacity)snprintf(error,capacity,"%s",message);}
xls_dependency_session* xlsDependencySessionCreate(const xls_session_limits* limits)
{
    xls_session_limits chosen;if(!xlsInputLimits(limits,&chosen))return NULL;
    xls_dependency_session* session=xrtCalloc(1,sizeof(*session));if(session)session->limits=chosen;return session;
}
void xlsDependencySessionDestroy(xls_dependency_session* s)
{if(s){xlsInputBatchDestroy(s->batch);xlsDependencyCaptureRelease(s->result);xlsSyntaxRelease(s->entrySyntax);xlsEntryCacheDestroy(s->entryCache);xrtFree(s);}}
void xlsDependencySessionStats(const xls_dependency_session* s,xls_dependency_session_stats* stats)
{if(stats)*stats=s?s->stats:(xls_dependency_session_stats){0};}
xls_session_state xlsDependencySessionState(const xls_dependency_session* s)
{return(xls_session_state){s?s->batch:NULL,NULL};}
xls_semantic_checkpoint xlsDependencySessionCheckpoint(const xls_dependency_session* s)
{
    if(!s)return(xls_semantic_checkpoint){0};
    return(xls_semantic_checkpoint){.inputs=xlsInputBatchRetain(s->batch),.dependency=xlsDependencyCaptureRetain(s->result),
        .syntax=xlsSyntaxRetain(s->entrySyntax),.usedInputs=s->usedInputs,.usageKnown=s->usageKnown,
        .documents=s->stats.documents,.textBytes=s->stats.textBytes};
}
void xlsDependencySessionRestore(xls_dependency_session* s,xls_semantic_checkpoint* old)
{
    if(!s || !old)return;
    xlsInputBatchDestroy(s->batch);xlsDependencyCaptureRelease(s->result);xlsSyntaxRelease(s->entrySyntax);
    s->batch=old->inputs;s->result=old->dependency;s->entrySyntax=old->syntax;s->usedInputs=old->usedInputs;s->usageKnown=old->usageKnown;
    s->stats.documents=old->documents;s->stats.textBytes=old->textBytes;*old=(xls_semantic_checkpoint){0};
}
xls_dependency_capture* xlsDependencySessionCaptureWithHistory(xls_dependency_session* s,xls_context* context,const xls_input_batch* history,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_input_batch* batch=NULL;xls_dependency_capture* result=NULL;xls_syntax* syntax=NULL;
    if(error && capacity)*error=0;
    if(!s){fail(error,capacity,"invalid dependency session");return NULL;}
    batch=xlsInputBatchCreateClosedInContext(context,&s->limits,history,documents,count,target,cancelled,user,error,capacity);
    if(!batch)goto done;
    uint64_t remapped=0;int ordinary=s->result && xlsInputBatchEqual(s->batch,batch);
    int used=s->result && s->usageKnown && xlsInputBatchEqualUsed(s->batch,batch,s->usedInputs,NULL,&remapped);
    if(!used && s->result && s->usageKnown)
        used=xlsInputBatchEqualTopology(s->batch,batch,s->usedInputs,NULL,xlsDependencyCaptureTopology(s->result),&remapped);
    int equal=ordinary || used;
    if(equal && !xlsDependencyCaptureValidate(s->result)){equal=0;++s->stats.invalidations;}
    if(stopped(cancelled,user))goto cancel;
    if(equal)result=xlsDependencyCaptureRetain(s->result);
    else{
        ++s->stats.builds;
        const xls_analysis_document* entry=xlsInputBatchTarget(batch),*oldTarget=xlsInputBatchTarget(s->batch);
        const xls_analysis_document* old=entry && oldTarget && !strcmp(oldTarget->key,target)?xlsInputBatchFind(s->batch,target):NULL;
        const xls_syntax* previous=xlsFrontendContextsEqual(context,xlsInputBatchContext(s->batch)) && old && old->identity==entry->identity && !strcmp(old->path,entry->path)?s->entrySyntax:NULL;
        if(previous && xlsInputSameBody(old,entry)){syntax=xlsSyntaxRetain(s->entrySyntax);++s->stats.entrySyntaxHits;}
        else if(entry){
            char* path=xrtPathAbs(entry->path);if(!path)goto memory;
            xls_parse_work work={0};syntax=xlsSyntaxBuild(path,entry->text,previous,&work);xrtFree(path);
            s->stats.entryFullParses+=work.full;s->stats.entryFragmentParses+=work.fragments;
            s->stats.entryReusedDeclarations+=work.reused;s->stats.entryParsedBytes+=work.bytes;
            if(!syntax)goto memory;
        }
        if(stopped(cancelled,user))goto cancel;
        xls_semantic_input inputs[64];xlsInputBatchInputs(batch,inputs,64);
        if(!s->entryCache)s->entryCache=xlsEntryCacheCreate(32u*1024u*1024u);
        result=xlsDependencyCaptureCreateParsedInContext(context,s->entryCache,xlsInputBatchEntry(batch),inputs,count,syntax,cancelled,user,error,capacity);
        if(!result)goto done;
    }
    if(stopped(cancelled,user)){xlsDependencyCaptureRelease(result);result=NULL;goto cancel;}
    if(equal){++s->stats.hits;if(used && !ordinary)++s->stats.inputScopeHits;s->usageKnown=used;s->usedInputs=remapped;}
    else s->usageKnown=xlsDependencyCaptureInputUsage(result,&s->usedInputs);
    xlsInputBatchDestroy(s->batch);s->batch=batch;batch=NULL;
    xlsDependencyCaptureRelease(s->result);s->result=xlsDependencyCaptureRetain(result);
    s->stats.documents=count;s->stats.textBytes=xlsInputBatchBytes(s->batch);
    if(!equal){xlsSyntaxRelease(s->entrySyntax);s->entrySyntax=syntax;syntax=NULL;}
    goto done;
memory:fail(error,capacity,"cannot retain dependency input syntax");goto done;
cancel:fail(error,capacity,"dependency session cancelled");
done:xlsSyntaxRelease(syntax);xlsInputBatchDestroy(batch);return result;
}
xls_dependency_capture* xlsDependencySessionCaptureInContext(xls_dependency_session* s,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return xlsDependencySessionCaptureWithHistory(s,context,s?s->batch:NULL,documents,count,target,cancelled,user,error,capacity);}
