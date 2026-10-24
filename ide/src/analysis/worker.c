#include "analysis/worker.h"
#include "analysis/frontend_context.h"
#include "analysis/session_state.h"
#include <xrt.h>
#include <string.h>
#include <stdio.h>

enum query_kind { QUERY_SYNTAX,QUERY_FOLDING,QUERY_CHECK,QUERY_COMPLETION,QUERY_SIGNATURE };
typedef struct semantic_session {
    enum query_kind kind;
    union {xls_check_session* check;xls_completion_session* completion;xls_signature_session* signature;} value;
} semantic_session;
struct xls_worker {
    xtaskpool* pool;xls_session* session;xls_session_limits limits;
    semantic_session cached[32];size_t cachedCount;xls_worker_cache_limits cacheLimits;xls_worker_cache_stats cacheStats;
    xls_input_batch* history[3];
    xls_check_stats checkTotals;xls_completion_stats completionTotals;xls_signature_stats signatureTotals;
};
struct xls_query { xfuture* future; xfuturewatch watch; int watchInitialized,watchLinked; void (*wake)(void*); void* user; };
typedef struct query_document { char *key,*path; xls_text* text; uint64_t identity,revision; } query_document;
typedef struct query_input { xls_worker* worker; xls_context* context; xmap documents; char *target,*queryKey; enum query_kind kind; size_t rangeLimit,offset; } query_input;
typedef struct query_result { xls_analysis_snapshot snapshot; xls_session_stats stats; uint64_t threadId; xls_folding* folding; xls_semantic_check* check; xls_check_stats checkStats; xls_semantic_completion* completion; xls_completion_stats completionStats; xls_signature_help* signature; xls_signature_stats signatureStats; xls_worker_cache_stats cacheStats; char error[256]; } query_result;
static xbytesview keyView(const char* key) { return (xbytesview){(const unsigned char*)key,strlen(key)}; }
static int validLabel(const char* label)
{ size_t size=label ? strlen(label) : 0; return size && size<=262144 && xrtUtf8Valid((xstrview){label,size},NULL); }
static void dropDocument(xbytesview key,void* value,void* user)
{ (void)key; (void)user; query_document* document=value; xrtFree(document->key); xrtFree(document->path); xlsTextRelease(document->text); }
static void freeInput(void* value,void* user)
{ (void)user; query_input* input=value; xlsContextRelease(input->context); xrtMapUnit(&input->documents); xrtFree(input->target); xrtFree(input->queryKey); xrtFree(input); }
static void freeResult(void* value,void* user)
{ (void)user; query_result* result=value; xlsSignatureRelease(result->signature); xlsSemanticCompletionDestroy(result->completion); xlsSemanticCheckDestroy(result->check); xlsFoldingDestroy(result->folding); xlsSyntaxRelease(result->snapshot.syntax); xrtFree(result); }
static int cancelled(void* user) { return xrtCancelRequested(user); }
static int live(const char* key,size_t size,void* user)
{ query_input* input=user; return xrtMapHas(&input->documents,(xbytesview){(const unsigned char*)key,size}); }
typedef struct update_context { xls_session* session; xcancel* cancel; int okay; } update_context;
static bool update(xbytesview key,void* value,void* user)
{
    (void)key; update_context* context=user; query_document* document=value;
    if(xrtCancelRequested(context->cancel)) { context->okay=0; return false; }
    context->okay=xlsSessionUpdate(context->session,document->key,document->path,
                                   document->identity,document->revision,document->text);
    return context->okay;
}
typedef struct check_inputs { xls_analysis_document documents[64]; size_t count; } check_inputs;
static bool captureCheck(xbytesview key,void* value,void* user)
{
    (void)key; check_inputs* inputs=user; query_document* d=value;
    if(inputs->count==64) return false;
    inputs->documents[inputs->count++]=(xls_analysis_document){d->key,d->path,d->text,d->identity,d->revision};
    return true;
}
#include "analysis/worker_cache.inc"
static xtaskoutcome execute(xcancel* cancel,void* value,xtaskvalue* output)
{
    query_input* input=value; xls_session* session=input->worker->session;
    if(xrtCancelRequested(cancel)) return XTASK_CANCELLED;
    if(input->kind==QUERY_CHECK || input->kind==QUERY_COMPLETION || input->kind==QUERY_SIGNATURE) return executeSemantic(cancel,input,output);
    if(!xlsSessionPrune(session,live,input)) return XTASK_FAILED;
    update_context context={session,cancel,1}; xrtMapVisit(&input->documents,update,&context);
    if(xrtCancelRequested(cancel)) return XTASK_CANCELLED;
    if(!context.okay) return XTASK_FAILED;
    query_result* result=xrtMalloc(sizeof(*result));
    if(!result) return XTASK_FAILED;
    memset(result,0,sizeof(*result));
    if(!xlsSessionSyntax(session,input->target,&result->snapshot)) { freeResult(result,NULL); return XTASK_FAILED; }
    if(input->kind==QUERY_FOLDING) {
        result->folding=xlsFoldingCreate(result->snapshot.syntax,input->rangeLimit,cancelled,cancel);
        if(!result->folding) { freeResult(result,NULL); return xrtCancelRequested(cancel) ? XTASK_CANCELLED : XTASK_FAILED; }
    }
    xlsSessionStats(session,&result->stats); result->threadId=xrtThreadCurrentId();
    if(xrtCancelRequested(cancel)) { freeResult(result,NULL); return XTASK_CANCELLED; }
    *output=(xtaskvalue){result,freeResult,NULL}; return XTASK_SUCCESS;
}
xls_worker* xlsWorkerCreateWithCache(const xls_session_limits* limits,size_t queueLimit,const xls_worker_cache_limits* cache)
{
    xls_session_limits chosen=limits ? *limits : (xls_session_limits){64,8u*1024u*1024u,32u*1024u*1024u};
    xls_worker_cache_limits selected=cache ? *cache : (xls_worker_cache_limits){8,64u*1024u*1024u};
    if(!chosen.documents || !chosen.documentBytes || chosen.documentBytes>chosen.totalBytes || chosen.totalBytes>SIZE_MAX/2) return NULL;
    if(!selected.sessions || selected.sessions>32 || !selected.inputStorageBytes || selected.inputStorageBytes>SIZE_MAX/2)return NULL;
    xls_worker* worker=xrtMalloc(sizeof(*worker));
    if(!worker) return NULL;
    memset(worker,0,sizeof(*worker)); worker->limits=chosen;worker->cacheLimits=selected;
    worker->cacheStats.sessionLimit=selected.sessions;worker->cacheStats.inputStorageLimit=selected.inputStorageBytes;
    /* During sequential synchronization old and new document sizes can briefly
     * coexist; the submitted final batch is still constrained by totalBytes. */
    xls_session_limits staging=chosen; staging.totalBytes*=2;
    worker->session=xlsSessionCreate(&staging);
    xtaskpoolconfig config={.Threads=1,.QueueLimit=queueLimit ? queueLimit : 64};
    if(worker->session) worker->pool=xrtTaskPoolCreate(&config);
    if(!worker->pool) { xlsWorkerDestroy(worker); return NULL; }
    return worker;
}
xls_worker* xlsWorkerCreate(const xls_session_limits* limits,size_t queueLimit)
{return xlsWorkerCreateWithCache(limits,queueLimit,NULL);}
void xlsWorkerDestroy(xls_worker* worker)
{
    if(!worker) return;
    if(worker->pool) { xrtTaskPoolCancel(worker->pool); xrtTaskPoolDestroy(worker->pool); }
    xlsSessionDestroy(worker->session);
    for(size_t i=0;i<worker->cachedCount;++i)destroySemantic(&worker->cached[i]);
    for(size_t i=0;i<3;++i)xlsInputBatchDestroy(worker->history[i]);
    xrtFree(worker);
}
int xlsWorkerLoad(xls_worker* worker,xls_worker_load* load)
{
    if(!load) return 0;
    memset(load,0,sizeof(*load)); xtaskpoolstats stats={0};
    if(!worker || !xrtTaskPoolGet(worker->pool,&stats)) return 0;
    *load=(xls_worker_load){stats.Queued,stats.Running,stats.Submitted,stats.Completed,stats.Cancelled,stats.Rejected}; return 1;
}
static xls_query* submit(xls_worker* worker,xls_context* context,const xls_analysis_document* documents,size_t count,const char* target,enum query_kind kind,size_t rangeLimit,const char* queryKey,size_t offset)
{
    if(!worker || !documents || !count || count>worker->limits.documents ||
       (target ? !validLabel(target) : !context || (kind!=QUERY_CHECK && kind!=QUERY_COMPLETION && kind!=QUERY_SIGNATURE)) ||
       ((kind==QUERY_COMPLETION || kind==QUERY_SIGNATURE) && !validLabel(queryKey))) return NULL;
    query_input* input=xrtMalloc(sizeof(*input));
    if(!input) return NULL;
    memset(input,0,sizeof(*input)); input->context=xlsContextRetain(context); input->worker=worker; input->target=target ? xrtStrDup(target) : NULL; input->kind=kind; input->rangeLimit=rangeLimit;
    input->offset=offset; if(queryKey) input->queryKey=xrtStrDup(queryKey);
    if((target && !input->target) || (queryKey && !input->queryKey) || !xrtMapInit(&input->documents,sizeof(query_document)) ||
        !xrtMapSetDrop(&input->documents,dropDocument,NULL)) { freeInput(input,NULL); return NULL; }
    size_t bytes=0;
    for(size_t i=0;i<count;++i) {
        const xls_analysis_document* source=&documents[i];
        if(!source->text || !validLabel(source->key) || !validLabel(source->path) ||
            xlsTextSize(source->text)>worker->limits.documentBytes || xlsTextSize(source->text)>worker->limits.totalBytes-bytes) goto failed;
        bytes+=xlsTextSize(source->text);
        xbytesview key=keyView(source->key);
        if(xrtMapHas(&input->documents,key)) goto failed;
        query_document document={xrtStrDup(source->key),xrtStrDup(source->path),xlsTextRetain(source->text),source->identity,source->revision};
        if(!document.key || !document.path || !xrtMapSet(&input->documents,key,&document)) { dropDocument(key,&document,NULL); goto failed; }
    }
    query_document* targetDocument=target ? xrtMapGet(&input->documents,keyView(target)) : NULL;
    if((target && !targetDocument) || !xlsFrontendContextValid(context,targetDocument ? targetDocument->path : xlsContextSpec(context)->entry,NULL,0)) goto failed;
    if(queryKey && !xrtMapHas(&input->documents,keyView(queryKey))) goto failed;
    xls_query* query=xrtMalloc(sizeof(*query));
    if(!query) goto failed;
    memset(query,0,sizeof(*query));
    xtaskargs args={.Destroy=freeInput}; query->future=xrtTaskSubmit(worker->pool,execute,input,&args);
    if(!query->future) { xrtFree(query); goto failed; }
    return query;
failed: freeInput(input,NULL); return NULL;
}
xls_query* xlsWorkerSyntax(xls_worker* worker,const xls_analysis_document* documents,size_t count,const char* target)
{ return submit(worker,NULL,documents,count,target,QUERY_SYNTAX,0,NULL,0); }
xls_query* xlsWorkerFolding(xls_worker* worker,const xls_analysis_document* documents,size_t count,const char* target,size_t rangeLimit)
{ return submit(worker,NULL,documents,count,target,QUERY_FOLDING,rangeLimit,NULL,0); }
xls_query* xlsWorkerCheck(xls_worker* worker,const xls_analysis_document* documents,size_t count,const char* target)
{ return count>64 ? NULL : submit(worker,NULL,documents,count,target,QUERY_CHECK,0,NULL,0); }
xls_query* xlsWorkerCompletion(xls_worker* worker,const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset)
{ return count>64 ? NULL : submit(worker,NULL,documents,count,entry,QUERY_COMPLETION,0,queryKey,offset); }
xls_query* xlsWorkerCheckInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target)
{ return count>64 ? NULL : submit(worker,context,documents,count,target,QUERY_CHECK,0,NULL,0); }
xls_query* xlsWorkerCompletionInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset)
{ return count>64 ? NULL : submit(worker,context,documents,count,entry,QUERY_COMPLETION,0,queryKey,offset); }
void xlsQueryCancel(xls_query* query) { if(query) xrtFutureCancel(query->future); }
static void wakeQuery(void* user) { xls_query* query=user; query->wake(query->user); }
int xlsQuerySetWake(xls_query* query,void (*wake)(void*),void* user)
{
    if(!query || !wake || query->watchInitialized) return 0;
    query->wake=wake; query->user=user;
    if(!xrtFutureWatchInit(&query->watch,wakeQuery,NULL,query)) return 0;
    query->watchInitialized=1;
    xfuturewatchresult state=xrtFutureWatchAdd(query->future,&query->watch);
    if(state==XFUTURE_WATCH_ERROR) return 0;
    query->watchLinked=state==XFUTURE_WATCH_PENDING;
    if(state==XFUTURE_WATCH_READY) wake(user);
    return 1;
}
void xlsQueryDestroy(xls_query* query)
{
    if(query) {
        if(query->watchLinked) xrtFutureWatchRemove(query->future,&query->watch);
        xrtFutureDestroy(query->future); xrtFree(query);
    }
}
enum xls_query_state xlsQueryResult(xls_query* query,xls_analysis_snapshot* result,xls_session_stats* stats,uint64_t* threadId)
{
    if(result) memset(result,0,sizeof(*result));
    if(stats) memset(stats,0,sizeof(*stats));
    if(threadId) *threadId=0;
    if(!query || !result) return XLS_QUERY_FAILED;
    xfuturestate state=xrtFutureState(query->future);
    if(state==XFUTURE_PENDING) return XLS_QUERY_PENDING;
    if(state==XFUTURE_CANCELLED) return XLS_QUERY_CANCELLED;
    if(state!=XFUTURE_RESOLVED) return XLS_QUERY_FAILED;
    const query_result* value=xrtFutureValue(query->future);
    if(!value || !value->snapshot.syntax) return XLS_QUERY_FAILED;
    *result=value->snapshot; result->syntax=xlsSyntaxRetain(result->syntax);
    if(stats) *stats=value->stats;
    if(threadId) *threadId=value->threadId;
    return XLS_QUERY_READY;
}
const xls_folding* xlsQueryFolding(const xls_query* query)
{
    if(!query || xrtFutureState(query->future)!=XFUTURE_RESOLVED) return NULL;
    const query_result* result=xrtFutureValue(query->future); return result ? result->folding : NULL;
}
enum xls_query_state xlsQueryCheckResult(xls_query* query,xls_semantic_check** result,
    xls_check_stats* stats,uint64_t* threadId,char* error,size_t capacity)
{
    if(result) *result=NULL;
    if(stats) memset(stats,0,sizeof(*stats));
    if(threadId) *threadId=0;
    if(error && capacity) *error=0;
    if(!query || !result) return XLS_QUERY_FAILED;
    xfuturestate state=xrtFutureState(query->future);
    if(state==XFUTURE_PENDING) return XLS_QUERY_PENDING;
    if(state==XFUTURE_CANCELLED) return XLS_QUERY_CANCELLED;
    const query_result* value=state==XFUTURE_RESOLVED ? xrtFutureValue(query->future) : NULL;
    if(!value || !value->check) {
        if(error && capacity) snprintf(error,capacity,"%s",value && *value->error ? value->error : "semantic query failed");
        return XLS_QUERY_FAILED;
    }
    *result=xlsSemanticCheckRetain(value->check);
    if(stats) *stats=value->checkStats;
    if(threadId) *threadId=value->threadId;
    return XLS_QUERY_READY;
}
enum xls_query_state xlsQueryCompletionResult(xls_query* query,xls_semantic_completion** result,
    xls_completion_stats* stats,uint64_t* threadId,char* error,size_t capacity)
{
    if(result) *result=NULL;
    if(stats) memset(stats,0,sizeof(*stats));
    if(threadId) *threadId=0;
    if(error && capacity) *error=0;
    if(!query || !result) return XLS_QUERY_FAILED;
    xfuturestate state=xrtFutureState(query->future);
    if(state==XFUTURE_PENDING) return XLS_QUERY_PENDING;
    if(state==XFUTURE_CANCELLED) return XLS_QUERY_CANCELLED;
    const query_result* value=state==XFUTURE_RESOLVED ? xrtFutureValue(query->future) : NULL;
    if(!value || !value->completion) {
        if(error && capacity) snprintf(error,capacity,"%s",value && *value->error ? value->error : "cursor query failed");
        return XLS_QUERY_FAILED;
    }
    *result=xlsSemanticCompletionRetain(value->completion);
    if(stats) *stats=value->completionStats;
    if(threadId) *threadId=value->threadId;
    return XLS_QUERY_READY;
}

xls_query* xlsWorkerSignatureInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset)
{return count>64 ? NULL : submit(worker,context,documents,count,entry,QUERY_SIGNATURE,0,queryKey,offset);}
enum xls_query_state xlsQuerySignatureResult(xls_query* query,xls_signature_help** result,
    xls_signature_stats* stats,uint64_t* threadId,char* error,size_t capacity)
{
    if(result)*result=NULL;
    if(stats)*stats=(xls_signature_stats){0};
    if(threadId)*threadId=0;
    if(error && capacity)*error=0;
    if(!query || !result)return XLS_QUERY_FAILED;
    xfuturestate state=xrtFutureState(query->future);
    if(state==XFUTURE_PENDING)return XLS_QUERY_PENDING;
    if(state==XFUTURE_CANCELLED)return XLS_QUERY_CANCELLED;
    const query_result* value=state==XFUTURE_RESOLVED ? xrtFutureValue(query->future) : NULL;
    if(!value || !value->signature){if(error && capacity)snprintf(error,capacity,"%s",value && *value->error ? value->error : "signature query failed");return XLS_QUERY_FAILED;}
    *result=xlsSignatureRetain(value->signature);
    if(stats)*stats=value->signatureStats;
    if(threadId)*threadId=value->threadId;
    return XLS_QUERY_READY;
}
int xlsQuerySessionCache(const xls_query* query,xls_worker_cache_stats* stats)
{
    if(stats)*stats=(xls_worker_cache_stats){0};
    if(!query || !stats || xrtFutureState(query->future)!=XFUTURE_RESOLVED)return 0;
    const query_result* result=xrtFutureValue(query->future);
    if(!result || (!result->check && !result->completion && !result->signature))return 0;
    *stats=result->cacheStats;return 1;
}
