/* Included after the shared alias-declaration predicate. Original selection,
 * name matching, chain limit and canonical rules remain upstream. */
#include "analysis/frontend_types.h"
#define XLS_TYPE_ALIAS_BYTES (8u*1024u*1024u)
static _Thread_local xls_type_alias_scope* xlsActiveAliasScope;
static _Thread_local xls_type_alias_work xlsTypeAliasWork;
static const XL6_AstNode* xlsTypeFindAliasOriginal(const XL6_AstNode*,const char*);
void xlsFrontendTypeAliasBegin(xls_type_alias_scope* scope,const XL6_AstNode* root,size_t bytes)
{
    if(!scope) return;
    memset(scope,0,sizeof(*scope)); scope->root=root;
    scope->previous=xlsActiveAliasScope; xlsActiveAliasScope=scope;
    if(!root || !bytes) return;
    if(bytes>XLS_TYPE_ALIAS_BYTES) bytes=XLS_TYPE_ALIAS_BYTES;
    size_t count=0;
    for(size_t i=0;i<root->childCount;++i) {
        ++xlsTypeAliasWork.rootVisits;
        if(xl6TypeIsAliasDecl(root->children[i]) && ++count>bytes/sizeof(*scope->items)) return;
    }
    size_t charged=count*sizeof(*scope->items);
    scope->items=count ? xrtMalloc(charged) : NULL;
    if(count && !scope->items) return;
    size_t next=0;
    for(size_t i=0;i<root->childCount;++i)
        if(xl6TypeIsAliasDecl(root->children[i])) scope->items[next++]=root->children[i];
    scope->sourceChildren=root->children; scope->sourceCount=root->childCount;
    scope->filtered=*root; scope->filtered.children=scope->items;
    scope->filtered.childCount=scope->filtered.childCapacity=count; scope->ready=1;
    ++xlsTypeAliasWork.indexes; xlsTypeAliasWork.aliases+=count;
    if(charged>xlsTypeAliasWork.peakBytes) xlsTypeAliasWork.peakBytes=charged;
}
void xlsFrontendTypeAliasEnd(xls_type_alias_scope* scope)
{
    if(!scope || xlsActiveAliasScope!=scope) return;
    xlsActiveAliasScope=scope->previous; xrtFree(scope->items); memset(scope,0,sizeof(*scope));
}
static const XL6_AstNode* xl6TypeFindAlias(const XL6_AstNode* root,const char* name)
{
    xls_type_alias_scope* scope=xlsActiveAliasScope;
    if(!scope) return xlsTypeFindAliasOriginal(root,name);
    ++xlsTypeAliasWork.queries;
    if(scope && scope->ready && scope->root==root && root->children==scope->sourceChildren && root->childCount==scope->sourceCount) {
        ++xlsTypeAliasWork.indexedQueries; return xlsTypeFindAliasOriginal(&scope->filtered,name);
    }
    ++xlsTypeAliasWork.linearQueries; return xlsTypeFindAliasOriginal(root,name);
}
void xlsFrontendTypeAliasReset(void) { memset(&xlsTypeAliasWork,0,sizeof(xlsTypeAliasWork)); }
xls_type_alias_work xlsFrontendTypeAliasWork(void) { return xlsTypeAliasWork; }
