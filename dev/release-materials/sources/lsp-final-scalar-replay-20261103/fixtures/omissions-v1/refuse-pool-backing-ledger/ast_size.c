#include "analysis/ast_size.h"
_Thread_local int xlsNativeTestAllocation;
static bool xlsNativeTestSet(xmap* map,xbytesview key,const void* value)
{xlsNativeTestAllocation=1;bool okay=xrtMapSet(map,key,value);xlsNativeTestAllocation=0;return okay;}

#include <string.h>
static int charge(xls_ast_size* size,size_t bytes)
{if(size->bytes>size->limit || bytes>size->limit-size->bytes){size->exceeded=1;return 0;}size->bytes+=bytes;return 1;}
int xlsAstSizeBegin(xls_ast_size* size,size_t limit,int backing)
{
    *size=(xls_ast_size){.limit=limit,.backing=backing};
    return xrtMapInit(&size->sources,sizeof(unsigned char)) && (!backing || xrtMapInit(&size->pools,sizeof(unsigned char)));
}
void xlsAstSizeEnd(xls_ast_size* size)
{xrtMapUnit(&size->sources);xrtMapUnit(&size->pools);}
int xlsAstSizeNode(xls_ast_size* size,const XL6_AstNode* node)
{
    if(!node || node->childCapacity<node->childCount || node->childCapacity>SIZE_MAX/sizeof(*node->children) ||
       (node->childCount && !node->children))return 0;
    unsigned char yes=1;
    if(size->backing && node->xlsStorage){
        const void* pool=node->xlsStorage;xbytesview key={(const unsigned char*)&pool,sizeof(pool)};
        if(!xrtMapHas(&size->pools,key)){
            uint64_t bytes=xlsAstStorageWork(node->xlsStorage).bytes;
            if(bytes>SIZE_MAX || !charge(size,(size_t)bytes) || !xlsNativeTestSet(&size->pools,key,&yes))return 0;
        }
    }else if(!charge(size,sizeof(*node)))return 0;
    if(!charge(size,node->childCapacity*sizeof(*node->children)))return 0;
    const char* strings[]={node->text,node->doc
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
        ,node->compiledPackageLink
#endif
    };
    for(size_t i=0;i<sizeof(strings)/sizeof(*strings);++i)
        if(strings[i] && !charge(size,strlen(strings[i])+1))return 0;
    if(node->literalData && (node->literalSize==SIZE_MAX || !charge(size,node->literalSize+1)))return 0;
    if(node->source){
        const XL6_AstSource* source=node->source;xbytesview key={(const unsigned char*)&source,sizeof(source)};
        if(!xrtMapHas(&size->sources,key)){
            if(!charge(size,sizeof(*source)) || (source->path && !charge(size,strlen(source->path)+1)) ||
               !xrtMapSet(&size->sources,key,&yes))return 0;
        }
    }
    ++size->nodes;return 1;
}
int xlsAstSizeTree(xls_ast_size* size,const XL6_AstNode* node,size_t depth,int (*cancelled)(void*),void* user)
{
    if(!node)return 1;
    if(depth>512 || size->nodes>=1048576u || ((size->nodes&63u)==0 && cancelled && cancelled(user)) || !xlsAstSizeNode(size,node))return 0;
    for(size_t i=0;i<node->childCount;++i)if(!xlsAstSizeTree(size,node->children[i],depth+1,cancelled,user))return 0;
    return xlsAstSizeTree(size,node->inferredType,depth+1,cancelled,user) && xlsAstSizeTree(size,node->semanticDeclaredType,depth+1,cancelled,user);
}
