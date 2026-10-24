#include "analysis/ast_materialize.h"
static _Thread_local xls_ast_materialize_scope* active;
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
        if(!active->storage)active->storage=xlsAstStorageCreate(xlsAstMaterializeBudget());
        if(active->storage){
            XL6_AstNode* root=xlsAstCloneInStorage(input,options,active->storage,xlsAstMaterializeBudget(),bytes);
            if(root){++active->work.pooledCopies;return root;}
        }
        ++active->work.poolFallbacks;
    }
    XL6_AstNode* root=xlsAstClone(input,options,bytes);if(root)++active->work.heapCopies;return root;
}
