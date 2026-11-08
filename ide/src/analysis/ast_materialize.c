#include "analysis/ast_materialize.h"
static _Thread_local xls_ast_materialize_scope* active;
/* A full-page-sized function/body gets independent backing. Keep small
 * declarations together to retain the existing allocation amortization. This
 * bounded read-only choice is not a source/semantic validity certificate. */
#define XLS_MATERIALIZE_SPLIT_NODES 64u
static int countForSplit(const XL6_AstNode* node,size_t* nodes,size_t depth)
{
    if(!node || *nodes>=XLS_MATERIALIZE_SPLIT_NODES)return 1;
    if(depth>512 || (node->childCount && !node->children))return 0;
    ++*nodes;
    for(size_t i=0;i<node->childCount && *nodes<XLS_MATERIALIZE_SPLIT_NODES;++i)
        if(!countForSplit(node->children[i],nodes,depth+1))return 0;
    return countForSplit(node->inferredType,nodes,depth+1) && countForSplit(node->semanticDeclaredType,nodes,depth+1);
}
static int separatePool(const XL6_AstNode* input)
{
    if(input->kind!=XL6_AST_FUNC && input->kind!=XL6_AST_ASYNC_FUNC && input->kind!=XL6_AST_EXPORT_FUNC && input->kind!=XL6_AST_BLOCK)return 0;
    size_t nodes=0;return countForSplit(input,&nodes,0) && nodes>=XLS_MATERIALIZE_SPLIT_NODES;
}
size_t xlsAstMaterializeBudget(void)
{
    if(!active)return 64u*1024u*1024u;
    uint64_t used=active->work.chargedBytes+xlsAstStorageWork(active->storage).bytes;
    return used<=active->limit ? active->limit-(size_t)used : 0;
}
void xlsAstMaterializeBegin(xls_ast_materialize_scope* scope,size_t bytes)
{
    if(!scope)return;
    size_t available=xlsAstMaterializeBudget();if(bytes>available)bytes=available;
    *scope=(xls_ast_materialize_scope){.previous=active,.limit=bytes};active=scope;
}
void xlsAstMaterializeEnd(xls_ast_materialize_scope* scope,xls_ast_materialize_work* result)
{
    if(!scope || active!=scope){if(result)*result=(xls_ast_materialize_work){0};return;}
    xls_ast_pool_work own=xlsAstStorageWork(scope->storage);
    xls_ast_materialize_work work=scope->work;work.nodes+=own.nodes;work.pages+=own.pages;work.chargedBytes+=own.bytes;
    active=scope->previous;
    if(active){
        active->work.calls+=work.calls;active->work.pooledCopies+=work.pooledCopies;active->work.heapCopies+=work.heapCopies;
        active->work.poolFallbacks+=work.poolFallbacks;active->work.nodes+=work.nodes;active->work.pages+=work.pages;active->work.chargedBytes+=work.chargedBytes;
    }
    if(result)*result=work;
    xlsAstStorageUnpin(scope->storage);scope->storage=NULL;
}
XL6_AstNode* xlsAstMaterialize(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes)
{
    if(!active)return xlsAstClone(input,options,bytes);
    ++active->work.calls;
    if(active->limit && input){
        if(separatePool(input)){
            /* Reuse the same nested-scope quota and cleanup. Its pool stays
             * visible to the active budget while cloning; End transfers the
             * actual backing charge and unpins it. The returned tree owns the
             * remaining node lifetime, independent of sibling copies. */
            xls_ast_materialize_scope tree;xlsAstMaterializeBegin(&tree,xlsAstMaterializeBudget());
            tree.storage=xlsAstStorageCreate(xlsAstMaterializeBudget());
            XL6_AstNode* root=tree.storage ? xlsAstCloneInStorage(input,options,tree.storage,xlsAstMaterializeBudget(),bytes) : NULL;
            if(root)++tree.work.pooledCopies;else ++tree.work.poolFallbacks;
            xlsAstMaterializeEnd(&tree,NULL);
            if(root)return root;
        }else{
            if(!active->storage)active->storage=xlsAstStorageCreate(xlsAstMaterializeBudget());
            if(active->storage){
                XL6_AstNode* root=xlsAstCloneInStorage(input,options,active->storage,xlsAstMaterializeBudget(),bytes);
                if(root){++active->work.pooledCopies;return root;}
            }
            ++active->work.poolFallbacks;
        }
    }
    XL6_AstNode* root=xlsAstClone(input,options,bytes);if(root)++active->work.heapCopies;return root;
}
