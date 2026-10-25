#include "analysis/check.h"
#include "analysis/check_ir.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_input_view.h"
#include "analysis/frontend_input_usage.h"
#include "analysis/input_topology.h"
#include "analysis/frontend_module_graph.h"
#include "analysis/frontend_index.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_sem.h"
#include "analysis/frontend_cancel.h"
#include "analysis/frontend_ir_memo.h"
#include "analysis/index_entities.h"
#include "src/parser/xl6_parser_api.h"
#include "src/compile/xl6_imports.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_sem_index.h"
#include "src/ir/xl6_ir.h"
#include <stdio.h>

struct xls_semantic_check {
    xatomic32 references,invalid;
    xls_context* context;
    xmutex lock;
    int lockInitialized;
    XL6_ParseResult parse;
    XL6_ImportDeps deps;
    XL6_SourceReport report;
    XL6_SourceOverlay overlay;
    XL6_IrModule ir;
    xls_ir_result* cachedIr;
    char *path,*index;
    xls_semantic_index* typedIndex;
    xls_sem_memo* memo;
    xls_sem_work semanticWork;
    xls_entry_parse_work entryWork,moduleWork;
    xls_parse_work moduleSyntaxWork;
    xls_ast_materialize_work materializationWork;
    const char* phase;
    int accepted,captureIndex;
    uint64_t usedInputs;int usageKnown;
    xls_input_topology* topology;
    xls_module_graph* modules;
};
static int stopped(int (*cancelled)(void*),void* user)
{ return xlsFrontendCancelObserved() || (cancelled && cancelled(user)); }
static void fail(char* error,size_t capacity,const char* message)
{ if(error && capacity) snprintf(error,capacity,"%s",message); }
static int invalid(const xls_semantic_check* r)
{ return xrtAtomic32Load(&r->invalid,XMEMORY_ACQUIRE)!=0; }
xls_semantic_check* xlsSemanticCheckRetain(xls_semantic_check* r)
{ if(r) xrtAtomic32FetchAdd(&r->references,1,XMEMORY_RELAXED); return r; }
void xlsSemanticCheckDestroy(xls_semantic_check* r)
{
    if(!r || xrtAtomic32FetchSub(&r->references,1,XMEMORY_ACQ_REL)!=1) return;
    xrtFree(r->index); xlsFrontendIndexDestroy(r->typedIndex); xlsFrontendSemMemoDestroy(r->memo);
    xlsInputTopologyDestroy(r->topology);
    xlsModuleGraphRelease(r->modules);
    xlsFrontendIrResultDestroy(r->cachedIr);xl6IrModuleFree(&r->ir); xl6ParseResultFree(&r->parse);
    xl6ImportDepsFree(&r->deps); xl6SourceOverlayClear(&r->overlay); xl6SourceReportFree(&r->report);
    xlsContextRelease(r->context); xrtFree(r->path); if(r->lockInitialized) xrtMutexUnit(&r->lock); xrtFree(r);
}
static xls_semantic_check* createBody(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int memoEnabled,xls_module_graph_scope* moduleScope,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_semantic_check* r=NULL;const xls_ir_result* irBaseline=NULL;
    if(error && capacity) *error=0;
    if(xlsSyntaxIsCursor(syntax)) { fail(error,capacity,"ordinary check cannot consume cursor syntax"); return NULL; }
    if(!entry || !xrtPathIsAbs(entry) || count>XL6_SOURCE_OVERLAY_FILES ||
       (count && !inputs) || (captureIndex!=0 && captureIndex!=1)) {
        fail(error,capacity,"check requires an absolute entry and 0..64 overlays"); return NULL;
    }
    if(stopped(cancelled,user)) { fail(error,capacity,"semantic check cancelled"); return NULL; }
    r=xrtCalloc(1,sizeof(*r)); if(!r) { fail(error,capacity,"cannot allocate semantic check"); return NULL; }
    xrtAtomic32Init(&r->references,1); xrtAtomic32Init(&r->invalid,0); r->context=xlsContextRetain(context);
    if(!xrtMutexInit(&r->lock)) goto invalid;
    r->lockInitialized=1;
    xl6ParseResultInit(&r->parse); xl6ImportDepsInit(&r->deps); xl6SourceReportInit(&r->report); xl6IrModuleInit(&r->ir);
    r->path=xrtPathAbs(entry); r->captureIndex=captureIndex;
    if(!r->path) goto invalid;
    for(size_t i=0;i<count;++i) {
        const xls_semantic_input* in=&inputs[i]; size_t bom=0;
        if(stopped(cancelled,user)) goto cancel;
        if(!in->path || !xrtPathIsAbs(in->path) || !in->text || in->size>XL6_SOURCE_OVERLAY_BYTES ||
           xl6SourceOverlayFind(&r->overlay,in->path)) goto invalid;
        if(in->size>=3 && !memcmp(in->text,"\xef\xbb\xbf",3)) bom=3;
        if(!xl6SourceOverlaySet(&r->overlay,in->path,in->text+bom,in->size-bom)) goto invalid;
    }
    xlsFrontendInputUsageBind(&r->overlay,r->overlay.count);
    r->overlay.report=&r->report; r->overlay.semanticIndex=captureIndex; r->deps.sourceOverlay=&r->overlay;
    r->phase="parse"; int okay;
    if(syntax) {
        /* Keep exactly the overlay/report contract used by the shared parser.
         * Reject an unrelated parse rather than silently accepting stale facts. */
        const XL6_SourceOverlayItem* item=xl6SourceOverlayFind(&r->overlay,r->path);
        const xls_text* text=xlsSyntaxText(syntax); size_t skip=0;
        if(text && xlsTextSize(text)>=3 && !memcmp(xlsTextData(text),"\xef\xbb\xbf",3)) skip=3;
        if(!item || !text || strcmp(item->path,xlsSyntaxPath(syntax)) ||
           item->size!=xlsTextSize(text)-skip || memcmp(item->text,xlsTextData(text)+skip,item->size)) goto invalid;
        if(!xl6SourceReportText(&r->report,item->path,item->text,item->size,"overlay") ||
           !xlsSyntaxCopyParse(syntax,&r->parse)) {
            fail(error,capacity,"cannot own parsed entry or source report"); xlsSemanticCheckDestroy(r); return NULL;
        }
        okay=xlsSyntaxAccepted(syntax);
    } else okay=xl6SourceOverlayParse(&r->overlay,r->path,&r->parse);
    if(stopped(cancelled,user)) goto cancel;
    if(okay) { r->phase="import"; okay=xl6ImportsExpandModule(r->parse.root,r->path,&r->parse.diags,&r->deps); }
    if(stopped(cancelled,user)) goto cancel;
    if(okay) { r->phase="stdlib"; okay=xl6StdlibExpandModule(r->parse.root,&r->parse.diags); }
    if(stopped(cancelled,user)) goto cancel;
    if(okay) {
        char module[256]; char* stem=xrtPathStem(r->path); if(!stem) goto invalid;
        snprintf(module,sizeof(module),"%s",*stem ? stem : "module"); xrtFree(stem);
        r->phase="sem";
        if(memoEnabled) {
            const xls_sem_memo* baseline=NULL;
            int compatible=previous && xlsFrontendContextsEqual(previous->context,context) && previous->memo && previous->captureIndex==captureIndex && !strcmp(previous->path,r->path) &&
                xlsSemanticCheckValidate(previous) && xlsInputTopologyOverlayCompatible(xlsSemanticCheckTopology(previous),&previous->overlay,&r->overlay);
            /* The frontend compares every function/environment and maps
             * coordinates against each report's original source bytes.
             * Membership is exact or differences have a complete original
             * lookup absence proof. Disk/probe/environment validity still
             * precedes every borrowed per-function semantic/IR fact. */
            if(compatible) {baseline=previous->memo;irBaseline=previous->cachedIr;}
            okay=xlsFrontendSemCheckMemo(r->parse.root,&r->parse.diags,module,captureIndex,r->path,
                &r->report,baseline ? &previous->report : NULL,baseline,&r->memo,&r->semanticWork);
        } else okay=xl6SemCheckModuleWithIndex(r->parse.root,&r->parse.diags,module,captureIndex);
    }
    if(stopped(cancelled,user)) goto cancel;
    /* Collect checked facts before raw IR so the optional cache can bind
     * external identities to THIS compilation. Delay index resource errors
     * until after original IR diagnostics;neither changes their precedence. */
    char indexError[256]={0};
    if(okay && captureIndex)r->typedIndex=xlsFrontendIndexCreate(r->parse.root,&r->report,indexError,sizeof(indexError));
    r->modules=xlsModuleGraphEnd(moduleScope,&r->report);
    xlsFrontendIndexAttachModules(r->typedIndex,r->modules);
    if(stopped(cancelled,user)) goto cancel;
    if(okay) {
        r->phase="ir";
        if(memoEnabled){
            /* Optional fragment failures never invent source diagnostics.
             * Keep attempts private;the original builder remains authoritative
             * for a failed attempt as well as the independent fresh API. */
            XL6_DiagList attempted;xl6DiagInit(&attempted);
            r->cachedIr=xlsFrontendIrResultBuildEntities(r->parse.root,&attempted,irBaseline,32u*1024u*1024u,xlsSemanticIndexEntities(r->typedIndex));
            okay=r->cachedIr && !attempted.count;
            if(!okay){xlsFrontendIrResultDestroy(r->cachedIr);r->cachedIr=NULL;}
            xl6DiagFree(&attempted);
            if(!okay && !stopped(cancelled,user))okay=xl6IrBuildModuleFromAst(&r->ir,r->parse.root,&r->parse.diags);
        }else okay=xl6IrBuildModuleFromAst(&r->ir,r->parse.root,&r->parse.diags);
    }
    if(stopped(cancelled,user)) goto cancel;
    r->accepted=okay && !r->parse.diags.count;
    if(r->accepted && captureIndex) {
        if(!r->typedIndex) {
            /* An index budget/allocation failure is infrastructure failure,
             * never a diagnostic claiming that valid user code is wrong. */
            fail(error,capacity,*indexError ? indexError : "cannot retain semantic index");
            xlsSemanticCheckDestroy(r); return NULL;
        }
    }
    if(!r->accepted){xlsFrontendIndexDestroy(r->typedIndex);r->typedIndex=NULL;}
    if(stopped(cancelled,user)) goto cancel;
    if(!xlsSemanticCheckValidate(r)) {
        fail(error,capacity,"semantic check inputs are incomplete or changed");
        xlsSemanticCheckDestroy(r); return NULL;
    }
    if(r->accepted) r->phase="checked";
    return r;
invalid:
    fail(error,capacity,"invalid semantic check overlay, path or capacity");
    xlsSemanticCheckDestroy(r); return NULL;
cancel:
    fail(error,capacity,"semantic check cancelled");
    xlsSemanticCheckDestroy(r); return NULL;
}
xls_semantic_check* xlsSemanticCheckCreateCachedInContext(xls_context* context,xls_entry_cache* cache,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!xlsFrontendContextValid(context,entry,error,capacity)) return NULL;
    xls_frontend_context_scope scope; xlsFrontendContextBegin(&scope,context);
    xls_ast_materialize_scope materialization;
    xlsAstMaterializeBegin(&materialization,(cache || syntax) ? xlsAstMaterializeBudget() : 0);
    xls_entry_cache_scope entryScope;xlsFrontendEntryBegin(&entryScope,cache,context,entry);
    xls_input_usage_scope usage;xlsFrontendInputUsageBegin(&usage);
    xls_input_topology_scope topology;xlsInputTopologyBegin(&topology,(cache || syntax) ? 8u*1024u*1024u : 0);
    xls_module_graph_scope modules;xlsModuleGraphBegin(&modules,(cache || syntax) ? xlsModuleScopeCurrent() : NULL,context,8u*1024u*1024u);
    xls_frontend_cancel_scope cancellation;xlsFrontendCancelBegin(&cancellation,cancelled,user);
    xls_semantic_check* result=createBody(context,entry,inputs,count,captureIndex,syntax,previous,cache || syntax,&modules,cancelled,user,error,capacity);
    uint64_t used=0;int known=xlsFrontendInputUsageEnd(&usage,&used);
    xls_input_topology* captured=xlsInputTopologyEnd(&topology);
    if(result && known)result->topology=captured;else xlsInputTopologyDestroy(captured);
    /* Early invalid/cancelled creation may leave the capture active. Ordinary
     * success already ended it after semantics,before the read-only IR phase. */
    xlsModuleGraphRelease(xlsModuleGraphEnd(&modules,NULL));
    if(result && stopped(cancelled,user)) {xlsSemanticCheckDestroy(result);result=NULL;fail(error,capacity,"semantic check cancelled");}
    xlsFrontendCancelEnd(&cancellation,NULL);
    int scoped=xlsFrontendContextEnd(&scope);
    xls_entry_parse_work work=xlsFrontendEntryEnd(&entryScope,result && scoped);
    xls_ast_materialize_work materializationWork;xlsAstMaterializeEnd(&materialization,&materializationWork);
    if(!scoped) {
        xlsSemanticCheckDestroy(result); fail(error,capacity,(scope.failed&2) ? "cannot retain bounded completion namespace" : "cannot resolve native context path or package environment"); return NULL;
    }
    if(result) {result->entryWork=work;result->moduleWork=entryScope.moduleWork;result->moduleSyntaxWork=entryScope.moduleSyntaxWork;result->materializationWork=materializationWork;result->usedInputs=used;result->usageKnown=known && result->report.complete && result->report.count && result->overlay.count==count;}
    return result;
}
int xlsSemanticCheckInputUsage(const xls_semantic_check* result,uint64_t* used)
{
    if(used)*used=0;
    if(!used || !result || invalid(result) || !result->usageKnown)return 0;
    *used=result->usedInputs;return 1;
}
const xls_input_topology* xlsSemanticCheckTopology(const xls_semantic_check* result)
{return result && !invalid(result) && result->usageKnown ? result->topology : NULL;}
const xls_module_graph* xlsSemanticCheckModules(const xls_semantic_check* result)
{return result && !invalid(result) ? result->modules : NULL;}
xls_semantic_check* xlsSemanticCheckCreateIncrementalInContext(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsSemanticCheckCreateCachedInContext(context,NULL,entry,inputs,count,captureIndex,syntax,previous,cancelled,user,error,capacity); }
xls_entry_parse_work xlsSemanticCheckEntryWork(const xls_semantic_check* result)
{ return result ? result->entryWork : (xls_entry_parse_work){0}; }
xls_entry_parse_work xlsSemanticCheckModuleWork(const xls_semantic_check* result)
{ return result ? result->moduleWork : (xls_entry_parse_work){0}; }
xls_parse_work xlsSemanticCheckModuleParseWork(const xls_semantic_check* result)
{ return result ? result->moduleSyntaxWork : (xls_parse_work){0}; }
xls_ast_materialize_work xlsSemanticCheckMaterializationWork(const xls_semantic_check* result)
{ return result ? result->materializationWork : (xls_ast_materialize_work){0}; }
xls_ast_intern_work xlsSemanticCheckMemoTypes(const xls_semantic_check* result)
{return xlsFrontendSemMemoTypes(result ? result->memo : NULL);}
xls_semantic_check* xlsSemanticCheckCreateIncremental(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsSemanticCheckCreateIncrementalInContext(NULL,entry,inputs,count,captureIndex,syntax,previous,cancelled,user,error,capacity); }
xls_semantic_check* xlsSemanticCheckCreateInContext(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsSemanticCheckCreateIncrementalInContext(context,entry,inputs,count,captureIndex,NULL,NULL,cancelled,user,error,capacity); }
const xls_context* xlsSemanticCheckContext(const xls_semantic_check* result)
{ return result ? result->context : NULL; }
xls_semantic_check* xlsSemanticCheckCreate(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsSemanticCheckCreateParsed(entry,inputs,count,captureIndex,NULL,cancelled,user,error,capacity); }
xls_semantic_check* xlsSemanticCheckCreateParsed(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return xlsSemanticCheckCreateIncremental(entry,inputs,count,captureIndex,syntax,NULL,cancelled,user,error,capacity); }
xls_check_semantic_work xlsSemanticCheckWork(const xls_semantic_check* r)
{ return r ? (xls_check_semantic_work){r->semanticWork.checked,r->semanticWork.reused,r->semanticWork.contextHits,r->semanticWork.bytes} : (xls_check_semantic_work){0}; }
xls_sem_read_work xlsSemanticCheckReads(const xls_semantic_check* r)
{return r && !invalid(r) && r->accepted ? xlsFrontendSemMemoReads(r->memo) : (xls_sem_read_work){0};}
const XL6_IrModule* xlsSemanticCheckIr(const xls_semantic_check* r)
{ return r && !invalid(r) && r->accepted ? (r->cachedIr ? xlsFrontendIrResultModule(r->cachedIr) : &r->ir) : NULL; }
xls_check_ir_work xlsSemanticCheckIrWork(const xls_semantic_check* r)
{
    xls_ir_memo_work w=xlsFrontendIrResultWork(r ? r->cachedIr : NULL);
    return (xls_check_ir_work){w.checked,w.reused,w.captured,w.nodesReused,w.bytes,w.declined,w.textNodes,w.textBytes,w.ownerBytes};
}
size_t xlsSemanticCheckIrDependencies(const xls_semantic_check* r,uint64_t function)
{return r && !invalid(r) && r->accepted ? xlsFrontendIrResultDependencies(r->cachedIr,function) : 0;}
int xlsSemanticCheckIrDependency(const xls_semantic_check* r,uint64_t function,size_t at,xls_check_ir_dependency* out)
{
    if(out)*out=(xls_check_ir_dependency){0};
    xls_ir_declaration_dependency row;
    if(!out || !r || invalid(r) || !r->accepted || !xlsFrontendIrResultDependency(r->cachedIr,function,at,&row))return 0;
    *out=(xls_check_ir_dependency){row.declaration,row.version};return 1;
}
const XL6_AstNode* xlsSemanticCheckAst(const xls_semantic_check* r)
{ return r && !invalid(r) ? r->parse.root : NULL; }
xls_check_info xlsSemanticCheckInfo(const xls_semantic_check* r)
{
    if(!r) return (xls_check_info){0};
    int stale=invalid(r);
    const XL6_IrModule* ir=r->cachedIr ? xlsFrontendIrResultModule(r->cachedIr) : &r->ir;
    return (xls_check_info){r->path,r->phase,stale ? "semantic inputs changed" : NULL,
        r->parse.diags.count,ir->types.count,ir->nodeCount,r->accepted && !stale,
        r->typedIndex && r->accepted && !stale};
}
int xlsSemanticCheckDiagnostic(const xls_semantic_check* r,size_t index,xls_check_diagnostic* out)
{
    if(!r || invalid(r) || !out || index>=r->parse.diags.count) return 0;
    const XL6_Diag* d=&r->parse.diags.items[index];
    *out=(xls_check_diagnostic){d->file ? d->file : r->path,d->message,d->line,d->column}; return 1;
}
int xlsSemanticCheckInputs(const xls_semantic_check* r,xls_check_inputs* out)
{
    return xlsInputReportInputs(r && !invalid(r) ? &r->report : NULL,out);
}
int xlsSemanticCheckSource(const xls_semantic_check* r,size_t index,xls_check_source* out)
{
    return xlsInputReportSource(r && !invalid(r) ? &r->report : NULL,index,out);
}
int xlsSemanticCheckProbe(const xls_semantic_check* r,size_t index,xls_check_probe* out)
{
    return xlsInputReportProbe(r && !invalid(r) ? &r->report : NULL,index,out);
}
const xls_semantic_index* xlsSemanticCheckTypedIndex(const xls_semantic_check* r)
{ return r && !invalid(r) && r->accepted ? r->typedIndex : NULL; }
static const char* indexLocked(xls_semantic_check* r)
{
    if(!xlsSemanticCheckTypedIndex(r)) return NULL;
    if(!r->index) r->index=xl6SemIndexJson(r->parse.root,&r->report,NULL,0);
    return r->index;
}
const char* xlsSemanticCheckIndex(xls_semantic_check* r)
{
    if(!r || !xrtMutexLock(&r->lock)) return NULL;
    const char* index=indexLocked(r); xrtMutexUnlock(&r->lock); return index;
}
static int validateLocked(xls_semantic_check* r)
{
    if(invalid(r)) return 0;
    xls_frontend_context_scope scope; xlsFrontendContextBegin(&scope,r->context);
    int okay=xl6SourceReportValidate(&r->report); int scoped=xlsFrontendContextEnd(&scope);
    if(!okay || !scoped) { xrtAtomic32Store(&r->invalid,1,XMEMORY_RELEASE); return 0; }
    return 1;
}
int xlsSemanticCheckValidate(xls_semantic_check* r)
{
    if(!r || !xrtMutexLock(&r->lock)) return 0;
    int okay=validateLocked(r); xrtMutexUnlock(&r->lock); return okay;
}
static char* reportLocked(xls_semantic_check* r,size_t* size,int includeIndex)
{
    if(size) *size=0;
    if(!validateLocked(r)) return NULL;
    const char* index=includeIndex && r->captureIndex ? (r->accepted ? indexLocked(r) :
        "{\"schema\":1,\"scope\":\"checked-compilation\",\"complete\":false,\"nodes\":[]}") : NULL;
    if(includeIndex && r->captureIndex && r->accepted && !index) return NULL;
    XL6_Output out; char* text=NULL; size_t length=0;
    if(xl6OutputOpenMemory(&out)) {
        xl6DiagPrintJsonWithIndex(&out,&r->parse.diags,r->path,r->phase,r->accepted,&r->report,index);
        if(!xl6OutputHasFailed(&out)) text=xl6OutputTakeMemoryText(&out,&length);
        xl6OutputDiscard(&out);
    }
    if(length>XLS_CHECK_REPORT_BYTES || !validateLocked(r)) { xrtFree(text); text=NULL; length=0; }
    if(size) *size=length;
    return text;
}
static char* reportJson(xls_semantic_check* r,size_t* size,int includeIndex)
{
    if(size) *size=0;
    if(!r || !xrtMutexLock(&r->lock)) return NULL;
    char* text=reportLocked(r,size,includeIndex); xrtMutexUnlock(&r->lock); return text;
}
char* xlsSemanticCheckJson(xls_semantic_check* r,size_t* size) { return reportJson(r,size,1); }
char* xlsSemanticCheckReportJson(xls_semantic_check* r,size_t* size) { return reportJson(r,size,0); }
