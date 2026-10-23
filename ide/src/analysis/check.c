#include "analysis/check.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_entry_cache.h"
#include "analysis/frontend_index.h"
#include "analysis/frontend_internal.h"
#include "analysis/frontend_sem.h"
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
    char *path,*index;
    xls_semantic_index* typedIndex;
    xls_sem_memo* memo;
    xls_sem_work semanticWork;
    const char* phase;
    int accepted,captureIndex;
};
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
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
    xl6IrModuleFree(&r->ir); xl6ParseResultFree(&r->parse);
    xl6ImportDepsFree(&r->deps); xl6SourceOverlayClear(&r->overlay); xl6SourceReportFree(&r->report);
    xlsContextRelease(r->context); xrtFree(r->path); if(r->lockInitialized) xrtMutexUnit(&r->lock); xrtFree(r);
}
static xls_semantic_check* createBody(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_semantic_check* r=NULL;
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
        if(syntax) {
            const xls_sem_memo* baseline=NULL;
            int compatible=previous && xlsFrontendContextsEqual(previous->context,context) && previous->memo && previous->captureIndex==captureIndex && !strcmp(previous->path,r->path) &&
                previous->overlay.count==r->overlay.count && xlsSemanticCheckValidate(previous);
            /* Changed imported overlays are not interchangeable merely
             * because they have the same public spelling. This first slice
             * retains entry-edit facts only; dependencies still recheck. */
            for(size_t i=0;compatible && i<r->overlay.count;++i) {
                const XL6_SourceOverlayItem* current=&r->overlay.items[i];
                const XL6_SourceOverlayItem* old=xl6SourceOverlayFind(&previous->overlay,current->path);
                compatible=old && !strcmp(old->path,current->path);
                if(compatible && strcmp(current->path,r->path)) compatible=old->size==current->size && !memcmp(old->text,current->text,current->size);
            }
            if(compatible) baseline=previous->memo;
            okay=xlsFrontendSemCheckMemo(r->parse.root,&r->parse.diags,module,captureIndex,r->path,
                (xls_text*)xlsSyntaxText(syntax),baseline,&r->memo,&r->semanticWork);
        } else okay=xl6SemCheckModuleWithIndex(r->parse.root,&r->parse.diags,module,captureIndex);
    }
    if(stopped(cancelled,user)) goto cancel;
    if(okay) { r->phase="ir"; okay=xl6IrBuildModuleFromAst(&r->ir,r->parse.root,&r->parse.diags); }
    if(stopped(cancelled,user)) goto cancel;
    r->accepted=okay && !r->parse.diags.count;
    if(r->accepted && captureIndex) {
        char indexError[256]={0};
        r->typedIndex=xlsFrontendIndexCreate(r->parse.root,&r->report,indexError,sizeof(indexError));
        if(!r->typedIndex) {
            /* An index budget/allocation failure is infrastructure failure,
             * never a diagnostic claiming that valid user code is wrong. */
            fail(error,capacity,*indexError ? indexError : "cannot retain semantic index");
            xlsSemanticCheckDestroy(r); return NULL;
        }
    }
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
xls_semantic_check* xlsSemanticCheckCreateIncrementalInContext(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,const xls_syntax* syntax,
    xls_semantic_check* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!xlsFrontendContextValid(context,entry,error,capacity)) return NULL;
    xls_frontend_context_scope scope; xlsFrontendContextBegin(&scope,context);
    xls_entry_cache_scope entryScope;xlsFrontendEntryBegin(&entryScope,NULL,context,entry);
    xls_semantic_check* result=createBody(context,entry,inputs,count,captureIndex,syntax,previous,cancelled,user,error,capacity);
    xlsFrontendEntryEnd(&entryScope,0);
    if(!xlsFrontendContextEnd(&scope)) {
        xlsSemanticCheckDestroy(result); fail(error,capacity,(scope.failed&2) ? "cannot retain bounded completion namespace" : "cannot resolve native context path or package environment"); return NULL;
    }
    return result;
}
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
const XL6_IrModule* xlsSemanticCheckIr(const xls_semantic_check* r)
{ return r && !invalid(r) && r->accepted ? &r->ir : NULL; }
const XL6_AstNode* xlsSemanticCheckAst(const xls_semantic_check* r)
{ return r && !invalid(r) ? r->parse.root : NULL; }
xls_check_info xlsSemanticCheckInfo(const xls_semantic_check* r)
{
    if(!r) return (xls_check_info){0};
    int stale=invalid(r);
    return (xls_check_info){r->path,r->phase,stale ? "semantic inputs changed" : NULL,
        r->parse.diags.count,r->ir.types.count,r->ir.nodeCount,r->accepted && !stale,
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
    if(out) memset(out,0,sizeof(*out));
    if(!r || !out || invalid(r)) return 0;
    /* Validation can mutate report.complete/error, never these captured rows.
     * Read only immutable fields; the atomic invalid flag gates new views. */
    *out=(xls_check_inputs){r->report.count,r->report.probeCount,r->report.packagePath,r->report.packagePathObserved};
    return 1;
}
int xlsSemanticCheckSource(const xls_semantic_check* r,size_t index,xls_check_source* out)
{
    if(out) memset(out,0,sizeof(*out));
    if(!r || !out || invalid(r) || index>=r->report.count) return 0;
    const XL6_SourceReportItem* source=&r->report.items[index];
    out->path=source->path; out->text=source->text; out->origin=source->origin;
    out->size=source->size; out->rawSize=source->rawSize; out->kind=source->kind;
    memcpy(out->textSha256,source->textSha256,32); memcpy(out->rawSha256,source->rawSha256,32);
    return 1;
}
int xlsSemanticCheckProbe(const xls_semantic_check* r,size_t index,xls_check_probe* out)
{
    if(out) memset(out,0,sizeof(*out));
    if(!r || !out || invalid(r) || index>=r->report.probeCount) return 0;
    *out=(xls_check_probe){r->report.probes[index].path,r->report.probes[index].exists}; return 1;
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
