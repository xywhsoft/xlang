/* Shared collection/selection/type rules keep their original order.
 * Legacy JSON keeps its 1536-byte per-record allowance. Native typed capture
 * charges actual arena capacity and reserves 64 bytes per node for the
 * canonical builder's minimum capacity; the original 6*length charges bound
 * its geometric growth. Segments have stable root-declaration ownership;
 * only the pointer directory moves when a segment grows. Neither storage
 * policy changes collected semantic facts. */
#include <limits.h>
#include "analysis/frontend_function_index.h"
#include "analysis/typed_module_internal.h"
typedef struct xls_index_storage xls_index_storage;
struct xls_function_index_asset {
    xatomic32 references;xls_index_storage* storage;XL6_IndexFact* facts;
    size_t first,count,capacity,canonicalBytes;xls_typed_function* function;
};
struct xls_index_storage {
    xatomic32 references;size_t slots,active,metadataBytes,sourceBytes,sourceCount;
    XL6_IndexFact** rows;xls_function_index_source* sources;
    xls_function_index_asset chunks[];
};
#define XLS_INDEX_STORAGE_BYTES (8u*1024u*1024u)
static _Thread_local xls_index_storage_control* xlsIndexStorageControl;
void xlsFrontendIndexStorageBegin(xls_index_storage_control* control,size_t bytes)
{if(control){*control=(xls_index_storage_control){xlsIndexStorageControl,bytes};xlsIndexStorageControl=control;}}
void xlsFrontendIndexStorageEnd(xls_index_storage_control* control)
{if(control && xlsIndexStorageControl==control)xlsIndexStorageControl=control->previous;}
static _Thread_local XL6_IndexBuild* xlsTypedIndexBuild;
static int xl6IndexBudget(XL6_IndexBuild*,size_t);
static void xlsIndexStorageParentRelease(xls_index_storage*);
static XL6_IndexFact* xlsIndexFactAt(const XL6_IndexBuild* build,size_t at)
{return build->storage ? build->storage->rows[at] : &build->facts[at];}
static xls_index_storage* xlsIndexStorageCreate(XL6_IndexBuild* build)
{
    size_t limit=xlsIndexStorageControl ? xlsIndexStorageControl->bytes : XLS_INDEX_STORAGE_BYTES;
    if(limit>XLS_INDEX_STORAGE_BYTES)limit=XLS_INDEX_STORAGE_BYTES;
    if(!build->root || build->root->kind!=XL6_AST_MODULE || build->root->childCount>=65537u ||
       (build->root->childCount && !build->root->children) || limit<sizeof(xls_index_storage))return NULL;
    int candidate=0;
    for(size_t i=0;i<build->root->childCount && !candidate;++i){const XL6_AstNode* child=build->root->children[i];
        candidate=child && (child->kind==XL6_AST_FUNC || child->kind==XL6_AST_ASYNC_FUNC || child->kind==XL6_AST_EXPORT_FUNC);
    }
    if(!candidate)return NULL;
    size_t slots=build->root->childCount+1;
    if(slots>(limit-sizeof(xls_index_storage))/sizeof(xls_function_index_asset))return NULL;
    size_t bytes=sizeof(xls_index_storage)+slots*sizeof(xls_function_index_asset);
    if(bytes>XL6_INDEX_BYTES-build->budget)return NULL;
    xls_index_storage* storage=xrtCalloc(1,bytes);if(!storage)return NULL;
    xrtAtomic32Init(&storage->references,1);storage->slots=slots;storage->metadataBytes=bytes;
    (void)xl6IndexBudget(build,bytes);return storage;
}
static int xlsIndexRecordBudget(XL6_IndexBuild* build)
{ return xl6IndexBudget(build,build==xlsTypedIndexBuild ? 64 : 1536); }
static size_t xlsIndexNodeLimit(const XL6_IndexBuild* build)
{ return build==xlsTypedIndexBuild ? UINT_MAX : XL6_INDEX_NODES; }
static size_t xlsIndexCapacity(XL6_IndexBuild* build,size_t capacity,size_t item,size_t initial)
{
    size_t available=(XL6_INDEX_BYTES-build->budget)/item;
    if(!available || capacity>=UINT_MAX) { build->error="semantic index exceeds byte limit"; return 0; }
    size_t increment=capacity ? capacity/2 : initial;
    if(!increment) increment=1;
    if(increment>available) increment=available;
    if(increment>UINT_MAX-capacity) increment=UINT_MAX-capacity;
    return capacity+increment;
}
static int xlsIndexReserveFact(XL6_IndexBuild* build)
{
    if(build!=xlsTypedIndexBuild || build->count<build->factsCapacity) return 1;
    if(build->storage){
        size_t capacity=xlsIndexCapacity(build,build->factsCapacity,sizeof(XL6_IndexFact*),256);if(!capacity)return 0;
        XL6_IndexFact** rows=xrtRealloc(build->storage->rows,capacity*sizeof(*rows));
        if(!rows){build->error="cannot grow semantic index directory";return 0;}
        size_t added=capacity-build->factsCapacity;build->storage->rows=rows;build->factsCapacity=capacity;
        return xl6IndexBudget(build,added*sizeof(*rows));
    }
    size_t capacity=xlsIndexCapacity(build,build->factsCapacity,sizeof(*build->facts),256);
    if(!capacity) return 0;
    XL6_IndexFact* facts=xrtRealloc(build->facts,capacity*sizeof(*facts));
    if(!facts) { build->error="cannot grow semantic index facts"; return 0; }
    size_t added=capacity-build->factsCapacity;
    memset(facts+build->factsCapacity,0,added*sizeof(*facts));
    build->facts=facts; build->factsCapacity=capacity;
    return xl6IndexBudget(build,added*sizeof(*facts));
}
static XL6_IndexFact* xlsIndexAppendFact(XL6_IndexBuild* build)
{
    xls_index_storage* storage=build->storage;
    if(!storage)return &build->facts[build->count++];
    if(storage->active>=storage->slots){build->error="semantic index declaration slot mismatch";return NULL;}
    xls_function_index_asset* chunk=&storage->chunks[storage->active];
    if(chunk->count==chunk->capacity){
        size_t capacity=xlsIndexCapacity(build,chunk->capacity,sizeof(XL6_IndexFact),16);if(!capacity)return NULL;
        XL6_IndexFact* facts=xrtRealloc(chunk->facts,capacity*sizeof(*facts));
        if(!facts){build->error="cannot grow semantic declaration facts";return NULL;}
        size_t added=capacity-chunk->capacity;memset(facts+chunk->capacity,0,added*sizeof(*facts));
        chunk->facts=facts;chunk->capacity=capacity;(void)xl6IndexBudget(build,added*sizeof(*facts));
        for(size_t i=0;i<chunk->count;++i)storage->rows[chunk->first+i]=&facts[i];
    }
    if(!chunk->storage){
        chunk->storage=storage;chunk->first=build->count;xrtAtomic32Init(&chunk->references,1);
        xrtAtomic32FetchAdd(&storage->references,1,XMEMORY_RELAXED);
    }
    XL6_IndexFact* result=&chunk->facts[chunk->count++];storage->rows[build->count++]=result;return result;
}
static int xlsIndexReserveSource(XL6_IndexBuild* build)
{
    if(build!=xlsTypedIndexBuild) return build->sourceCount<XL6_INDEX_NODES;
    if(build->sourceCount<build->sourcesCapacity) return 1;
    size_t capacity=xlsIndexCapacity(build,build->sourcesCapacity,sizeof(*build->sources),16);
    if(!capacity) return 0;
    XL6_IndexSource* sources=xrtRealloc(build->sources,capacity*sizeof(*sources));
    if(!sources) { build->error="cannot grow semantic index sources"; return 0; }
    size_t added=capacity-build->sourcesCapacity;
    memset(sources+build->sourcesCapacity,0,added*sizeof(*sources));
    build->sources=sources; build->sourcesCapacity=capacity;
    return xl6IndexBudget(build,added*sizeof(*sources));
}
