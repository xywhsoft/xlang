#include "analysis/check.h"
#include "src/parser/xl6_parser_api.h"
#include "src/compile/xl6_imports.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include "src/sem/xl6_sem_index.h"
#include "src/ir/xl6_ir.h"
#include <stdio.h>

struct xls_semantic_check {
    XL6_ParseResult parse;
    XL6_ImportDeps deps;
    XL6_SourceReport report;
    XL6_SourceOverlay overlay;
    XL6_IrModule ir;
    char *path,*index;
    const char* phase;
    int accepted,invalid,captureIndex;
};
static int stopped(int (*cancelled)(void*),void* user) { return cancelled && cancelled(user); }
static void fail(char* error,size_t capacity,const char* message)
{ if(error && capacity) snprintf(error,capacity,"%s",message); }
void xlsSemanticCheckDestroy(xls_semantic_check* r)
{
    if(!r) return;
    xrtFree(r->index); xl6IrModuleFree(&r->ir); xl6ParseResultFree(&r->parse);
    xl6ImportDepsFree(&r->deps); xl6SourceOverlayClear(&r->overlay); xl6SourceReportFree(&r->report);
    xrtFree(r->path); xrtFree(r);
}
xls_semantic_check* xlsSemanticCheckCreate(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity)
{
    xls_semantic_check* r=NULL;
    if(error && capacity) *error=0;
    if(!entry || !xrtPathIsAbs(entry) || count>XL6_SOURCE_OVERLAY_FILES ||
       (count && !inputs) || (captureIndex!=0 && captureIndex!=1)) {
        fail(error,capacity,"check requires an absolute entry and 0..64 overlays"); return NULL;
    }
    if(stopped(cancelled,user)) { fail(error,capacity,"semantic check cancelled"); return NULL; }
    r=xrtCalloc(1,sizeof(*r)); if(!r) { fail(error,capacity,"cannot allocate semantic check"); return NULL; }
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
    r->phase="parse"; int okay=xl6SourceOverlayParse(&r->overlay,r->path,&r->parse);
    if(stopped(cancelled,user)) goto cancel;
    if(okay) { r->phase="import"; okay=xl6ImportsExpandModule(r->parse.root,r->path,&r->parse.diags,&r->deps); }
    if(stopped(cancelled,user)) goto cancel;
    if(okay) { r->phase="stdlib"; okay=xl6StdlibExpandModule(r->parse.root,&r->parse.diags); }
    if(stopped(cancelled,user)) goto cancel;
    if(okay) {
        char module[256]; char* stem=xrtPathStem(r->path); if(!stem) goto invalid;
        snprintf(module,sizeof(module),"%s",*stem ? stem : "module"); xrtFree(stem);
        r->phase="sem"; okay=xl6SemCheckModuleWithIndex(r->parse.root,&r->parse.diags,module,captureIndex);
    }
    if(stopped(cancelled,user)) goto cancel;
    if(okay) { r->phase="ir"; okay=xl6IrBuildModuleFromAst(&r->ir,r->parse.root,&r->parse.diags); }
    if(stopped(cancelled,user)) goto cancel;
    r->accepted=okay && !r->parse.diags.count;
    if(r->accepted && captureIndex) {
        char indexError[256]={0};
        r->index=xl6SemIndexJson(r->parse.root,&r->report,indexError,sizeof(indexError));
        if(!r->index) {
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
xls_check_info xlsSemanticCheckInfo(const xls_semantic_check* r)
{
    if(!r) return (xls_check_info){0};
    return (xls_check_info){r->path,r->phase,r->invalid ? "semantic inputs changed" : NULL,
        r->parse.diags.count,r->ir.types.count,r->ir.nodeCount,r->accepted && !r->invalid,
        r->index && r->accepted && !r->invalid};
}
int xlsSemanticCheckDiagnostic(const xls_semantic_check* r,size_t index,xls_check_diagnostic* out)
{
    if(!r || r->invalid || !out || index>=r->parse.diags.count) return 0;
    const XL6_Diag* d=&r->parse.diags.items[index];
    *out=(xls_check_diagnostic){d->file ? d->file : r->path,d->message,d->line,d->column}; return 1;
}
const char* xlsSemanticCheckIndex(const xls_semantic_check* r)
{ return r && !r->invalid && r->accepted ? r->index : NULL; }
int xlsSemanticCheckValidate(xls_semantic_check* r)
{
    if(!r || r->invalid) return 0;
    if(!xl6SourceReportValidate(&r->report)) { r->invalid=1; return 0; }
    return 1;
}
char* xlsSemanticCheckJson(xls_semantic_check* r,size_t* size)
{
    if(size) *size=0;
    if(!r || !xlsSemanticCheckValidate(r)) return NULL;
    const char* index=r->captureIndex ? (r->accepted ? r->index :
        "{\"schema\":1,\"scope\":\"checked-compilation\",\"complete\":false,\"nodes\":[]}") : NULL;
    XL6_Output out; char* text=NULL; size_t length=0;
    if(xl6OutputOpenMemory(&out)) {
        xl6DiagPrintJsonWithIndex(&out,&r->parse.diags,r->path,r->phase,r->accepted,&r->report,index);
        if(!xl6OutputHasFailed(&out)) text=xl6OutputTakeMemoryText(&out,&length);
        xl6OutputDiscard(&out);
    }
    if(length>XLS_CHECK_REPORT_BYTES || !xlsSemanticCheckValidate(r)) { xrtFree(text); text=NULL; length=0; }
    if(size) *size=length;
    return text;
}
