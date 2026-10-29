#include "analysis/semantic.h"
#include "analysis/frontend_overlay.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_input_view.h"
#include "analysis/frontend_input_usage.h"
#include "analysis/input_topology.h"
#include "analysis/frontend_module_graph.h"
#include "analysis/frontend_cancel.h"
#include "analysis/signature_internal.h"
#include "analysis/completion_unit.h"
#include "analysis/cursor_word_view.h"
#include "analysis/frontend_completion_pool.h"
#include "analysis/frontend_raw_body.h"
#include "src/parser/xl6_parser_api.h"
#include "src/compile/xl6_imports.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_completion.h"
#include <stdio.h>

typedef struct xls_completion_word_unit xls_completion_word_unit;
static void wordUnitRelease(xls_completion_word_unit*);
static void wordQueryFree(xls_semantic_completion*);
static int validateLocked(xls_semantic_completion*);
struct xls_semantic_completion {
    xatomic32 references,invalid;
    xls_context* context;
    xmutex lock;
    int lockInitialized,available;
    XL6_ParseResult parse;
    XL6_ImportDeps deps;
    XL6_SourceReport report;
    XL6_SourceOverlay overlay;
    xls_input_batch* inputOwner;
    XL6_CompletionQuery query;
    xls_completion_pool pool;
    int poolComplete;
    char *path,*queryError;
    xls_completion_entry_work entryWork;
    xls_entry_parse_work moduleWork;
    int cursorSyntaxApplied;
    xls_sem_memo* memo;
    xls_sem_work semanticWork;
    xls_sem_call_work callWork;
    xls_sem_tail* tail;
    uint64_t bodyRetries;
    xls_raw_native_work rawWork;
    xls_sem_storage_work storageWork;
    xls_ast_materialize_work materializationWork;
    xls_signature_data* signature;
    uint64_t usedInputs;int usageKnown;
    xls_input_topology* topology;
    xls_module_graph* modules;
    xls_completion_word_unit* wordUnit;
    const char* wordKey;
    xls_completion_unit_work wordWork;
    xls_semantic_completion* demanded;
};
static int absolute(const char* path)
{
    return path && *path && xrtPathIsAbs(path);
}
static int stopped(int (*cancelled)(void*),void* user)
{ return xlsFrontendCancelObserved() || (cancelled && cancelled(user)); }
static void fail(char* error,size_t capacity,const char* message)
{ if(error && capacity) snprintf(error,capacity,"%s",message); }
static int invalid(const xls_semantic_completion* r)
{ return xrtAtomic32Load(&r->invalid,XMEMORY_ACQUIRE)!=0; }
static int ready(const xls_semantic_completion* r)
{
    /* report.complete can change under validation's lock. Published query
     * facts and this initial availability remain immutable for all readers. */
    return r && !invalid(r) && r->available;
}
xls_semantic_completion* xlsSemanticCompletionRetain(xls_semantic_completion* r)
{ if(r) xrtAtomic32FetchAdd(&r->references,1,XMEMORY_RELAXED); return r; }
void xlsSemanticCompletionDestroy(xls_semantic_completion* r)
{
    if(!r || xrtAtomic32FetchSub(&r->references,1,XMEMORY_ACQ_REL)!=1) return;
    xlsSignatureDataDestroy(r->signature);
    if(!r->wordUnit)xlsInputTopologyDestroy(r->topology);
    xlsModuleGraphRelease(r->modules);
    wordQueryFree(r);
    xlsCompletionPoolFree(&r->pool);
    xlsFrontendSemTailDestroy(r->tail);
    xl6ParseResultFree(&r->parse);
    xl6ImportDepsFree(&r->deps);
    xlsFrontendOverlayClear(&r->overlay,r->inputOwner);xlsInputBatchDestroy(r->inputOwner);
    xl6SourceReportFree(&r->report);
    wordUnitRelease(r->wordUnit);
    xlsSemanticCompletionDestroy(r->demanded);
    xlsFrontendSemMemoDestroy(r->memo);
    xlsContextRelease(r->context); xrtFree(r->queryError); xrtFree(r->path); if(r->lockInitialized) xrtMutexUnit(&r->lock); xrtFree(r);
}
#include "analysis/semantic_word_unit.inc"
static xls_semantic_completion* createBody(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    xls_semantic_completion* previous,int memoEnabled,
    int (*cancelled)(void*),void* user,char* error,size_t capacity,xls_signature_scope* signature,int* bodyRetry,const xls_input_batch* batch)
{
    xls_semantic_completion* r=NULL;
    *bodyRetry=0;
    if(error && capacity) *error=0;
    if(!absolute(entry) || !absolute(queryPath) || !inputs || !count || count>XL6_SOURCE_OVERLAY_FILES) {
        fail(error,capacity,"semantic completion requires absolute paths and 1..64 overlays"); return NULL;
    }
    if(stopped(cancelled,user)) { fail(error,capacity,"semantic completion cancelled"); return NULL; }
    r=xrtCalloc(1,sizeof(*r)); if(!r) { fail(error,capacity,"cannot allocate semantic completion"); return NULL; }
    xrtAtomic32Init(&r->references,1); xrtAtomic32Init(&r->invalid,0); r->context=xlsContextRetain(context);
    if(signature){r->signature=signature->data;signature->adopted=1;}
    if(!xrtMutexInit(&r->lock)) goto invalid;
    r->lockInitialized=1;
    xl6ParseResultInit(&r->parse); xl6ImportDepsInit(&r->deps); xl6SourceReportInit(&r->report);
    r->path=xrtPathAbs(queryPath); r->query.path=r->path; r->query.offset=offset;
    r->inputOwner=xlsInputBatchRetain(batch);
    if(!r->path) goto invalid;
    /* Bind only after the result owns the existing XRT canonical query path.
     * Caller URI/context spelling is not a second source identity. Original
     * later source/topology/context admission still runs before semantic reuse. */
    xls_raw_scope* rawScope=xlsRawBodyScopeCurrent();
    if(rawScope && rawScope->ready && previous && !strcmp(previous->path,r->path) &&
       xlsFrontendContextsEqual(previous->context,context) && xlsSemanticCompletionValidate(previous))
        rawScope->baseline=previous->wordUnit ? previous->wordUnit->memo : previous->memo;
    if(batch) {
        int captured=xlsFrontendOverlayBorrow(&r->overlay,batch,cancelled,user);
        if(captured<0)goto cancel;
        if(!captured)goto invalid;
    } else for(size_t i=0;i<count;++i) {
        const xls_semantic_input* in=&inputs[i]; size_t bom=0;
        if(stopped(cancelled,user)) goto cancel;
        if(!absolute(in->path) || !in->text || in->size>XL6_SOURCE_OVERLAY_BYTES ||
           xl6SourceOverlayFind(&r->overlay,in->path)) goto invalid;
        if(in->size>=3 && !memcmp(in->text,"\xef\xbb\xbf",3)) bom=3;
        if(!xl6SourceOverlaySet(&r->overlay,in->path,in->text+bom,in->size-bom)) goto invalid;
    }
    xlsFrontendInputUsageBind(&r->overlay,r->overlay.count);
    const XL6_SourceOverlayItem* target=xl6SourceOverlayFind(&r->overlay,r->path);
    const XL6_SourceOverlayItem* entryInput=xl6SourceOverlayFind(&r->overlay,entry);
    if(!target || offset>target->size ||
       (offset<target->size && ((unsigned char)target->text[offset]&0xc0)==0x80)) goto invalid;
    if(signature){signature->query=&r->query;if(!signature->data->hasSite)return r;}
    r->overlay.report=&r->report; r->overlay.completionQuery=&r->query; r->deps.sourceOverlay=&r->overlay;
    int okay;
    if(syntax) {
        const xls_text* text=xlsSyntaxText(syntax); size_t skip=0;
        if(text && xlsTextSize(text)>=3 && !memcmp(xlsTextData(text),"\xef\xbb\xbf",3)) skip=3;
        /* Snapshot and source report describe the same query bytes. A parse
         * for another cursor/mode/path is never substituted for this query. */
        if(!text || !xlsSyntaxCursorMatches(syntax,target->path,offset) ||
           strcmp(target->path,xlsSyntaxPath(syntax)) || target->size!=xlsTextSize(text)-skip ||
           memcmp(target->text,xlsTextData(text)+skip,target->size)) goto invalid;
    }
    if(syntax && entryInput==target) {
        if(!xl6SourceReportText(&r->report,target->path,target->text,target->size,"overlay") ||
           !xlsRawBodyCopyCursorParse(syntax,&r->query,&r->parse)) goto invalid;
        r->cursorSyntaxApplied=1;
        if(r->query.error) {
            r->queryError=xrtStrDup(r->query.error); if(!r->queryError) goto invalid;
            r->query.error=r->queryError;
        }
        okay=xlsSyntaxAccepted(syntax);
    } else okay=xl6SourceOverlayParse(&r->overlay,entry,&r->parse);
    if(stopped(cancelled,user)) goto cancel;
    if(okay) okay=xl6ImportsExpandModule(r->parse.root,entry,&r->parse.diags,&r->deps);
    /* An imported cursor copy may borrow its error from the retained syntax.
     * The completion result must outlive that syntax and its owning session. */
    if(r->query.error && !r->queryError) {
        r->queryError=xrtStrDup(r->query.error);if(!r->queryError) goto invalid;
        r->query.error=r->queryError;
    }
    if(stopped(cancelled,user)) goto cancel;
    int retry=0;
    if(!xlsRawBodySealPreprocess(&r->report) || (rawScope && rawScope->deferred && (!okay || r->parse.diags.count)))retry=2;
    if(okay && !retry) okay=xl6StdlibExpandModule(r->parse.root,&r->parse.diags);
    if(stopped(cancelled,user)) goto cancel;
    /* A successful cursor projection intentionally stops semantic checking
     * early. Its available/reached flags, not full-check return, are decisive. */
    if(okay && !retry) {
        /* Match the compiler's module identity without borrowing its mutable
         * CompileUnit/IR pipeline. XRT already provides path decomposition. */
        char module[256]; char* stem=xrtPathStem(entry);
        if(!stem) goto invalid;
        snprintf(module,sizeof(module),"%s",*stem ? stem : "module"); xrtFree(stem);
        xls_completion_pool_scope pool;
        xlsCompletionPoolBegin(&pool,&r->query,memoEnabled && batch && syntax && !signature ? wordProducerBudget() : 0);
        xls_sem_call_control calls;
        xls_sem_storage_control storage;
        xlsFrontendSemStorageBegin(&storage,xlsFrontendSemStorageBudget());
        xlsFrontendSemCallsBegin(&calls,xlsFrontendSemCallsBudget());
        if(memoEnabled) {
            xls_sem_memo* memo=previous && previous->wordUnit ? previous->wordUnit->memo : previous ? previous->memo : NULL;
            const XL6_SourceReport* oldReport=previous && previous->wordUnit ? &previous->wordUnit->report : previous ? &previous->report : NULL;
            int topology=previous && previous->wordUnit ? xlsInputTopologyOverlayCompatible(previous->wordUnit->topology,&previous->wordUnit->overlay,&r->overlay) :
                previous && xlsInputTopologyOverlayCompatible(xlsSemanticCompletionTopology(previous),&previous->overlay,&r->overlay);
            int compatible=previous && memo && !strcmp(previous->path,r->path) &&
                xlsFrontendContextsEqual(previous->context,context) && xlsSemanticCompletionValidate(previous) &&
                topology;
            retry=xlsFrontendSemCheckCursorDeferred(r->parse.root,&r->parse.diags,module,&r->query,entry,&r->report,
                compatible ? oldReport : NULL,compatible ? memo : NULL,&r->memo,&r->semanticWork,&r->tail)<0;
        } else xl6SemCheckModuleWithQuery(r->parse.root,&r->parse.diags,module,0,&r->query);
        r->poolComplete=xlsCompletionPoolEnd(&pool,&r->pool);
        xlsFrontendSemCallsEnd(&calls,&r->callWork);
        xlsFrontendSemStorageEnd(&storage,&r->storageWork);
    }
    if(retry){
        *bodyRetry=rawScope && rawScope->deferred ? 2 : retry;
        if(signature){xlsSignatureDataResetCaptured(signature->data);r->signature=NULL;signature->adopted=0;}
        xlsSemanticCompletionDestroy(r);return NULL;
    }
    if(stopped(cancelled,user)) goto cancel;
    r->available=r->query.available && !r->query.error &&
        r->query.parsed==1 && r->query.reached==1 && r->report.complete;
    xlsSemanticCompletionValidate(r);
    return r;
invalid:
    fail(error,capacity,"invalid semantic overlay, path, cursor or capacity");
    xlsSemanticCompletionDestroy(r); return NULL;
cancel:
    fail(error,capacity,"semantic completion cancelled");
    xlsSemanticCompletionDestroy(r); return NULL;
}
static xls_semantic_completion* create(xls_context* context,xls_entry_cache* cache,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    xls_semantic_completion* previous,int memoEnabled,
    int (*cancelled)(void*),void* user,char* error,size_t capacity,xls_signature_data* signatureData,const xls_input_batch* batch)
{
    if(!xlsFrontendContextValid(context,entry,error,capacity)){xlsSignatureDataDestroy(signatureData);return NULL;}
    xls_signature_scope signature;
    if(signatureData)xlsSignatureScopeBegin(&signature,signatureData,cancelled,user);
    xls_frontend_context_scope scope; xlsFrontendContextBegin(&scope,context);
    xls_raw_scope raw;xlsRawBodyScopeBegin(&raw,NULL,memoEnabled && batch && (syntax || (signatureData && signatureData->hasSite)) ? 32u*1024u*1024u : 0);
    xls_ast_materialize_scope materialization;
    xlsAstMaterializeBegin(&materialization,memoEnabled ? xlsAstMaterializeBudget() : 0);
    xls_entry_cache_scope entryScope;xlsFrontendEntryBegin(&entryScope,cache,context,entry);
    entryScope.cursorSyntax=syntax;
    xls_input_usage_scope usage;xlsFrontendInputUsageBegin(&usage);
    xls_input_topology_scope topology;xlsInputTopologyBegin(&topology,memoEnabled ? 8u*1024u*1024u : 0);
    xls_module_graph_scope modules;xlsModuleGraphBegin(&modules,memoEnabled ? xlsModuleScopeCurrent() : NULL,context,8u*1024u*1024u);
    xls_frontend_cancel_scope cancellation;xlsFrontendCancelBegin(&cancellation,cancelled,user);
    int bodyRetry=0;
    xls_semantic_completion* result=createBody(context,entry,queryPath,offset,inputs,count,syntax,previous,memoEnabled,cancelled,user,error,capacity,signatureData ? &signature : NULL,&bodyRetry,batch);
    uint64_t used=0;int known=xlsFrontendInputUsageEnd(&usage,&used);
    xls_input_topology* captured=xlsInputTopologyEnd(&topology);
    if(result && known)result->topology=captured;else xlsInputTopologyDestroy(captured);
    xls_module_graph* graph=xlsModuleGraphEnd(&modules,result ? &result->report : NULL);
    if(result)result->modules=graph;else xlsModuleGraphRelease(graph);
    if(signatureData){
        if(!signature.adopted && !bodyRetry)xlsSignatureDataDestroy(signatureData);
        xlsSignatureScopeEnd(&signature);
        if(signature.cancelObserved){
            if(bodyRetry)xlsSignatureDataDestroy(signatureData);
            bodyRetry=0;xlsSemanticCompletionDestroy(result);result=NULL;fail(error,capacity,"signature query cancelled");
        }
    }
    if(entryScope.cursorFailed) {
        if(bodyRetry && signatureData)xlsSignatureDataDestroy(signatureData);
        bodyRetry=0;xlsSemanticCompletionDestroy(result);result=NULL;fail(error,capacity,"cannot own exact imported cursor syntax");
    }
    if(bodyRetry && stopped(cancelled,user)){
        xlsSignatureDataDestroy(signatureData);bodyRetry=0;fail(error,capacity,"semantic completion cancelled");
    }
    if(result && stopped(cancelled,user)) {xlsSemanticCompletionDestroy(result);result=NULL;fail(error,capacity,"semantic completion cancelled");}
    xlsFrontendCancelEnd(&cancellation,NULL);
    if(result)result->rawWork=(xls_raw_native_work){.registered=raw.registered,.deferred=raw.deferred,.restored=raw.consumed,.capturedUnitBytes=raw.unitBytes,.memoBytes=xlsFrontendSemRawMemoBytes(result->memo)};
    xlsRawBodyScopeEnd(&raw);
    int scoped=xlsFrontendContextEnd(&scope);
    xls_completion_entry_work work=xlsFrontendEntryEnd(&entryScope,result && scoped);
    xls_ast_materialize_work materializationWork;xlsAstMaterializeEnd(&materialization,&materializationWork);
    if(!scoped) {
        if(bodyRetry)xlsSignatureDataDestroy(signatureData);
        xlsSemanticCompletionDestroy(result); fail(error,capacity,(scope.failed&2) ? "cannot retain bounded completion namespace" : "cannot resolve native context path or package environment"); return NULL;
    }
    if(bodyRetry){
        result=create(context,bodyRetry==2 ? cache : NULL,entry,queryPath,offset,inputs,count,syntax,NULL,bodyRetry==2 ? memoEnabled : 0,cancelled,user,error,capacity,signatureData,batch);
        if(result){++result->bodyRetries;if(bodyRetry==2){++result->rawWork.retries;result->rawWork.discardedFunctions+=raw.deferred;result->rawWork.discardedCopyNodes+=materializationWork.nodes;}}
        return result;
    }
    if(result) {result->entryWork=work;result->moduleWork=entryScope.moduleWork;result->cursorSyntaxApplied|=entryScope.cursorApplied;result->materializationWork=materializationWork;result->usedInputs=used;result->usageKnown=known && result->report.complete && result->report.count && result->overlay.count==count;}
    return result;
}
int xlsSemanticCompletionInputUsage(const xls_semantic_completion* result,uint64_t* used)
{
    if(used)*used=0;
    if(!used || !result || invalid(result) || !result->usageKnown)return 0;
    *used=result->usedInputs;return 1;
}
const xls_input_topology* xlsSemanticCompletionTopology(const xls_semantic_completion* result)
{return result && !invalid(result) && result->usageKnown ? result->topology : NULL;}
const xls_module_graph* xlsSemanticCompletionModules(const xls_semantic_completion* result)
{return result && !invalid(result) ? result->modules : NULL;}
xls_completion_entry_work xlsSemanticCompletionEntryWork(const xls_semantic_completion* result)
{ return result ? result->entryWork : (xls_completion_entry_work){0}; }
xls_entry_parse_work xlsSemanticCompletionModuleWork(const xls_semantic_completion* result)
{ return result ? result->moduleWork : (xls_entry_parse_work){0}; }
int xlsSemanticCompletionCursorSyntaxApplied(const xls_semantic_completion* result)
{return result && result->cursorSyntaxApplied;}
xls_sem_work xlsSemanticCompletionSemWork(const xls_semantic_completion* result)
{return result ? result->semanticWork : (xls_sem_work){0};}
xls_sem_call_work xlsSemanticCompletionCallWork(const xls_semantic_completion* result)
{return result ? result->callWork : (xls_sem_call_work){0};}
xls_sem_storage_work xlsSemanticCompletionStorageWork(const xls_semantic_completion* result)
{return result ? result->storageWork : (xls_sem_storage_work){0};}
xls_ast_materialize_work xlsSemanticCompletionMaterializationWork(const xls_semantic_completion* result)
{return result ? result->materializationWork : (xls_ast_materialize_work){0};}
const XL6_AstNode* xlsSemanticCompletionAst(const xls_semantic_completion* result)
{
    xls_semantic_completion* r=(xls_semantic_completion*)result;
    if(!r || !xrtMutexLock(&r->lock))return NULL;
    if(r->wordUnit || r->rawWork.deferred){
        const XL6_AstNode* root=wordDemandAst(r);if(r->rawWork.deferred){r->rawWork.fullMaterialized=!!root;if(!root)++r->rawWork.astFailures;}xrtMutexUnlock(&r->lock);return root;
    }
    int materialized=xlsFrontendSemTailMaterialize(r->tail);
    const XL6_AstNode* root=materialized ? r->parse.root : NULL;xrtMutexUnlock(&r->lock);return root;
}
xls_sem_tail_work xlsSemanticCompletionTailWork(const xls_semantic_completion* result)
{
    xls_semantic_completion* r=(xls_semantic_completion*)result;
    if(!r || !xrtMutexLock(&r->lock))return (xls_sem_tail_work){0};
    xls_sem_tail_work work=xlsFrontendSemTailWork(r->tail);work.bodyRetries=r->bodyRetries;xrtMutexUnlock(&r->lock);return work;
}
xls_semantic_completion* xlsSemanticCompletionCreateIncrementalInContext(xls_context* context,xls_entry_cache* cache,
    const char* entry,const char* queryPath,size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    xls_semantic_completion* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return create(context,cache,entry,queryPath,offset,inputs,count,syntax,previous,1,cancelled,user,error,capacity,NULL,NULL);}
xls_semantic_completion* xlsSemanticCompletionCreateBatch(xls_entry_cache* cache,const xls_input_batch* batch,
 const char* queryKey,size_t offset,const xls_syntax* syntax,xls_semantic_completion* previous,
 int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
 const xls_analysis_document* query=xlsInputBatchFind(batch,queryKey);
 if(!query){fail(error,capacity,"missing immutable completion query");return NULL;}
 xls_semantic_input inputs[64];size_t count=xlsInputBatchInputs(batch,inputs,64);
 xls_semantic_completion* result=create((xls_context*)xlsInputBatchContext(batch),cache,xlsInputBatchEntry(batch),query->path,
     offset,inputs,count,syntax,previous,1,cancelled,user,error,capacity,NULL,batch);
 if(result)result->wordKey=query->key;
 return result;
}
int xlsSemanticCompletionInputs(const xls_semantic_completion* r,xls_inputs* out)
{ return xlsInputReportInputs(r && !invalid(r) ? &r->report : NULL,out); }
int xlsSemanticCompletionSource(const xls_semantic_completion* r,size_t index,xls_input_source* out)
{ return xlsInputReportSource(r && !invalid(r) ? &r->report : NULL,index,out); }
int xlsSemanticCompletionProbe(const xls_semantic_completion* r,size_t index,xls_input_probe* out)
{ return xlsInputReportProbe(r && !invalid(r) ? &r->report : NULL,index,out); }
xls_semantic_completion* xlsSemanticCompletionCreateCachedInContext(xls_context* context,xls_entry_cache* cache,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return create(context,cache,entry,queryPath,offset,inputs,count,NULL,NULL,0,cancelled,user,error,capacity,NULL,NULL); }
xls_semantic_completion* xlsSemanticCompletionCreateCachedParsedInContext(xls_context* context,xls_entry_cache* cache,
    const char* entry,const char* queryPath,size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!syntax) {fail(error,capacity,"cached parsed completion requires cursor syntax");return NULL;}
    return create(context,cache,entry,queryPath,offset,inputs,count,syntax,NULL,0,cancelled,user,error,capacity,NULL,NULL);
}
xls_semantic_completion* xlsSemanticCompletionCreateInContext(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return create(context,NULL,entry,queryPath,offset,inputs,count,NULL,NULL,0,cancelled,user,error,capacity,NULL,NULL); }
xls_semantic_completion* xlsSemanticCompletionCreateParsedInContext(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!syntax) { fail(error,capacity,"parsed completion requires cursor syntax"); return NULL; }
    return create(context,NULL,entry,queryPath,offset,inputs,count,syntax,NULL,0,cancelled,user,error,capacity,NULL,NULL);
}
const xls_context* xlsSemanticCompletionContext(const xls_semantic_completion* result)
{ return result ? result->context : NULL; }
xls_semantic_completion* xlsSemanticCompletionCreate(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return create(NULL,NULL,entry,queryPath,offset,inputs,count,NULL,NULL,0,cancelled,user,error,capacity,NULL,NULL); }
xls_semantic_completion* xlsSemanticCompletionCreateParsed(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!syntax) { fail(error,capacity,"parsed completion requires cursor syntax"); return NULL; }
    return create(NULL,NULL,entry,queryPath,offset,inputs,count,syntax,NULL,0,cancelled,user,error,capacity,NULL,NULL);
}
xls_completion_info xlsSemanticCompletionInfo(const xls_semantic_completion* r)
{
    if(!r) return (xls_completion_info){0};
    int stale=invalid(r),available=!stale && r->available;
    return (xls_completion_info){r->path,r->query.prefix,stale ? "semantic inputs changed" : r->query.error,
        r->query.offset,r->query.replacement.startOffset,r->query.replacement.endOffset,
        available ? r->query.count : 0,available};
}
int xlsSemanticCompletionItem(const xls_semantic_completion* r,size_t index,xls_completion_item* out)
{
    if(!out || !ready(r) || index>=r->query.count) return 0;
    const XL6_CompletionItem* item=&r->query.items[index]; const XL6_AstNode* node=item->selection;
    *out=(xls_completion_item){item->name,item->canonical,item->kind,xl6AstSourcePath(node),
        item->symbolId,node ? node->tokenRange.startOffset : 0,node ? node->tokenRange.endOffset : 0,
        node && node->tokenRange.known};
    return 1;
}
static int validateLocked(xls_semantic_completion* r)
{
    if(invalid(r)) return 0;
    xls_frontend_context_scope scope; xlsFrontendContextBegin(&scope,r->context);
    int okay=xl6SourceReportValidate(&r->report); int scoped=xlsFrontendContextEnd(&scope);
    if(!okay || !scoped) { xrtAtomic32Store(&r->invalid,1,XMEMORY_RELEASE); return 0; }
    return 1;
}
int xlsSemanticCompletionValidate(xls_semantic_completion* r)
{
    if(!r || !xrtMutexLock(&r->lock)) return 0;
    int okay=validateLocked(r); xrtMutexUnlock(&r->lock); return okay;
}
static char* jsonLocked(xls_semantic_completion* r,size_t* size)
{
    if(size) *size=0;
    if(r->signature)return NULL;
    if(!validateLocked(r)) return NULL;
    char* completion=xl6CompletionJson(&r->query,&r->report); if(!completion) return NULL;
    XL6_Output out; char* text=NULL;
    if(xl6OutputOpenMemory(&out)) {
        xl6DiagPrintJsonWithCompletion(&out,&r->report,ready(r),completion);
        if(!xl6OutputHasFailed(&out)) text=xl6OutputTakeMemoryText(&out,size);
        xl6OutputDiscard(&out);
    }
    xrtFree(completion);
    if(text && size && *size>12u*1024u*1024u) { xrtFree(text); text=NULL; *size=0; }
    if(!validateLocked(r)) { xrtFree(text); text=NULL; if(size) *size=0; }
    return text;
}
char* xlsSemanticSignatureInputJson(xls_semantic_completion* r,size_t* size)
{
    if(size)*size=0;
    if(!r || !r->signature || !r->signature->hasSite || !xrtMutexLock(&r->lock))return NULL;
    char* text=NULL;XL6_Output out;
    if(validateLocked(r) && xl6OutputOpenMemory(&out)){
        xl6DiagPrintJsonWithSources(&out,NULL,NULL,"signature",1,&r->report);
        if(!xl6OutputHasFailed(&out))text=xl6OutputTakeMemoryText(&out,size);
        xl6OutputDiscard(&out);
    }
    if((size && *size>12u*1024u*1024u) || !validateLocked(r)){xrtFree(text);text=NULL;if(size)*size=0;}
    xrtMutexUnlock(&r->lock);return text;
}
char* xlsSemanticCompletionJson(xls_semantic_completion* r,size_t* size)
{
    if(size) *size=0;
    if(!r || !xrtMutexLock(&r->lock)) return NULL;
    char* text=jsonLocked(r,size); xrtMutexUnlock(&r->lock); return text;
}
void xlsSemanticStringFree(char* text) { xrtFree(text); }

xls_signature_data* xlsSemanticSignatureData(const xls_semantic_completion* result)
{return result ? result->signature : NULL;}
xls_semantic_completion* xlsSemanticSignatureCreateInContext(xls_context* context,const char* entry,
    const char* path,size_t offset,const xls_semantic_input* inputs,size_t count,xls_signature_data* data,
    xls_entry_cache* cache,xls_semantic_completion* previous,int memoEnabled,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{return create(context,cache,entry,path,offset,inputs,count,NULL,previous && previous->signature ? previous : NULL,memoEnabled,cancelled,user,error,capacity,data,NULL);}

xls_semantic_completion* xlsSemanticSignatureCreateBatch(xls_entry_cache* cache,const xls_input_batch* batch,
 const char* queryKey,size_t offset,xls_signature_data* data,xls_semantic_completion* previous,
 int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
 const xls_analysis_document* query=xlsInputBatchFind(batch,queryKey);
 if(!query){xlsSignatureDataDestroy(data);fail(error,capacity,"missing immutable signature query");return NULL;}
 xls_semantic_input inputs[64];size_t count=xlsInputBatchInputs(batch,inputs,64);
 xls_semantic_completion* result=create((xls_context*)xlsInputBatchContext(batch),cache,xlsInputBatchEntry(batch),query->path,
     offset,inputs,count,NULL,previous && previous->signature ? previous : NULL,1,cancelled,user,error,capacity,data,batch);
 if(result)result->wordKey=query->key;
 return result;
}

xls_raw_native_work xlsSemanticCompletionRawBodyWork(const xls_semantic_completion* r)
{
    xls_semantic_completion* owner=(xls_semantic_completion*)r;
    if(!owner || !xrtMutexLock(&owner->lock))return (xls_raw_native_work){0};
    xls_raw_native_work work=owner->rawWork;xrtMutexUnlock(&owner->lock);return work;
}
