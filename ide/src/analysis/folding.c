#include "analysis/folding.h"
#include "analysis/frontend_internal.h"
#include <stdlib.h>

struct xls_folding { xarray ranges; };
typedef struct walk_frame { const XL6_AstNode* node; size_t next; } walk_frame;
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
xls_folding* xlsFoldingCreate(const xls_syntax* syntax,size_t limit,int (*cancelled)(void*),void* user)
{
    if(!syntax || (cancelled && cancelled(user))) return NULL;
    xls_folding* result=xrtMalloc(sizeof(*result));
    if(!result) return NULL;
    if(!xrtArrayInit(&result->ranges,sizeof(xls_fold_range))) { xrtFree(result); return NULL; }
    if(!limit || !xlsSyntaxAccepted(syntax)) return result;
    xarray stack={0}; int okay=xrtArrayInit(&stack,sizeof(walk_frame));
    const XL6_AstNode* root=xlsSyntaxAst(syntax); size_t visited=0,lines=xlsTextLines(xlsSyntaxText(syntax));
    walk_frame initial={root,0};
    if(root && okay) okay=xrtArrayPush(&stack,&initial);
    while(okay && stack.Count && result->ranges.Count<limit) {
        walk_frame* frame=xrtArrayGet(&stack,stack.Count-1); const XL6_AstNode* node=frame->node;
        if(!frame->next) {
            if((visited++&63u)==0 && cancelled && cancelled(user)) { okay=0; break; }
            if(structural(node->kind) && node->range.known && node->range.startLine>0 &&
                node->range.endLine>node->range.startLine && (size_t)node->range.endLine<=lines) {
                xls_fold_range range={(size_t)node->range.startLine-1,(size_t)node->range.endLine-1};
                okay=xrtArrayPush(&result->ranges,&range); if(!okay) break;
            }
        }
        if(frame->next<node->childCount) {
            const XL6_AstNode* child=node->children[frame->next++];
            if(child) { walk_frame next={child,0}; okay=xrtArrayPush(&stack,&next); }
        } else --stack.Count;
    }
    xrtArrayUnit(&stack);
    if(cancelled && cancelled(user)) okay=0;
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
