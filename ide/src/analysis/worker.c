#include "analysis/worker.h"
#include <xrt.h>
#include <string.h>

struct xls_worker { xtaskpool* pool; xls_session* session; xls_session_limits limits; };
struct xls_query { xfuture* future; };
typedef struct query_document { char *key,*path; xls_text* text; uint64_t identity,revision; } query_document;
typedef struct query_input { xls_worker* worker; xmap documents; char* target; } query_input;
typedef struct query_result { xls_analysis_snapshot snapshot; xls_session_stats stats; uint64_t threadId; } query_result;
static xbytesview keyView(const char* key) { return (xbytesview){(const unsigned char*)key,strlen(key)}; }
static int validLabel(const char* label)
{ size_t size=label ? strlen(label) : 0; return size && size<=262144 && xrtUtf8Valid((xstrview){label,size},NULL); }
static void dropDocument(xbytesview key,void* value,void* user)
{ (void)key; (void)user; query_document* document=value; xrtFree(document->key); xrtFree(document->path); xlsTextRelease(document->text); }
static void freeInput(void* value,void* user)
{ (void)user; query_input* input=value; xrtMapUnit(&input->documents); xrtFree(input->target); xrtFree(input); }
static void freeResult(void* value,void* user)
{ (void)user; query_result* result=value; xlsSyntaxRelease(result->snapshot.syntax); xrtFree(result); }
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
static xtaskoutcome execute(xcancel* cancel,void* value,xtaskvalue* output)
{
    query_input* input=value; xls_session* session=input->worker->session;
    if(xrtCancelRequested(cancel)) return XTASK_CANCELLED;
    if(!xlsSessionPrune(session,live,input)) return XTASK_FAILED;
    update_context context={session,cancel,1}; xrtMapVisit(&input->documents,update,&context);
    if(xrtCancelRequested(cancel)) return XTASK_CANCELLED;
    if(!context.okay) return XTASK_FAILED;
    query_result* result=xrtMalloc(sizeof(*result));
    if(!result) return XTASK_FAILED;
    memset(result,0,sizeof(*result));
    if(!xlsSessionSyntax(session,input->target,&result->snapshot)) { freeResult(result,NULL); return XTASK_FAILED; }
    xlsSessionStats(session,&result->stats); result->threadId=xrtThreadCurrentId();
    if(xrtCancelRequested(cancel)) { freeResult(result,NULL); return XTASK_CANCELLED; }
    *output=(xtaskvalue){result,freeResult,NULL}; return XTASK_SUCCESS;
}
xls_worker* xlsWorkerCreate(const xls_session_limits* limits,size_t queueLimit)
{
    xls_session_limits chosen=limits ? *limits : (xls_session_limits){64,8u*1024u*1024u,32u*1024u*1024u};
    if(!chosen.documents || !chosen.documentBytes || chosen.documentBytes>chosen.totalBytes || chosen.totalBytes>SIZE_MAX/2) return NULL;
    xls_worker* worker=xrtMalloc(sizeof(*worker));
    if(!worker) return NULL;
    memset(worker,0,sizeof(*worker)); worker->limits=chosen;
    /* During sequential synchronization old and new document sizes can briefly
     * coexist; the submitted final batch is still constrained by totalBytes. */
    xls_session_limits staging=chosen; staging.totalBytes*=2;
    worker->session=xlsSessionCreate(&staging);
    xtaskpoolconfig config={.Threads=1,.QueueLimit=queueLimit ? queueLimit : 64};
    if(worker->session) worker->pool=xrtTaskPoolCreate(&config);
    if(!worker->pool) { xlsWorkerDestroy(worker); return NULL; }
    return worker;
}
void xlsWorkerDestroy(xls_worker* worker)
{
    if(!worker) return;
    if(worker->pool) { xrtTaskPoolCancel(worker->pool); xrtTaskPoolDestroy(worker->pool); }
    xlsSessionDestroy(worker->session); xrtFree(worker);
}
int xlsWorkerLoad(xls_worker* worker,xls_worker_load* load)
{
    if(!load) return 0;
    memset(load,0,sizeof(*load)); xtaskpoolstats stats={0};
    if(!worker || !xrtTaskPoolGet(worker->pool,&stats)) return 0;
    *load=(xls_worker_load){stats.Queued,stats.Running,stats.Submitted,stats.Completed,stats.Cancelled,stats.Rejected}; return 1;
}
xls_query* xlsWorkerSyntax(xls_worker* worker,const xls_analysis_document* documents,size_t count,const char* target)
{
    if(!worker || !documents || !count || count>worker->limits.documents || !validLabel(target)) return NULL;
    query_input* input=xrtMalloc(sizeof(*input));
    if(!input) return NULL;
    memset(input,0,sizeof(*input)); input->worker=worker; input->target=xrtStrDup(target);
    if(!input->target || !xrtMapInit(&input->documents,sizeof(query_document)) ||
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
    xbytesview wanted=keyView(target);
    if(!xrtMapHas(&input->documents,wanted)) goto failed;
    xls_query* query=xrtMalloc(sizeof(*query));
    if(!query) goto failed;
    xtaskargs args={.Destroy=freeInput}; query->future=xrtTaskSubmit(worker->pool,execute,input,&args);
    if(!query->future) { xrtFree(query); goto failed; }
    return query;
failed: freeInput(input,NULL); return NULL;
}
void xlsQueryCancel(xls_query* query) { if(query) xrtFutureCancel(query->future); }
void xlsQueryDestroy(xls_query* query) { if(query) { xrtFutureDestroy(query->future); xrtFree(query); } }
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
    if(!value) return XLS_QUERY_FAILED;
    *result=value->snapshot; result->syntax=xlsSyntaxRetain(result->syntax);
    if(stats) *stats=value->stats;
    if(threadId) *threadId=value->threadId;
    return XLS_QUERY_READY;
}
