#include "analysis/ast_storage.h"
#include <xrt.h>
#ifndef XLS_NATIVE_AST_STORAGE
#error Native AST storage requires the audited private header in every native translation unit
#endif
#ifndef XLS_NATIVE_AST_LEAF_REFS
#error Native AST leaf reference layout requires every consumer to be rebuilt
#endif
#define XLS_AST_PAGE_NODES 64u
struct xls_ast_storage { xpool pool;size_t budget,bytes;uint64_t nodes,pages;int pinned; };
xls_ast_storage* xlsAstStorageCreate(size_t budget)
{
    if(budget>64u*1024u*1024u)budget=64u*1024u*1024u;
    if(budget<sizeof(xls_ast_storage))return NULL;
    xls_ast_storage* storage=xrtCalloc(1,sizeof(*storage));if(!storage)return NULL;
    if(!xrtPoolInitLayout(&storage->pool,sizeof(XL6_AstNode),16,XLS_AST_PAGE_NODES)){xrtFree(storage);return NULL;}
    storage->budget=budget;storage->bytes=sizeof(*storage);storage->pinned=1;return storage;
}
XL6_AstNode* xlsAstStorageNode(xls_ast_storage* storage,size_t extraBudget)
{
    if(!storage || !storage->pinned)return NULL;
    size_t added=0;
    if(!storage->pool.Available){
        /* Audited XRT: node payload rounded to16,64slots/page,a separate page
         * record and geometric address index (initial8).64bytes conservatively
         * cover payload alignment slack. Count unused slots and index capacity,
         * not only live node bytes. This is not a process RSS/heap-header bound. */
        size_t stride=(sizeof(XL6_AstNode)+15u)&~(size_t)15u;
        added=XLS_AST_PAGE_NODES*stride+sizeof(xpoolpage)+64u;
        if(storage->pool.PageCount==storage->pool.IndexCapacity){
            size_t capacity=storage->pool.IndexCapacity ? storage->pool.IndexCapacity*2 : 8;
            if(capacity<storage->pool.IndexCapacity || capacity>SIZE_MAX/sizeof(xpoolpage*))return NULL;
            size_t indexBytes=(capacity-storage->pool.IndexCapacity)*sizeof(xpoolpage*);
            if(indexBytes>SIZE_MAX-added)return NULL;
            added+=indexBytes;
        }
    }
    if(added>extraBudget || added>storage->budget-storage->bytes)return NULL;
    XL6_AstNode* node=xrtPoolCalloc(&storage->pool);if(!node)return NULL;
    storage->bytes+=added;storage->pages+=added!=0;++storage->nodes;
    node->xlsStorage=storage;return node;
}
static void destroyEmpty(xls_ast_storage* storage)
{if(!storage->pinned && !storage->pool.LiveCount){xrtPoolUnit(&storage->pool);xrtFree(storage);}}
void xlsAstStorageUnpin(xls_ast_storage* storage)
{if(storage){storage->pinned=0;destroyEmpty(storage);}}
void xlsAstStorageFreeNode(XL6_AstNode* node)
{
    if(!node || !node->xlsStorage)return;
    xls_ast_storage* storage=node->xlsStorage;
    /* The original destructor already released every normally allocated field.
     * Do not access node after PoolFree:an empty page may be reclaimed. */
    if(xrtPoolFree(&storage->pool,node))destroyEmpty(storage);
}
xls_ast_pool_work xlsAstStorageWork(const xls_ast_storage* storage)
{return storage ? (xls_ast_pool_work){storage->nodes,storage->pages,storage->bytes} : (xls_ast_pool_work){0};}
