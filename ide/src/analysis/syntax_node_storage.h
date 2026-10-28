#ifndef XLS_SYNTAX_NODE_STORAGE_H
#define XLS_SYNTAX_NODE_STORAGE_H
#include "src/ast/xl6_ast.h"
#include <stdint.h>
#include <string.h>
#define XLS_SYNTAX_STORAGE_DEPTH 512u
/* Existing logical owner accounting,shared by the complete generic walk and
 * its mechanically derived source-binding traversal. No allocator/RSS claim. */
static inline int xlsSyntaxStorageAdd(size_t* total,size_t bytes,size_t limit)
{
    if(*total>limit || bytes>limit-*total)return 0;
    *total+=bytes;return 1;
}
static inline int xlsSyntaxNodeStorage(const XL6_AstNode* node,size_t depth,size_t* bytes,size_t limit)
{
    if(depth>XLS_SYNTAX_STORAGE_DEPTH || node->xlsStorage || node->childCount>node->childCapacity ||
       node->childCapacity>SIZE_MAX/sizeof(*node->children) ||
       !xlsSyntaxStorageAdd(bytes,sizeof(*node),limit) ||
       !xlsSyntaxStorageAdd(bytes,node->childCapacity*sizeof(*node->children),limit))return 0;
    return (!node->text || xlsSyntaxStorageAdd(bytes,strlen(node->text)+1,limit)) &&
        (!node->doc || xlsSyntaxStorageAdd(bytes,strlen(node->doc)+1,limit)) &&
        (!node->literalData || (node->literalSize!=SIZE_MAX && xlsSyntaxStorageAdd(bytes,node->literalSize+1,limit)));
}
/* Return the original source setter's success. bytes==0 means no complete
 * optional size proof;generic accounting remains authoritative/fallback. */
int xlsAstSetSourceTreeSized(XL6_AstNode* node,const char* path,size_t* bytes);
#endif
