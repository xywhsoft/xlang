#include "analysis/frontend_symbols.h"
#include "analysis/frontend_cancel.h"
#include <string.h>
void xlsLexicalSymbolsClear(xls_lexical_symbols* symbols)
{
    if(!symbols)return;
    xrtArrayUnit(&symbols->order);xrtArrayUnit(&symbols->sorted);memset(symbols,0,sizeof(*symbols));
}
static int xlsLexicalInside(const XL6_AstNode* root,const XL6_AstNode* node)
{
    const char *a=xl6AstSourcePath(root),*b=xl6AstSourcePath(node);
    return a && b && !strcmp(a,b) && root->range.known && node->range.known &&
        root->range.endOffset>=root->range.startOffset && node->range.endOffset>=node->range.startOffset &&
        node->range.startOffset>=root->range.startOffset && node->range.endOffset<=root->range.endOffset;
}
static int xlsLexicalGlobal(const XL6_AstNode* root,XL6_SymbolId id)
{
    if(id==root->semanticSymbolId || id==root->semanticDeclaredSymbolId)return 1;
    /* The compiler gives the function entity and its module-scope NAME
     * separate IDs. Both are global,including recursive references. */
    const XL6_AstNode* name=root->childCount && root->children ? root->children[0] : NULL;
    return name && name->kind==XL6_AST_NAME &&
        (id==name->semanticSymbolId || id==name->semanticDeclaredSymbolId);
}
static int xlsLexicalGather(xls_lexical_symbols* symbols,const XL6_AstNode* node,size_t depth,size_t limit,size_t* visited)
{
    if(!node)return 1;
    if(depth>512 || ((++*visited&63u)==0 && xlsFrontendCancelPoint(XLS_CANCEL_IR_NODE,1)))return 0;
    XL6_SymbolId id=node->semanticDeclaredSymbolId;
    if(id && node!=symbols->root && !xlsLexicalGlobal(symbols->root,id) && xlsLexicalInside(symbols->root,node)){
        if(symbols->order.Count>=UINT32_MAX || symbols->order.Count>=limit/(4*sizeof(xls_lexical_symbol)))return 0;
        xls_lexical_symbol row={id,(uint32)(symbols->order.Count+1)};
        if(!xrtArrayPush(&symbols->order,&row) || symbols->order.Capacity>limit/(2*sizeof(row)))return 0;
    }
    if(node->childCount && !node->children)return 0;
    /* Inferred/declared fact types can contain foreign declaration objects.
     * They remain exact;only lexical children establish ownership here. */
    for(size_t i=0;i<node->childCount;++i)if(!xlsLexicalGather(symbols,node->children[i],depth+1,limit,visited))return 0;
    return 1;
}
static int xlsLexicalCompare(const void* left,const void* right)
{
    const xls_lexical_symbol *a=left,*b=right;
    if(a->symbol!=b->symbol)return a->symbol<b->symbol ? -1 : 1;
    return (a->slot>b->slot)-(a->slot<b->slot);
}
int xlsLexicalSymbolsBuild(xls_lexical_symbols* symbols,const XL6_AstNode* root,size_t bytes)
{
    if(!symbols)return 0;
    xlsLexicalSymbolsClear(symbols);if(bytes>XLS_LEXICAL_SYMBOL_BYTES)bytes=XLS_LEXICAL_SYMBOL_BYTES;
    if(!root || !bytes || (root->kind!=XL6_AST_FUNC && root->kind!=XL6_AST_ASYNC_FUNC && root->kind!=XL6_AST_EXPORT_FUNC))return 0;
    symbols->root=root;size_t visited=0;
    if(!xrtArrayInit(&symbols->order,sizeof(xls_lexical_symbol)) || !xrtArrayInit(&symbols->sorted,sizeof(xls_lexical_symbol)) ||
       !xlsLexicalGather(symbols,root,0,bytes,&visited) || !xrtArrayResize(&symbols->sorted,symbols->order.Count))goto failed;
    symbols->bytes=(symbols->order.Capacity+symbols->sorted.Capacity)*sizeof(xls_lexical_symbol);
    if(symbols->bytes>bytes)goto failed;
    if(symbols->order.Count)memcpy(symbols->sorted.Data,symbols->order.Data,symbols->order.Count*sizeof(xls_lexical_symbol));
    if(!xrtArraySort(&symbols->sorted,xlsLexicalCompare))goto failed;
    symbols->ready=1;return 1;
failed:xlsLexicalSymbolsClear(symbols);return 0;
}
XL6_SymbolId xlsLexicalSymbolKey(const xls_lexical_symbols* symbols,XL6_SymbolId symbol,int* local)
{
    *local=0;if(!symbol || !symbols || !symbols->ready)return symbol;
    size_t low=0,high=symbols->sorted.Count;
    while(low<high){size_t mid=low+(high-low)/2;const xls_lexical_symbol* row=xrtArrayConstGet(&symbols->sorted,mid);if(row->symbol<symbol)low=mid+1;else high=mid;}
    const xls_lexical_symbol* row=xrtArrayConstGet(&symbols->sorted,low);
    if(row && row->symbol==symbol){*local=1;return row->slot;}
    return symbol;
}
int xlsLexicalSymbolResolve(const xls_lexical_symbols* symbols,XL6_SymbolId key,int local,XL6_SymbolId* symbol)
{
    *symbol=key;if(local!=0 && local!=1)return 0;if(!local)return 1;
    if(!key || !symbols || !symbols->ready)return 0;
    const xls_lexical_symbol* row=xrtArrayConstGet(&symbols->order,(size_t)key-1);
    if(!row || row->slot!=key || !row->symbol)return 0;
    *symbol=row->symbol;return 1;
}
