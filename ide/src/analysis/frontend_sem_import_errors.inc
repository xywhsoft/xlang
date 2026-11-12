/* Query-only isolation of proven foreign free-function body diagnostics.
 * Keep the original list, checker order and return status. No failed body
 * becomes a memo fact. Header/default/type/export errors remain prerequisites.
 * The normal check/IR path never enters this adapter. */
#include "analysis/frontend_completion_path.h"
static int xlsSemQueryErrors(const XL6_SemContext* ctx)
{return ctx->diags && ctx->diags->count!=ctx->xlsImportedBodyDiagnostics;}

#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
typedef struct xls_sem_import_body {
    struct xls_sem_import_body* previous;
    XL6_SemContext* context;
    XL6_AstNode* node;
    size_t diagnostics;
    int headerAccepted;
} xls_sem_import_body;
static _Thread_local xls_sem_import_body* xlsActiveImportBody;
static void xlsSemImportedBodyStart(XL6_SemContext* ctx,XL6_AstNode* node,int okay)
{
    xls_sem_import_body* frame=xlsActiveImportBody;
    if(frame && frame->context==ctx && frame->node==node){
        frame->diagnostics=ctx->diags->count;
        frame->headerAccepted=okay && !xlsSemQueryErrors(ctx);
    }
}
static int xlsSemCheckFuncBodyOriginal(XL6_SemContext*,XL6_AstNode*);
static int xlsSemCheckFuncOriginal(XL6_SemContext* ctx,XL6_AstNode* node)
{
    XL6_CompletionQuery* query=ctx->completionQuery;
    if(!query)return xlsSemCheckFuncBodyOriginal(ctx,node);
    const char* path=xl6AstSourcePath(node);
    if(query->error || query->reached || !ctx->diags ||
       xlsSemQueryErrors(ctx) || ctx->currentFunction || ctx->currentClass ||
       ctx->typeParams || ctx->captureBoundary || ctx->currentReturns ||
       !ctx->scope || ctx->scope->parent || ctx->flowFactCount ||
       !node || !node->source || node->source->completionQuery || !path ||
       !query->path ||
       (node->kind!=XL6_AST_FUNC && node->kind!=XL6_AST_ASYNC_FUNC && node->kind!=XL6_AST_EXPORT_FUNC))
        return xlsSemCheckFuncBodyOriginal(ctx,node);
    xls_sem_import_body frame={xlsActiveImportBody,ctx,node,ctx->diags->count,0};
    size_t types=ctx->types.count,children=ctx->root->childCount;
    uint32 typeId=ctx->types.nextId;
    xls_sem_effect_scope effects;xlsSemEffectScopeBegin(&effects,ctx);
    xlsActiveImportBody=&frame;
    int okay=xlsSemCheckFuncBodyOriginal(ctx,node);
    xlsActiveImportBody=frame.previous;
    int pure=xlsSemEffectScopeEnd(&effects);
    if(!okay && frame.headerAccepted && ctx->diags->count>frame.diagnostics &&
       pure && types==ctx->types.count && typeId==ctx->types.nextId &&
       children==ctx->root->childCount && !query->error && !query->reached &&
       !ctx->flowFactCount && xl6PathIdentityEqual(path,query->path)==0){
        /* Module values could carry assignment/flow state across a failed
         * body. Decline that domain. This error-only proof costs no extra
         * allocation, path conversion or AST walk on successful functions.
         * A path comparison error never proves that sources differ. */
        size_t i;
        for(i=0;i<ctx->scope->count;++i)
            if(ctx->scope->items[i].kind!=XL6_SYM_FUNC)break;
        if(i==ctx->scope->count){
            for(i=0;i<children;++i)if(ctx->root->children[i]==node)break;
            if(i<children)ctx->xlsImportedBodyDiagnostics+=ctx->diags->count-frame.diagnostics;
        }
    }
    if(!okay && !query->error && !query->reached && ctx->diags->count==frame.diagnostics)
        query->error="imported function semantics failed without diagnostics";
    return okay;
}
#else
#define xlsSemImportedBodyStart(ctx,node,okay) ((void)0)
static int xlsSemCheckFuncBodyOriginal(XL6_SemContext*,XL6_AstNode*);
static int xlsSemCheckFuncOriginal(XL6_SemContext* ctx,XL6_AstNode* node)
{return xlsSemCheckFuncBodyOriginal(ctx,node);}
#endif
