#include "analysis/semantic.h"
#include "src/parser/xl6_parser_api.h"
#include "src/compile/xl6_imports.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_completion.h"
#include <stdio.h>

struct xls_semantic_completion {
    XL6_ParseResult parse;
    XL6_ImportDeps deps;
    XL6_SourceReport report;
    XL6_SourceOverlay overlay;
    XL6_CompletionQuery query;
    char* path;
    int invalid;
};
static int absolute(const char* path)
{
    return path && *path && xrtPathIsAbs(path);
}
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
static void fail(char* error,size_t capacity,const char* message)
{ if(error && capacity) snprintf(error,capacity,"%s",message); }
static int ready(const xls_semantic_completion* r)
{
    return r && !r->invalid && r->query.available && !r->query.error &&
        r->query.parsed==1 && r->query.reached==1 && r->report.complete;
}
void xlsSemanticCompletionDestroy(xls_semantic_completion* r)
{
    if(!r) return;
    xl6CompletionFree(&r->query);
    xl6ParseResultFree(&r->parse);
    xl6ImportDepsFree(&r->deps);
    xl6SourceOverlayClear(&r->overlay);
    xl6SourceReportFree(&r->report);
    xrtFree(r->path); xrtFree(r);
}
xls_semantic_completion* xlsSemanticCompletionCreate(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_semantic_completion* r=NULL;
    if(error && capacity) *error=0;
    if(!absolute(entry) || !absolute(queryPath) || !inputs || !count || count>XL6_SOURCE_OVERLAY_FILES) {
        fail(error,capacity,"semantic completion requires absolute paths and 1..64 overlays"); return NULL;
    }
    if(stopped(cancelled,user)) { fail(error,capacity,"semantic completion cancelled"); return NULL; }
    r=xrtCalloc(1,sizeof(*r)); if(!r) { fail(error,capacity,"cannot allocate semantic completion"); return NULL; }
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
    if(!target || !xl6SourceOverlayFind(&r->overlay,entry) || offset>target->size ||
       (offset<target->size && ((unsigned char)target->text[offset]&0xc0)==0x80)) goto invalid;
    r->overlay.report=&r->report; r->overlay.completionQuery=&r->query; r->deps.sourceOverlay=&r->overlay;
    int okay=xl6SourceOverlayParse(&r->overlay,entry,&r->parse);
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
    xlsSemanticCompletionValidate(r);
    return r;
invalid:
    fail(error,capacity,"invalid semantic overlay, path, cursor or capacity");
    xlsSemanticCompletionDestroy(r); return NULL;
cancel:
    fail(error,capacity,"semantic completion cancelled");
    xlsSemanticCompletionDestroy(r); return NULL;
}
xls_completion_info xlsSemanticCompletionInfo(const xls_semantic_completion* r)
{
    if(!r) return (xls_completion_info){0};
    return (xls_completion_info){r->path,r->query.prefix,r->invalid ? "semantic inputs changed" : r->query.error,
        r->query.offset,r->query.replacement.startOffset,r->query.replacement.endOffset,
        ready(r) ? r->query.count : 0,ready(r)};
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
int xlsSemanticCompletionValidate(xls_semantic_completion* r)
{
    if(!r || r->invalid) return 0;
    if(!xl6SourceReportValidate(&r->report)) { r->invalid=1; r->query.available=0; return 0; }
    return 1;
}
char* xlsSemanticCompletionJson(xls_semantic_completion* r,size_t* size)
{
    if(size) *size=0;
    if(!r || !xlsSemanticCompletionValidate(r)) return NULL;
    char* completion=xl6CompletionJson(&r->query,&r->report); if(!completion) return NULL;
    XL6_Output out; char* text=NULL;
    if(xl6OutputOpenMemory(&out)) {
        xl6DiagPrintJsonWithCompletion(&out,&r->report,ready(r),completion);
        if(!xl6OutputHasFailed(&out)) text=xl6OutputTakeMemoryText(&out,size);
        xl6OutputDiscard(&out);
    }
    xrtFree(completion);
    if(text && size && *size>12u*1024u*1024u) { xrtFree(text); text=NULL; *size=0; }
    if(!xlsSemanticCompletionValidate(r)) { xrtFree(text); text=NULL; if(size) *size=0; }
    return text;
}
void xlsSemanticStringFree(char* text) { xrtFree(text); }
