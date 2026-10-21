#include "analysis/folding.h"
#include "analysis/frontend_internal.h"
#include <stdlib.h>

struct xls_folding { xarray ranges; };
static int structural(XL6_AstKind kind)
{
    switch(kind) {
    case XL6_AST_FUNC: case XL6_AST_ASYNC_FUNC: case XL6_AST_ANON_FUNC:
    case XL6_AST_CLASS: case XL6_AST_PROTOCOL: case XL6_AST_ENUM: case XL6_AST_UNION:
    case XL6_AST_GET: case XL6_AST_LET: case XL6_AST_IF: case XL6_AST_ELSEIF:
    case XL6_AST_WHILE: case XL6_AST_DO_WHILE: case XL6_AST_FOR: case XL6_AST_MATCH:
    case XL6_AST_MATCH_CASE: case XL6_AST_TRY: case XL6_AST_CATCH: return 1;
    default: return 0;
    }
}
static int compare(const void* a,const void* b)
{
    const xls_fold_range *left=a,*right=b;
    if(left->startLine!=right->startLine) return left->startLine<right->startLine ? -1 : 1;
    return left->endLine==right->endLine ? 0 : left->endLine>right->endLine ? -1 : 1;
}
typedef struct fold_visit { xls_folding* result; size_t limit,lines; } fold_visit;
static int collect(const XL6_AstNode* node,const XL6_AstRange* current,void* user)
{
    fold_visit* context=user;
    if(structural(node->kind) && current->known && current->startLine>0 && current->endLine>current->startLine && (size_t)current->endLine<=context->lines) {
        xls_fold_range range={(size_t)current->startLine-1,(size_t)current->endLine-1};
        if(!xrtArrayPush(&context->result->ranges,&range)) return -1;
    }
    return context->result->ranges.Count<context->limit;
}
xls_folding* xlsFoldingCreate(const xls_syntax* syntax,size_t limit,int (*cancelled)(void*),void* user)
{
    if(!syntax || (cancelled && cancelled(user))) return NULL;
    xls_folding* result=xrtMalloc(sizeof(*result));
    if(!result) return NULL;
    if(!xrtArrayInit(&result->ranges,sizeof(xls_fold_range))) { xrtFree(result); return NULL; }
    if(!limit || !xlsSyntaxAccepted(syntax)) return result;
    fold_visit context={result,limit,xlsTextLines(xlsSyntaxText(syntax))};
    int okay=xlsSyntaxVisit(syntax,collect,&context,cancelled,user);
    if(!okay) { xlsFoldingDestroy(result); return NULL; }
    if(result->ranges.Count>1) {
        qsort(result->ranges.Data,result->ranges.Count,sizeof(xls_fold_range),compare);
        size_t count=0; xls_fold_range* ranges=(xls_fold_range*)result->ranges.Data;
        for(size_t i=0;i<result->ranges.Count;++i) if(!count || compare(&ranges[i],&ranges[count-1])) ranges[count++]=ranges[i];
        result->ranges.Count=count;
    }
    return result;
}
void xlsFoldingDestroy(xls_folding* folding) { if(folding) { xrtArrayUnit(&folding->ranges); xrtFree(folding); } }
size_t xlsFoldingCount(const xls_folding* folding) { return folding ? folding->ranges.Count : 0; }
const xls_fold_range* xlsFoldingRanges(const xls_folding* folding) { return folding ? (const xls_fold_range*)folding->ranges.Data : NULL; }
