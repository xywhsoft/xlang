#include "analysis/semantic.h"
#include "analysis/frontend_context.h"
#include "analysis/frontend_internal.h"
#include "src/parser/xl6_parser_api.h"
#include "src/compile/xl6_imports.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_completion.h"
#include <stdio.h>

struct xls_semantic_completion {
    xatomic32 references,invalid;
    xls_context* context;
    xmutex lock;
    int lockInitialized,available;
    XL6_ParseResult parse;
    XL6_ImportDeps deps;
    XL6_SourceReport report;
    XL6_SourceOverlay overlay;
    XL6_CompletionQuery query;
    char *path,*queryError;
};
static int absolute(const char* path)
{
    return path && *path && xrtPathIsAbs(path);
}
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
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
    xl6CompletionFree(&r->query);
    xl6ParseResultFree(&r->parse);
    xl6ImportDepsFree(&r->deps);
    xl6SourceOverlayClear(&r->overlay);
    xl6SourceReportFree(&r->report);
    xlsContextRelease(r->context); xrtFree(r->queryError); xrtFree(r->path); if(r->lockInitialized) xrtMutexUnit(&r->lock); xrtFree(r);
}
static xls_semantic_completion* createBody(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_semantic_completion* r=NULL;
    if(error && capacity) *error=0;
    if(!absolute(entry) || !absolute(queryPath) || !inputs || !count || count>XL6_SOURCE_OVERLAY_FILES) {
        fail(error,capacity,"semantic completion requires absolute paths and 1..64 overlays"); return NULL;
    }
    if(stopped(cancelled,user)) { fail(error,capacity,"semantic completion cancelled"); return NULL; }
    r=xrtCalloc(1,sizeof(*r)); if(!r) { fail(error,capacity,"cannot allocate semantic completion"); return NULL; }
    xrtAtomic32Init(&r->references,1); xrtAtomic32Init(&r->invalid,0); r->context=xlsContextRetain(context);
    if(!xrtMutexInit(&r->lock)) goto invalid;
    r->lockInitialized=1;
    xl6ParseResultInit(&r->parse); xl6ImportDepsInit(&r->deps); xl6SourceReportInit(&r->report);
    r->path=xrtPathAbs(queryPath); r->query.path=r->path; r->query.offset=offset;
    if(!r->path) goto invalid;
    for(size_t i=0;i<count;++i) {
        const xls_semantic_input* in=&inputs[i]; size_t bom=0;
        if(stopped(cancelled,user)) goto cancel;
        if(!absolute(in->path) || !in->text || in->size>XL6_SOURCE_OVERLAY_BYTES ||
           xl6SourceOverlayFind(&r->overlay,in->path)) goto invalid;
        if(in->size>=3 && !memcmp(in->text,"\xef\xbb\xbf",3)) bom=3;
        if(!xl6SourceOverlaySet(&r->overlay,in->path,in->text+bom,in->size-bom)) goto invalid;
    }
    const XL6_SourceOverlayItem* target=xl6SourceOverlayFind(&r->overlay,r->path);
    const XL6_SourceOverlayItem* entryInput=xl6SourceOverlayFind(&r->overlay,entry);
    if(!target || !entryInput || offset>target->size ||
       (offset<target->size && ((unsigned char)target->text[offset]&0xc0)==0x80)) goto invalid;
    r->overlay.report=&r->report; r->overlay.completionQuery=&r->query; r->deps.sourceOverlay=&r->overlay;
    int okay;
    if(syntax) {
        const xls_text* text=xlsSyntaxText(syntax); size_t skip=0;
        if(text && xlsTextSize(text)>=3 && !memcmp(xlsTextData(text),"\xef\xbb\xbf",3)) skip=3;
        /* Snapshot and source report describe the same entry bytes. A parse
         * for another cursor/mode/path is never substituted for this query. */
        if(entryInput!=target || !text || !xlsSyntaxIsCursor(syntax) ||
           strcmp(target->path,xlsSyntaxPath(syntax)) || target->size!=xlsTextSize(text)-skip ||
           memcmp(target->text,xlsTextData(text)+skip,target->size)) goto invalid;
        if(!xl6SourceReportText(&r->report,target->path,target->text,target->size,"overlay") ||
           !xlsSyntaxCopyCursorParse(syntax,&r->query,&r->parse)) goto invalid;
        if(r->query.error) {
            r->queryError=xrtStrDup(r->query.error); if(!r->queryError) goto invalid;
            r->query.error=r->queryError;
        }
        okay=xlsSyntaxAccepted(syntax);
    } else okay=xl6SourceOverlayParse(&r->overlay,entry,&r->parse);
    if(stopped(cancelled,user)) goto cancel;
    if(okay) okay=xl6ImportsExpandModule(r->parse.root,entry,&r->parse.diags,&r->deps);
    if(stopped(cancelled,user)) goto cancel;
    if(okay) okay=xl6StdlibExpandModule(r->parse.root,&r->parse.diags);
    if(stopped(cancelled,user)) goto cancel;
    /* A successful cursor projection intentionally stops semantic checking
     * early. Its available/reached flags, not full-check return, are decisive. */
    if(okay) {
        /* Match the compiler's module identity without borrowing its mutable
         * CompileUnit/IR pipeline. XRT already provides path decomposition. */
        char module[256]; char* stem=xrtPathStem(entry);
        if(!stem) goto invalid;
        snprintf(module,sizeof(module),"%s",*stem ? stem : "module"); xrtFree(stem);
        xl6SemCheckModuleWithQuery(r->parse.root,&r->parse.diags,module,0,&r->query);
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
static xls_semantic_completion* create(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!xlsFrontendContextValid(context,entry,error,capacity)) return NULL;
    xls_frontend_context_scope scope; xlsFrontendContextBegin(&scope,context);
    xls_semantic_completion* result=createBody(context,entry,queryPath,offset,inputs,count,syntax,cancelled,user,error,capacity);
    if(!xlsFrontendContextEnd(&scope)) {
        xlsSemanticCompletionDestroy(result); fail(error,capacity,(scope.failed&2) ? "cannot retain bounded completion namespace" : "cannot resolve native context path or package environment"); return NULL;
    }
    return result;
}
xls_semantic_completion* xlsSemanticCompletionCreateInContext(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return create(context,entry,queryPath,offset,inputs,count,NULL,cancelled,user,error,capacity); }
xls_semantic_completion* xlsSemanticCompletionCreateParsedInContext(xls_context* context,const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!syntax) { fail(error,capacity,"parsed completion requires cursor syntax"); return NULL; }
    return create(context,entry,queryPath,offset,inputs,count,syntax,cancelled,user,error,capacity);
}
const xls_context* xlsSemanticCompletionContext(const xls_semantic_completion* result)
{ return result ? result->context : NULL; }
xls_semantic_completion* xlsSemanticCompletionCreate(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{ return create(NULL,entry,queryPath,offset,inputs,count,NULL,cancelled,user,error,capacity); }
xls_semantic_completion* xlsSemanticCompletionCreateParsed(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,const xls_syntax* syntax,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    if(!syntax) { fail(error,capacity,"parsed completion requires cursor syntax"); return NULL; }
    return create(NULL,entry,queryPath,offset,inputs,count,syntax,cancelled,user,error,capacity);
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
char* xlsSemanticCompletionJson(xls_semantic_completion* r,size_t* size)
{
    if(size) *size=0;
    if(!r || !xrtMutexLock(&r->lock)) return NULL;
    char* text=jsonLocked(r,size); xrtMutexUnlock(&r->lock); return text;
}
void xlsSemanticStringFree(char* text) { xrtFree(text); }
